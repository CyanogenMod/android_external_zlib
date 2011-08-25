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
#  define SOULL (sizeof(unsigned long long))

#include <stdio.h>

/* ========================================================================= */
local inline void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    if (likely(wsize < (1<<16))) {
        unsigned int i, j, f;
        unsigned long long vwsize;
        unsigned long long m1, m2;

        vwsize = (wsize << 16) | (wsize  & 0x0000ffff);
        vwsize = (vwsize << 32) | vwsize;

        /* align */
        f = (unsigned)ALIGN_DIFF(p, SOULL);
        if (unlikely(f)) {
            f /= sizeof(*p);
            n -= f;
            if (f & 1) {
                register unsigned m = *p;
                *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
                f--;
            }
            if (f >= 2) {
                unsigned int m = *(unsigned int *)p;
                asm ("psub2.uuu %0 = %1, %2" : "=r" (m) : "r" (m), "r" (vwsize));
                *(unsigned int *)p = m;
                p += 2;
            }
        }

        /* do it */
        i  = n / (SOULL/sizeof(*p));
        n %= SOULL/sizeof(*p);
        if (unlikely(i & 1)) {
                m1 = *(unsigned long long *)p;
                asm ("psub2.uuu %0 = %1, %2" : "=r" (m1) : "r" (m1), "r" (vwsize));
                *(unsigned long long *)p = m1;
                p += SOULL/sizeof(*p);
                i--;
        }
        i /= 2;
        if (likely(j = i / 4)) {
            unsigned long long m3, m4, m5, m6, m7, m8;
            Posf *p1, *p2, *p3, *p4, *p5, *p6, *p7;
            i -= j * 4;
            asm (
                "mov.i ar.lc = %17\n"
                "1:\n\t"
                "ld8 %0 = [%8]\n\t"
                "ld8 %1 = [%9]\n\t"
                "nop.i 0x1\n\t"

                "ld8 %2 = [%10]\n\t"
                "ld8 %3 = [%11]\n\t"
                "nop.i 0x2\n\t"

                "ld8 %4 = [%12]\n\t"
                "ld8 %5 = [%13]\n\t"
                "nop.i 0x3\n\t"

                "ld8 %6 = [%14]\n\t"
                "ld8 %7 = [%15]\n\t"
                "nop.i 0x4;;\n\t"

                "psub2.uuu %0 = %0, %16\n\t"
                "psub2.uuu %1 = %1, %16\n\t"
                "psub2.uuu %2 = %2, %16\n\t"

                "psub2.uuu %3 = %3, %16\n\t"
                "psub2.uuu %4 = %4, %16\n\t"
                "psub2.uuu %5 = %5, %16\n\t"

                "psub2.uuu %6 = %6, %16\n\t"
                "psub2.uuu %7 = %7, %16\n\t"
                "nop.i 0x5\n\t;;"

                "st8 [%8] = %0, 64\n\t"
                "st8 [%9] = %1, 64\n\t"
                "nop.i 0x6\n\t"

                "st8 [%10] = %2, 64\n\t"
                "st8 [%11] = %3, 64\n\t"
                "nop.i 0x7\n\t"

                "st8 [%12] = %4, 64\n\t"
                "st8 [%13] = %5, 64\n\t"
                "nop.i 0x8\n\t"

                "st8 [%14] = %6, 64\n\t"
                "st8 [%15] = %7, 64\n\t"
                "br.cloop.sptk.few 1b;;"
                : /*  0 */ "=&r" (m1),
                  /*  1 */ "=&r" (m2),
                  /*  2 */ "=&r" (m3),
                  /*  3 */ "=&r" (m4),
                  /*  4 */ "=&r" (m5),
                  /*  5 */ "=&r" (m6),
                  /*  6 */ "=&r" (m7),
                  /*  7 */ "=&r" (m8),
                  /*  8 */ "=r" (p),
                  /*  9 */ "=r" (p1),
                  /* 10 */ "=r" (p2),
                  /* 11 */ "=r" (p3),
                  /* 12 */ "=r" (p4),
                  /* 13 */ "=r" (p5),
                  /* 14 */ "=r" (p6),
                  /* 15 */ "=r" (p7)
                : /* 16 */ "r" (vwsize),
                  /* 17 */ "r" (j-1),
                  /*   */  "8" (p),
                  /*   */  "9" (p+4),
                  /*   */ "10" (p+8),
                  /*   */ "11" (p+12),
                  /*   */ "12" (p+16),
                  /*   */ "13" (p+20),
                  /*   */ "14" (p+24),
                  /*   */ "15" (p+28)
                : "ar.lc"
            );
        }
        if (unlikely(i)) {
            Posf *p1;
            asm (
                "mov.i ar.lc = %5\n"
                "1:\n\t"
                "ld8 %0 = [%2]\n\t"
                "ld8 %1 = [%3]\n\t"
                "nop.i 0x1;;\n\t"

                "psub2.uuu %0 = %0, %4\n\t"
                "psub2.uuu %1 = %1, %4\n\t"
                "nop.i 0x2;;\n\t"

                "st8 [%2] = %0, 16\n\t"
                "st8 [%3] = %1, 16\n\t"
                "br.cloop.sptk.few 1b;;"
                : /* 0 */ "=&r" (m1),
                  /* 1 */ "=&r" (m2),
                  /* 2 */ "=r" (p),
                  /* 3 */ "=r" (p1)
                : /* 4 */ "r" (vwsize),
                  /* 5 */ "r" (i-1),
                  /*  */ "2" (p),
                  /*  */ "3" (p+8)
                : "ar.lc"
            );
        }

        /* handle trailer */
        if (unlikely(n)) {
            if (n >= 2) {
                unsigned int m = *(unsigned int *)p;
                asm ("psub2.uuu %0 = %1, %2" : "=r" (m) : "r" (m), "r" (vwsize));
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
#endif
