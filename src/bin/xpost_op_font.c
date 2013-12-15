/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Xpost software product nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#elif !defined alloca
# ifdef __GNUC__
#  define alloca __builtin_alloca
# elif defined _AIX
#  define alloca __alloca
# elif defined _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# elif !defined HAVE_ALLOCA
#  ifdef  __cplusplus
extern "C"
#  endif
void *alloca (size_t);
# endif
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#ifdef HAVE_FREETYPE
# include <ft2build.h>
# include FT_FREETYPE_H
#endif

#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_font.h"

#include "xpost_save.h"
#include "xpost_context.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_op_font.h"

typedef struct fontdata
{
    FT_Face face;
} fontdata;

static
int _findfont (Xpost_Context *ctx,
               Xpost_Object fontname)
{
#ifdef HAVE_FREETYPE
    Xpost_Object fontstr;
    Xpost_Object fontdict;
    Xpost_Object privatestr;
    struct fontdata data;
    char *fname;

    if (xpost_object_get_type(fontname) == nametype)
        fontstr = strname(ctx, fontname);
    else
        fontstr = fontname;
    fname = alloca(fontstr.comp_.sz + 1);
    memcpy(fname, charstr(ctx, fontstr), fontstr.comp_.sz);
    fname[fontstr.comp_.sz] = '\0';

    fontdict = consbdc(ctx, 10);
    privatestr = consbst(ctx, sizeof data, NULL);
    bdcput(ctx, fontdict, consname(ctx, "Private"), privatestr);

    /* initialize font data, with x-scale and y-scale set to 1 */
    data.face = (FT_Face)xpost_font_face_new_from_name(fname);

    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    xpost_stack_push(ctx->lo, ctx->os, fontdict);
    return 0;
#else
    return invalidfont;
#endif
}


static
int _scalefont (Xpost_Context *ctx,
                Xpost_Object fontdict,
                Xpost_Object size)
{
    Xpost_Object privatestr;
    struct fontdata data;

    privatestr = bdcget(ctx, fontdict, consname(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);

    /* scale x and y sizes by @p size */
    xpost_font_face_scale(data.face, size.real_.val);

    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    xpost_stack_push(ctx->lo, ctx->os, fontdict);
    return 0;
}


static
int _setfont (Xpost_Context *ctx,
              Xpost_Object fontdict)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2); 
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = bdcget(ctx, userdict, consname(ctx, "graphicsdict"));
    gs = bdcget(ctx, gd, consname(ctx, "currgstate"));

    bdcput(ctx, gs, consname(ctx, "currfont"), fontdict);

    return 0;
}

static
void _draw_bitmap (Xpost_Context *ctx,
              Xpost_Object devdic,
              Xpost_Object putpix,
              FT_Bitmap *bitmap,
              int xpos,
              int ypos,
              int ncomp,
              Xpost_Object comp1,
              Xpost_Object comp2,
              Xpost_Object comp3)
{
    int i, j;
    unsigned char *tmp;

    tmp = bitmap->buffer;
    XPOST_LOG_INFO("bitmap->rows = %d, bitmap->width = %d", bitmap->rows, bitmap->width);

    for (i = 0; i < bitmap->rows; i++)
    {
        for (j = 0; j < bitmap->width; j++)
        {
            if (*tmp) {

                switch(ncomp) {
                case 1:
                    xpost_stack_push(ctx->lo, ctx->os, comp1);
                    break;
                case 3:
                    xpost_stack_push(ctx->lo, ctx->os, comp1);
                    xpost_stack_push(ctx->lo, ctx->os, comp2);
                    xpost_stack_push(ctx->lo, ctx->os, comp3);
                    break;
                }
                xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(xpos+j));
                xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(ypos+i));
                xpost_stack_push(ctx->lo, ctx->os, devdic);
                if (xpost_object_get_type(putpix) == operatortype)
                    xpost_stack_push(ctx->lo, ctx->es, putpix);
                else {
                    xpost_stack_push(ctx->lo, ctx->os, putpix);
                    xpost_stack_push(ctx->lo, ctx->es,
                            consname(ctx, "exec"));
                }
            }
            ++tmp;
        }
    }
}

static
int _show (Xpost_Context *ctx,
           Xpost_Object str)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;
    Xpost_Object fontdict;
    Xpost_Object privatestr;
    struct fontdata data;
    char *cstr;
    Xpost_Object path;
    Xpost_Object subpath;
    Xpost_Object pathelem;
    Xpost_Object pathelemdata;
    Xpost_Object datax, datay;
    real xpos, ypos;
    char *ch;
    Xpost_Object devdic;
    Xpost_Object putpix;
    Xpost_Object colorspace;
    int ncomp;
    Xpost_Object comp1, comp2, comp3;
    Xpost_Object finalize;

    /* load the graphicsdict, current graphics state, and current font */
    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2); 
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = bdcget(ctx, userdict, consname(ctx, "graphicsdict"));
    gs = bdcget(ctx, gd, consname(ctx, "currgstate"));
    fontdict = bdcget(ctx, gs, consname(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* load the device */
    devdic = bdcget(ctx, userdict, consname(ctx, "DEVICE"));
    putpix = bdcget(ctx, devdic, consname(ctx, "PutPix"));
    XPOST_LOG_INFO("loaded DEVICE and PutPix");

    /* get the font data from the font dict */
    privatestr = bdcget(ctx, fontdict, consname(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == invalidtype)
        return invalidfont;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = alloca(str.comp_.sz + 1);
    memcpy(cstr, charstr(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    /* get the current pen position */
    /*FIXME if any of these calls fail, should return nocurrentpoint; */
    path = bdcget(ctx, gs, consname(ctx, "currpath")); 
    subpath = bdcget(ctx, path, xpost_cons_int(
                diclength(xpost_context_select_memory(ctx,path), path) - 1));
    pathelem = bdcget(ctx, subpath, xpost_cons_int(
                diclength(xpost_context_select_memory(ctx,subpath), subpath) - 1));
    pathelemdata = bdcget(ctx, pathelem, consname(ctx, "data"));
    datax = barget(ctx, pathelemdata, pathelemdata.comp_.sz - 2);
    datay = barget(ctx, pathelemdata, pathelemdata.comp_.sz - 1);
    if (xpost_object_get_type(datax) == integertype)
        datax = xpost_cons_real(datax.int_.val);
    if (xpost_object_get_type(datay) == integertype)
        datay = xpost_cons_real(datay.int_.val);
    xpos = datax.real_.val;
    ypos = datay.real_.val;
    XPOST_LOG_INFO("currentpoint: %f %f", xpos, ypos);

    colorspace = bdcget(ctx, devdic, consname(ctx, "nativecolorspace"));
    if (objcmp(ctx, colorspace, consname(ctx, "DeviceGray")) == 0)
    {
        ncomp = 1;
        comp1 = bdcget(ctx, gs, consname(ctx, "colorcomp1"));
    }
    else if (objcmp(ctx, colorspace, consname(ctx, "DeviceRGB")) == 0)
    {
        ncomp = 3;
        comp1 = bdcget(ctx, gs, consname(ctx, "colorcomp1"));
        comp2 = bdcget(ctx, gs, consname(ctx, "colorcomp2"));
        comp3 = bdcget(ctx, gs, consname(ctx, "colorcomp3"));
    } else {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    XPOST_LOG_INFO("ncomp = %d", ncomp);

    finalize = xpost_object_cvx(consbar(ctx, 5));
    /* fill-in final pos before return */
    barput(ctx, finalize, 0, xpost_cons_real(xpos));
    barput(ctx, finalize, 1, xpost_cons_real(ypos));
    barput(ctx, finalize, 2, xpost_object_cvx(consname(ctx, "itransform")));
    barput(ctx, finalize, 3, xpost_object_cvx(consname(ctx, "moveto")));
    barput(ctx, finalize, 4, xpost_object_cvx(consname(ctx, "flushpage")));
    xpost_stack_push(ctx->lo, ctx->es, finalize);

    /* render text in char *cstr  with font data  at pen position xpos ypos */
#ifdef HAVE_FREETYPE
    for (ch = cstr; *ch; ch++) {
        FT_UInt glyph_index;
        FT_Error err;

        glyph_index = FT_Get_Char_Index(data.face, *ch);
        err = FT_Load_Glyph(data.face, glyph_index, FT_LOAD_DEFAULT);
        if (err)
        {
            XPOST_LOG_ERR("Can not load glyph (error : %d)", err);
            continue;
        }
        if (data.face->glyph->format != FT_GLYPH_FORMAT_BITMAP)
        {
            err = FT_Render_Glyph(data.face->glyph, FT_RENDER_MODE_NORMAL);
        }
        _draw_bitmap(ctx, devdic, putpix,
                &data.face->glyph->bitmap,
                xpos + data.face->glyph->bitmap_left,
                ypos - data.face->glyph->bitmap_top,
                ncomp, comp1, comp2, comp3);
        xpos += data.face->glyph->advance.x >> 6;
        ypos += data.face->glyph->advance.y >> 6;
    }
#endif

    /* update current position in the graphics state */
    barput(ctx, finalize, 0, xpost_cons_real(xpos));
    barput(ctx, finalize, 1, xpost_cons_real(ypos));

    return 0;
}


int initopfont (Xpost_Context *ctx,
                Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = consoper(ctx, "findfont", _findfont, 1, 1, nametype); INSTALL;
    op = consoper(ctx, "findfont", _findfont, 1, 1, stringtype); INSTALL;
    op = consoper(ctx, "scalefont", _scalefont, 1, 2, dicttype, floattype); INSTALL;
    //op = consoper(ctx, "makefont", _makefont, 1, 2, dicttype, arraytype); INSTALL;
    op = consoper(ctx, "setfont", _setfont, 1, 1, dicttype); INSTALL;

    op = consoper(ctx, "show", _show, 0, 1, stringtype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

    return 0;
}


