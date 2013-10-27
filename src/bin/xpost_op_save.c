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

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_save.h"
#include "xpost_interpreter.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_op_save.h"

static
void Zsave (context *ctx)
{
    push(ctx->lo, ctx->os, save(ctx->lo));
}

static
void Vrestore (context *ctx,
               Xpost_Object V)
{
    int z = count(ctx->lo, adrent(ctx->lo, VS));
    while(z > V.save_.lev) {
        restore(ctx->lo);
        z--;
    }
}

static
void Bsetglobal (context *ctx,
                 Xpost_Object B)
{
    ctx->vmmode = B.int_.val? GLOBAL: LOCAL;
}

static
void Zcurrentglobal (context *ctx)
{
    push(ctx->lo, ctx->os, xpost_cons_bool(ctx->vmmode==GLOBAL));
}

static
void Agcheck (context *ctx,
              Xpost_Object A)
{
    Xpost_Object r;
    switch(xpost_object_get_type(A)) {
    default:
            r = xpost_cons_bool(false); break;
    case stringtype:
    case nametype:
    case dicttype:
    case arraytype:
            r = xpost_cons_bool((A.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK)!=0);
    }
    push(ctx->lo, ctx->os, r);
}

static
void Zvmstatus (context *ctx)
{
    push(ctx->lo, ctx->os, xpost_cons_int(count(ctx->lo, adrent(ctx->lo, VS))));
    push(ctx->lo, ctx->os, xpost_cons_int(ctx->lo->used));
    push(ctx->lo, ctx->os, xpost_cons_int(ctx->lo->max));
}

void initopv(context *ctx,
             Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

    op = consoper(ctx, "save", Zsave, 1, 0); INSTALL;
    op = consoper(ctx, "restore", Vrestore, 0, 1, savetype); INSTALL;
    op = consoper(ctx, "setglobal", Bsetglobal, 0, 1, booleantype); INSTALL;
    op = consoper(ctx, "currentglobal", Zcurrentglobal, 1, 0); INSTALL;
    op = consoper(ctx, "gcheck", Agcheck, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "vmstatus", Zvmstatus, 3, 0); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


