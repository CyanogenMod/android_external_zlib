/* slhash.c -- slide the hash table during fill_window()
 * Copyright (C) 1995-2010 Jean-loup Gailly and Mark Adler
 * Copyright (C) 2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* NOTE:
 * We do not precheck the length or wsize for small values because
 * we assume a minimum len of 256 (for MEM_LEVEL 1) and a minimum wsize
 * of 256 for windowBits 8
 */

/* we use inline asm, so GCC it is */
#ifdef __GNUC__
#  define HAVE_SLHASH_VEC
#  define SOUL (sizeof(unsigned long))

#  if GCC_VERSION_GE(303)
#    define cmpbge  __builtin_alpha_cmpbge
#    define zapnot  __builtin_alpha_zapnot
#  else
/* ========================================================================= */
local inline unsigned long cmpbge(unsigned long a, unsigned long b)
{
    unsigned long r;
    asm ("cmpbge	%r1, %2, %0" : "=r" (r) : "rJ" (a), "rI" (b));
    return r;
}

/* ========================================================================= */
local inline unsigned long zapnot(unsigned long a, unsigned long mask)
{
    unsigned long r;
    asm ("zapnot	%r1, %2, %0" : "=r" (r) : "rJ" (a), "rI" (mask));
    return r;
}
#  endif

/* ========================================================================= */
/*
 * We do not have saturation, but funky multi compare/mask
 * instructions, since the first alpha, for character handling.
 */
local inline unsigned long psubus(unsigned long a, unsigned long b)
{
    unsigned long m = cmpbge(a, b);
    m  = ((m & 0xaa) >> 1) & m;
    m |= m << 1;
    a  = zapnot(a, m);
    return a - zapnot(b, m);
}

/* ========================================================================= */
local inline void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    if (likely(wsize < (1<<16))) {
        unsigned long vwsize;
        unsigned int i, f;

        vwsize = (wsize  << 16) | (wsize  & 0x0000ffff);
        vwsize = (vwsize << 32) | (vwsize & 0xffffffff);
        /* align */
        f = (unsigned)ALIGN_DIFF(p, SOUL);
        if (unlikely(f)) {
            f /= sizeof(*p);
            n -= f;
            if (unlikely(f & 1)) {
                register unsigned m = *p;
                *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
                f--;
            }
            if (f >= 2) {
                unsigned long x = psubus(*(unsigned int *)p, vwsize);
                *(unsigned int *)p = (unsigned int)x;
                p += 2;
                f -= 2;
            }
        }

        /* do it */
        i  = n / (SOUL/sizeof(*p));
        n %= SOUL/sizeof(*p);
        if (unlikely(i & 1)) {
            unsigned long x = psubus(*(unsigned long *)p, vwsize);
            *(unsigned long *)p = x;
            p += SOUL/sizeof(*p);
            i--;
        }
        i /= 2;
        do {
            unsigned long x1, x2;
            x1 = ((unsigned long *)p)[0];
            x2 = ((unsigned long *)p)[1];
            x1 = psubus(x1, vwsize);
            x2 = psubus(x2, vwsize);
            ((unsigned long *)p)[0] = x1;
            ((unsigned long *)p)[1] = x2;
            p += 2*(SOUL/sizeof(*p));
        } while (--i);

        /* handle trailer */
        if (unlikely(n)) {
            if (n >= 2) {
                unsigned long x = psubus(*(unsigned int *)p, vwsize);
                *(unsigned int *)p = (unsigned int)x;
                p += 2;
                n -= 2;
            }
            if (unlikely(n)) {
                register unsigned m = *p;
                *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
            }
        }

    } else {
        do {
            register unsigned m = *p;
            *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
        } while (--n);
    }
}
#endif
