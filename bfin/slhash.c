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
#  define SO32 (sizeof(unsigned int))

/* ========================================================================= */
local inline void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    /*
     * Cycles mesured with the cycle counter, both hashes, 32k:
     * orig:   1983162
     * new:     721636
     * So this code is 2.7 times faster. If only the memory
     * subsystem would be faster, then there would be more
     * speedup possible.
     * But i don't see it...
     */
    if (likely(wsize <= (1<<15))) {
        unsigned int vwsize, i, t, u, v;
        Posf *p1;

        /*
         * We have saturation, but not unsigned...
         * So we do dirty tricks.
         * We subtract 32k from the unsigned value, then add the
         * negated wsize with singed saturation, then we add 32k
         * again, unsigend.
         */
        wsize = (unsigned short)((~wsize)+1);
        vwsize = wsize | (wsize << 16);

        asm (
            "CC = BITTST(%0, 1);\n\t"
#  ifdef __ELF__
            "IF CC JUMP 3f;\n\t"
            ".subsection 2\n"
            "3:\t"
#  else
            "IF !CC JUMP 4f (BP);\n\t"
#  endif
            "%0 = W[%[i]] (Z);\n\t"
            "%h0 = %h0 - %h[off];\n\t"
            "%h0 = %h0 + %h[vwsize] (S);\n\t"
            "%h0 = %h0 + %h[off];\n\t"
            "W[%[i]++] = %0;\n\t"
            "%[n] += -1;\n\t"
#  ifdef __ELF__
            "JUMP 4f\n\t"
            ".previous\n"
#  endif
            "4:\n\t"
            "%0 = %[n];\n\t"
            "%[p1] = %[i];\n\t"
            "CC = %[n] == 0;\n\t"
            "%0 >>= 1;\n\t"
            "%[i] += 4;\n\t"
            "%0 += -2;\n\t"
            "%[p0] = %[i];\n\t"
            "%0 = ROT %0 by -1;\n\t"
            "%1 = [%[p1]];\n\t"
            "%[i] = %0;\n\t"
            "%0 = [%[p0]];\n\t"
            "%1 = %1 -|- %[off];\n\t"
            "%1 = %1 +|+ %[vwsize] (S);\n\t"
            "%1 = %1 +|+ %[off];\n\t"
            "LSETUP (1f, 2f) LC1 = %[i];\n"
            /*=========================*/
            "1:\t"
            "%0 = %0 -|- %[off] || [%[p1]++%[m8]] = %1;\n\t"
            "%0 = %0 +|+ %[vwsize] (S) || %1 = [%[p1]];\n\t"
            "%0 = %0 +|+ %[off];\n\t"
            "%1 = %1 -|- %[off] || [%[p0]++%[m8]] = %0;\n\t"
            "%1 = %1 +|+ %[vwsize] (S) || %0 = [%[p0]];\n\t"
            "2:\t"
            "%1 = %1 +|+ %[off];\n\t"
            /*=========================*/
            "%0 = %0 -|- %[off] || [%[p1]] = %1;\n\t"
            "%0 = %0 +|+ %[vwsize] (S);\n\t"
            "%0 = %0 +|+ %[off];\n\t"
            "[%[p0]++] = %0;\n\t"
#  ifdef __ELF__
            "IF CC JUMP 5f;\n\t"
            ".subsection 2\n"
            "5:\t"
#  else
            "IF !CC JUMP 6f (BP);\n\t"
#  endif
            "%0 = [%[p0]];\n\t"
            "%0 = %0 -|- %[off];\n\t"
            "%0 = %0 +|+ %[vwsize] (S);\n\t"
            "%0 = %0 +|+ %[off];\n"
            "[%[p0]++] = %0;\n"
#  ifdef __ELF__
            "JUMP 6f\n\t"
            ".previous\n"
#  endif
            "6:\n\t"
            "%0 = %[n];\n\t"
            "CC = BITTST(%0, 0);\n\t"
#  ifdef __ELF__
            "IF CC JUMP 7f;\n\t"
            ".subsection 2\n"
            "7:\t"
#  else
            "IF !CC JUMP 8f (BP);\n\t"
#  endif
            "%[n] = %[p0];\n\t"
            "%0 = W[%[n]] (Z);\n\t"
            "%h0 = %h0 - %h[off];\n\t"
            "%h0 = %h0 + %h[vwsize] (S);\n\t"
            "%h0 = %h0 + %h[off];\n\t"
            "W[%[n]++] = %0;\n\t"
            "%[p0] = %[n]\n\t"
#  ifdef __ELF__
            "JUMP 8f\n\t"
            ".previous\n"
#  endif
            "8:"
            : /*  0 */ "=&d" (t),
              /*  1 */ "=&d" (u),
              /*  2 */ [p0] "=b" (p),
              /*  3 */ [p1] "=b" (p1),
              /*  4 */ [n] "=a" (v),
              /*  5 */ [i] "=a" (i),
              /*  6 */ "=m" (*p)
            : /*  7 */ [off] "d" (0x80008000),
              /*  8 */ [vwsize] "d" (vwsize),
              /*  9 */ [m8] "f" (2*SO32),
              /*  */ "0" (p),
              /*  */ "4" (n),
              /*  */ "5" (p),
              /*  */ "m" (*p)
            : "cc"
        );
    } else {
        register unsigned m;
        /* GCC generates ugly code, so do it by hand */
        asm (
            "LSETUP (1f, 2f) LC1 = %5;\n"
            "1:\t"
            "%1 = W [%0] (Z);\n\t"
            "%1 = %1 - %3;\n\t"
            "CC = AC0_COPY;\n\t"
            "IF !CC %1 = %4;\n"
            "2:\t"
            "W [%0++] = %1;\n\t"
            : /* 0 */ "=a" (p),
              /* 1 */ "=&d" (m),
              /* 2 */ "=m" (*p)
            : /* 3 */ "d" (wsize),
              /* 3 */ "d" (0),
              /* 5 */ "a" (n),
              /*  */ "0" (p),
              /*  */ "m" (*p)
            : "cc"
        );
    }
}
#endif
