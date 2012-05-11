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

/* we use a bunch of inline asm, so GCC it is */
#ifdef __GNUC__
#  define HAVE_SLHASH_VEC
#  define SOUL (sizeof(unsigned long))

/* ========================================================================= */
local inline unsigned long v2sub(unsigned long a, unsigned long b)
{
    unsigned long r;
#  ifdef __tilegx__
    r = __insn_v2sub(a, b);
#  else
    r = __insn_subh(a, b);
#  endif
    return r;
}

/* ========================================================================= */
local inline unsigned long v2cmpltu(unsigned long a, unsigned long b)
{
    unsigned long r;
#  ifdef __tilegx__
    r =  __insn_v2cmpltu(a, b);
#  else
    r = __insn_slth_u(a, b);
#  endif
    return r;
}

/* ========================================================================= */
local inline unsigned long v2mz(unsigned long a, unsigned long b)
{
    unsigned long r;
#  ifdef __tilegx__
    r = __insn_v2mz(a, b);
#  else
    r = __insn_mzh(a, b);
#  endif
    return r;
}

/* ========================================================================= */
local void update_hoffset_m(Posf *p, uInt wsize, unsigned n)
{
    unsigned long vwsize;
    unsigned int i, f;

    vwsize = (unsigned short)wsize;
    vwsize = (vwsize << 16) | (vwsize & 0x0000ffff);
    if (SOUL > 4)
        vwsize = ((vwsize << 16) << 16) | (vwsize & 0xffffffff);

    /* align */
    f = (unsigned)ALIGN_DIFF(p, SOUL);
    if (unlikely(f)) {
        f /= sizeof(*p);
        n -= f;
        if (f & 1) {
            register unsigned m = *p;
            *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
            f--;
        }
        if (SOUL > 4 && f >= 2) {
            unsigned int m = *(unsigned int *)p;
            m = v2mz(v2cmpltu(m, vwsize), v2sub(m, vwsize));
            *(unsigned int *)p = m;
            p += 2;
        }
    }

    /* do it */
    i  = n / (SOUL/sizeof(*p));
    n %= SOUL/sizeof(*p);
    if (i & 1) {
            unsigned long m = *(unsigned long *)p;
            m = v2mz(v2cmpltu(m, vwsize), v2sub(m, vwsize));
            *(unsigned long *)p = m;
            p += SOUL/sizeof(*p);
            i--;
    }
    i /= 2;
    do {
            unsigned long m1 = ((unsigned long *)p)[0];
            unsigned long m2 = ((unsigned long *)p)[1];
            unsigned long mask1, mask2;
            mask1 = v2sub(m1, vwsize);
            m1    = v2cmpltu(m1, vwsize);
            mask2 = v2sub(m2, vwsize);
            m2    = v2cmpltu(m2, vwsize);
            m1    = v2mz(m1, mask1);
            m2    = v2mz(m2, mask2);
            ((unsigned long *)p)[0] = m1;
            ((unsigned long *)p)[1] = m2;
            p += 2*(SOUL/sizeof(*p));
    } while (--i);

    /* handle trailer */
    if (unlikely(n)) {
        if (SOUL > 4 && n >= 2) {
            unsigned int m = *(unsigned int *)p;
            m = v2mz(v2cmpltu(m, vwsize), v2sub(m, vwsize));
            *(unsigned int *)p = m;
            p += 2;
            n -= 2;
        }
        if (n & 1) {
            register unsigned m = *p;
            *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
        }
    }
}

/* ========================================================================= */
local void update_hoffset_l(Posf *p, uInt wsize, unsigned n)
{
    do {
        register unsigned m = *p;
        *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
    } while (--n);
}

/* ========================================================================= */
local inline void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    /*
     * Unfortunatly most chip designer prefer signed saturation...
     * So we are better off with a parralel compare and move/mask
     */
    if (likely(2 == sizeof(*p) && wsize < (1<<16)))
        update_hoffset_m(p, wsize, n);
    else
        update_hoffset_l(p, wsize, n);
}
#endif
