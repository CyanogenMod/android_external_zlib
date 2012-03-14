/* slhash.c -- slide the hash table during fill_window()
 * Copyright (C) 1995-2010 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#include "deflate.h"
#define NIL 0

#ifndef NO_SLHASH_VEC
#  if defined(__arm__)
#    include "arm/slhash.c"
#  elif defined(__alpha__)
#    include "alpha/slhash.c"
#  elif defined(__bfin__)
#    include "bfin/slhash.c"
#  elif defined(__ia64__)
#    include "ia64/slhash.c"
#  elif defined(__mips__)
#    include "mips/slhash.c"
#  elif defined(__hppa__) || defined(__hppa64__)
#    include "parisc/slhash.c"
#  elif defined(__powerpc__) || defined(__powerpc64__)
#    include "ppc/slhash.c"
#  elif defined(__tile__)
#    include "tile/slhash.c"
#  elif defined(__i386__) || defined(__x86_64__)
#    include "x86/slhash.c"
#  endif
#endif

#ifndef HAVE_SLHASH_COMPLETE
#  ifndef HAVE_SLHASH_VEC
local void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    register unsigned m;
    do {
        m = *p;
        *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
    } while (--n);
}
#  endif

void ZLIB_INTERNAL _sh_slide (p, q, wsize, n)
    Posf *p;
    Posf *q;
    uInt wsize;
    unsigned n;
{
    update_hoffset(p, wsize, n);
#  ifndef FASTEST
    /* If n is not on any hash chain, prev[n] is garbage but
     * its value will never be used.
     */
    update_hoffset(q, wsize, wsize);
#  endif
}
#endif
