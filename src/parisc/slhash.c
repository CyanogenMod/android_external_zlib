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
/*
 * the 7100LC && 7300LC are parisc 1.1, but have MAX-1, which already has the
 * hadd, but there is no preprocessor define to detect them.
 */
#  if defined(_PA_RISC2_0) || defined(HAVE_PA7x00LC)
#    define HAVE_SLHASH_VEC
#    define SOST (sizeof(size_t))

/* ========================================================================= */
local inline void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    if (likely(wsize <= (1<<15))) {
        unsigned int i, f;
        size_t vwsize;
        size_t m1, m2;

        /*
         * We have unsigned saturation, but one operand to the
         * instruction is always signed m(
         * So we add the negated wsize...
         */
        vwsize = (unsigned short)((~wsize)+1);
        vwsize = (vwsize  << 16) | (vwsize  & 0x0000ffff);
        /*
         * sigh...
         * PARISC 2.0 is _always_ 64 bit. So the register are 64Bit
         * and the instructions work on 64Bit, ldd/std works, even
         * in a 32Bit executable.
         * The sad part: Unfortunatly 32Bit processes do not seem
         * to get their upper register half saved on context switch.
         * So restrict the 64Bit-at-once goodness to hppa64.
         * Without this braindamage, this could bring a nice 16.5%
         * speedup even to 32Bit processes.
         * Now it's just 12.7% for 32 Bit processes.
         */
        if (SOST > 4) /* silence warning about shift larger then type size */
            vwsize = ((vwsize << 16) << 16) | (vwsize  & 0xffffffff);

        /* align */
        f = (unsigned)ALIGN_DIFF(p, SOST);
        if (unlikely(f)) {
            f /= sizeof(*p);
            n -= f;
            if (f & 1) {
                register unsigned m = *p;
                *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
                f--;
            }
            if (SOST > 4 && f >= 2) {
                unsigned int m = *(unsigned int *)p;
                asm ("hadd,us %1, %2, %0" : "=r" (m) : "r" (m), "r" (vwsize));
                *(unsigned int *)p = m;
                p += 2;
            }
        }

        /* do it */
        i  = n / (SOST/sizeof(*p));
        n %= SOST/sizeof(*p);
        if (i & 1) {
                m1 = *(size_t *)p;
                asm ("hadd,us %1, %2, %0" : "=r" (m1) : "r" (m1), "r" (vwsize));
                *(size_t *)p = m1;
                p += SOST/sizeof(*p);
                i--;
        }
        i /= 2;
        do {
                m1 = ((size_t *)p)[0];
                m2 = ((size_t *)p)[1];
                asm ("hadd,us %1, %2, %0" : "=r" (m1) : "r" (m1), "r" (vwsize));
                asm ("hadd,us %1, %2, %0" : "=r" (m2) : "r" (m2), "r" (vwsize));
                ((size_t *)p)[0] = m1;
                ((size_t *)p)[1] = m2;
                p += 2*(SOST/sizeof(*p));
        } while (--i);

        /* handle trailer */
        if (unlikely(n)) {
            if (SOST > 4 && n >= 2) {
                unsigned int m = *(unsigned int *)p;
                asm ("hadd,us %1, %2, %0" : "=r" (m) : "r" (m), "r" (vwsize));
                *(unsigned int *)p = m;
                p += 2;
                n -= 2;
            }
            if (n & 1) {
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
#  endif
#endif
