/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   blackfin implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/* we use inline asm, so GCC it is */
#ifdef __GNUC__
#  define HAVE_ADLER32_VEC
#  define MIN_WORK 16
#  define VNMAX (2*NMAX + ((9*NMAX)/10))

/* ========================================================================= */
local noinline uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    struct vord { unsigned long d[4]; };
    static const struct vord vord_e = {{0x00080001, 0x00060001, 0x00040001, 0x00020001}};
    static const struct vord vord_o = {{0x00070001, 0x00050001, 0x00030001, 0x00010001}};
    unsigned long s1, s2, s1s;
    unsigned long lt0, lt1;
    unsigned long fo, fe, fex;
    unsigned long lcount, pt;

    /* split Adler-32 into component sums */
    s1 = adler & 0xffff;
    s2 = (adler >> 16) & 0xffff;

    __asm__ __volatile__ (
        /* setup circular adressing */
        "I0 = %[lt1];\n\t"
        "B0 = %[lt1];\n\t"
        "%[pt] = %[len];\n\t"
        "I2 = %[vord_e];\n\t"
        "B2 = I2;\n\t"
        "%[pt] += 4\n\t"
        "L2 = %[sorder];\n\t"
        "I3 = %[vord_o];\n\t"
        "L0 = %[pt];\n\t"
        "B3 = I3;\n\t"
        "L3 = L2;\n\t"
        /* fetch start data */
        "DISALGNEXCPT || %[lt0] = [I0 ++ %[m_4]];\n\t"
        "%[fe] = [I2 ++ %[m_4]];\n\t"
        "%[fo] = [I3 ++ %[m_4]];\n"
        /****************************/
        "1:\n\t"
        /* put sums into ACC */
        "A1 = %0;\n\t"
        "A0 = %1;\n\t"
        /* prepare len and hw loop counter */
        "%[lcount] = %[vnmax] (Z);\n\t"
        "%[pt] = %[len];\n\t"
        "%[pt] += -1;\n\t"
        "CC = %[lcount] <= %[pt] (IU);\n\t"
        "IF !CC %[lcount] = %[pt];\n"
        "%[lcount] = %[lcount] >> 2;\n\t"
        "%[lcount] = %[lcount] >> 1;\n\t"
        "%[pt] = %[lcount] << 2;\n\t"
        "%[pt] = %[pt] << 1;\n\t"
        "%[len] -= %[pt];\n\t"
        "%[pt] = 8;\n\t"
        "%2 = 0;\n\t" /* s1s = 0 */
        "LOOP inner_add LC1 = %[lcount];\n\t"
        /*=========================*/
        "LOOP_BEGIN inner_add;\n\t"
        "%2 = %2 + %0;\n\t"
        "DISALGNEXCPT || %[lt1] = [I0 ++ %[m_4]];\n\t"
        "(%0, %1) = BYTEUNPACK r1:0;\n\t"
        "A1 += %h1 * %h[fe], A0 += %h1 * %d[fe] (FU) || %[fex] = [I2 ++ %[m_4]] || %[lt0] = [I3 ++ %[m_4]];\n\t"
        "A1 += %d1 * %h[fo], A0 += %d1 * %d[fo] (FU);\n\t"
        "A1 += %h0 * %h[fex], A0 += %h0 * %d[fex] (FU) || %[fe] = [I2 ++ %[m_4]] || %[fo] = [I3 ++ %[m_4]];\n\t"
        "A1 += %d0 * %h[lt0], A0 += %d0 * %d[lt0] (FU);\n\t"
        "DISALGNEXCPT || %[lt0] = [I0++%[m_4]];\n\t"
        "(%0, %1) = BYTEUNPACK r1:0 (R);\n\t"
        "A1 += %h1 * %h[fe], A0 += %h1 * %d[fe] (FU) || %[fex] = [I2 ++ %[m_4]] || %[lt1] = [I3 ++ %[m_4]];\n\t"
        "A1 += %d1 * %h[fo], A0 += %d1 * %d[fo] (FU);\n\t"
        "A1 += %h0 * %h[fex], A0 += %h0 * %d[fex] (FU) || %[fe] = [I2 ++ %[m_4]] || %[fo] = [I3 ++ %[m_4]];\n\t"
        "%0 = (A1 += %d0 * %h[lt1]), %1 = (A0 += %d0 * %d[lt1]) (FU);\n\t"
        "LOOP_END inner_add;\n\t"
        /*=========================*/
        /* chop s1s */
        "%[lt1] = %h2 (Z);\n\t"
        "%2 >>= 16;\n\t"
        "%[lt1] = %[lt1] - %2;\n\t"
        "%2 <<= 4;\n\t"
        "%2 = %2 + %[lt1];\n\t"
        /* add s1s * 8 to s2 */
        "%2 <<= 3;\n\t"
        "%1 = %1 + %2;\n\t"
        /* chop s2 */
        "%2 = %h1 (Z);\n\t"
        "%1 >>= 16;\n\t"
        "%2 = %2 - %1;\n\t"
        "%1 <<= 4;\n\t"
        "%1 = %1 + %2;\n\t"
        /* chop s1 */
        "%2 = %h0 (Z);\n\t"
        "%0 >>= 16;\n\t"
        "%2 = %2 - %0;\n\t"
        "%0 <<= 4;\n\t"
        "%0 = %0 + %2;\n\t"
        /* more then 8 byte left? */
        "CC = %[len] <= %[pt] (IU);\n\t"
        "IF !CC JUMP 1b;\n\t"
        /****************************/
        /* complete piece left? */
        "CC = %[len] <= 3 (IU);\n\t"
        "IF CC JUMP 3f;\n\t"
        /* handle complete piece */
        "%0 <<= 2;\n\t"
        "%1 = %1 + %0;\n\t"
        "A0 = %1;\n\t"
        "%[len] += -4;\n\t"
        "I2 += %[m_4];\n\t"
        "I3 += %[m_4];\n\t"
        "DISALGNEXCPT || %[lt1] = [I0 ++ %[m_4]];\n\t"
        "(%0, %1) = BYTEUNPACK r1:0 || %[fe] = [I2 ++ %[m_4]] || %[fo] = [I3 ++ %[m_4]];\n\t"
        "A1 += %h1 * %h[fe], A0 += %h1 * %d[fe] (FU) || %[fex] = [I2 ++ %[m_4]] || %[lt0] = [I3 ++ %[m_4]];\n\t"
        "A1 += %d1 * %h[fo], A0 += %d1 * %d[fo] (FU);\n\t"
        "A1 += %h0 * %h[fex], A0 += %h0 * %d[fex] (FU) || %[fe] = [I2 ++ %[m_4]] || %[fo] = [I3 ++ %[m_4]];\n\t"
        "%0 = (A1 += %d0 * %h[lt0]), %1 = (A0 += %d0 * %d[lt0]) (FU);\n\t"
        /* check remaining len */
        "3:\n\t"
        "CC = %[len] == 0;\n\t"
        "IF CC JUMP 4f;\n\t"
        /* handle trailer */
        "%[pt] = I0;\n\t"
        "%[pt] += -4;\n\t"
        "LOOP trailer_add LC1 = %[len];\n\t"
        /*=========================*/
        "LOOP_BEGIN trailer_add;\n\t"
        "%[lt0] = B [%[pt] ++] (Z);\n\t"
        "%0 = %0 + %[lt0];\n\t"
        "%1 = %1 + %0;\n\t"
        "LOOP_END trailer_add;\n\t"
        "%[len] = 0;\n\t"
        "4:\n\t"
        /* chop s1 */
        /* chop s2 */
        "%2 = %h1 (Z);\n\t"
        "%8 = %h0 (Z);\n\t"
        "%1 >>= 16;\n\t"
        "%0 >>= 16;\n\t"
        "%2 = %2 - %1;\n\t"
        "%8 = %8 - %0;\n\t"
        "%1 <<= 4;\n\t"
        "%0 <<= 4;\n\t"
        "%1 = %1 + %2;\n\t"
        "%2 = %[base] (Z);\n\t"
        "%0 = %0 + %8;\n\t"
        "%9 = %1 - %2;\n\t"
        "%8 = %0 - %2;\n\t"
        "CC = %2 <= %1 (IU);\n\t"
        "IF CC %1 = %9;\n\t"
        "CC = %2 <= %0 (IU);\n\t"
        "IF CC %0 = %8\n\t"
        /* disable circular addressing again */
        "L0 = %[len];\n\t"
        "L2 = %[len];\n\t"
        "L3 = %[len];\n\t"
        : /* %0  */ "=q7" (s1),
          /* %1  */ "=q6" (s2),
          /* %2  */ "=d" (s1s),
          /* %3  */ [lt0] "=q0" (lt0),
          /* %4  */ [lt1] "=q1" (lt1),
          /* %5  */ [len] "=a" (len),
          /* %6  */ [lcount] "=a" (lcount),
          /* %7  */ [pt] "=a" (pt),
          /* %8  */ [fo] "=d" (fo),
          /* %9  */ [fe] "=d" (fe),
          /* %10 */ [fex] "=d" (fex)
        : /*     */ [vnmax] "Kuh" (VNMAX),
          /*     */ [base] "Kuh" (BASE),
          /*     */ [m_4] "f" (4),
          /*     */ [vord_o] "x" (&vord_o),
          /*     */ [vord_e] "x" (&vord_e),
          /*     */ [sorder] "i" (sizeof(vord_o)),
          /*     */ "4" (buf),
          /*     */ "0" (s1),
          /*     */ "1" (s2),
          /*     */ "5" (len)
        : "CC", "LC1", "LB1", "LT1", "A0", "A1", "I0", "B0", "L0", "I2", "B2", "L2", "I3", "B3", "L3"
    );

    /* return recombined sums */
    return (s2 << 16) | s1;
}
#endif
