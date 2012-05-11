/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   ia64 implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2009-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/* inline asm ahead, GCC only */
#include <limits.h>
#ifdef __GNUC__
#  define HAVE_ADLER32_VEC
#  define MIN_WORK 64

#  define VNMAX (7*NMAX)
#  define SOULL (sizeof(unsigned long long))

local noinline uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    union scale_order { unsigned short x[4][4]; unsigned long long d[4];};
    local const union scale_order ord_le = {{{8,7,6,5},{4,3,2,1},{16,15,14,13},{12,11,10,9}}};
// TODO: Big Endian is untested
    local const union scale_order ord_be = {{{1,2,3,4},{5,6,7,8},{ 9,10,11,12},{13,14,15,16}}};
    unsigned int s1, s2;

    s1 = adler & 0xffff;
    s2 = (adler >> 16) & 0xffff;

    if (likely(len >= 4*SOULL)) {
        const union scale_order *scale_order;
        unsigned long long vs1, vs2;
        unsigned long long in8;
        unsigned int f, n, k;

        if (!host_is_bigendian())
            scale_order = &ord_le;
        else
            scale_order = &ord_be;

        /* align hard down */
        f = (unsigned int) ALIGN_DOWN_DIFF(buf, SOULL);
        buf = (const Bytef *)ALIGN_DOWN(buf, SOULL);
        n = SOULL - f;

        /* add n times s1 to s2 for start round */
        s2 += s1 * n;

        k = len < VNMAX ? len : VNMAX;
        len -= k;

        /* insert scalar start somewhere */
        vs1 = s1;
        vs2 = s2;
        {
            unsigned long long a, b, c, x, y;

            /* get input data */
            in8 = *(const unsigned long long *)buf;
            if (f) {
                if (!host_is_bigendian()) {
                    in8 >>= f * CHAR_BIT;
                    in8 <<= f * CHAR_BIT;
                } else {
                    in8 <<= f * CHAR_BIT;
                    in8 >>= f * CHAR_BIT;
                }
            }

            asm (
                /* add 8 byte horizontal and add to old qword */
                "psad1	%3=r0, %7\n\t"
                "unpack1.l	%5=r0, %7;;\n\t"

                /* apply order, widen to 32 bit */
                "add	%0=%0, %3\n\t"
                "unpack1.h	%6=r0, %7\n\t"
                "pmpy2.r	%2=%5, %9;;\n\t"

                "add	%1=%1, %2\n\t"
                "pmpy2.r	%4=%6, %8\n\t"
                "pmpy2.l	%3=%5, %9;;\n\t"

                "add	%2=%3, %4\n\t"
                "pmpy2.l	%3=%6, %8;;\n\t"
                "add	%1=%1, %3\n\t"
                : /* %0 */ "=&r" (vs1),
                  /* %1 */ "=&r" (vs2),
                  /* %2 */ "=&r" (a),
                  /* %3 */ "=&r" (b),
                  /* %4 */ "=&r" (c),
                  /* %5 */ "=&r" (x),
                  /* %6 */ "=&r" (y)
                : /* %7 */ "r" (in8),
                  /* %8 */ "r" (scale_order->d[1]),
                  /* %9 */ "r" (scale_order->d[0]),
                  /*  */ "0" (vs1),
                  /*  */ "1" (vs2)
            );
            vs2 += a;
        }

        buf += SOULL;
        k -= n;

        if (likely(k >= 2*SOULL)) {
            register const union scale_order *l_ord asm ("r15");
            register const Bytef *l_buf asm ("r16");
            register unsigned int l_k asm ("r17");
            register unsigned int l_len asm ("r18");
            register unsigned long long l_vs1 asm ("r19");
            register unsigned long long l_vs2 asm ("r20");
            register unsigned long long l_frame asm ("r21");
            void *br_reg;

            asm (
                ".equ inner_loop_count, 31*64\n\t" /* 127*16/64 == 31.75 */
                ".equ buf_ptr_1, %0\n\t"
                ".equ k, %1\n\t"
                ".equ len, %2\n\t"
                ".equ order_ptr, %3\n\t"
                ".equ vs1, %4\n\t"
                ".equ vs2, %5\n\t"
                ".equ vs1_l, vs1\n\t"

                ".equ old_frame_state, r34\n\t"
                ".equ buf_ptr_2, r35\n\t"
                ".equ buf_ptr_3, r36\n\t"
                ".equ buf_ptr_4, r37\n\t"
                ".equ loop_rem, r38\n\t"
                ".equ order_hihi, r39\n\t"
                ".equ order_hilo, r40\n\t"
                ".equ order_lohi, r41\n\t"
                ".equ order_lolo, r42\n\t"

                ".equ input8_1, r43\n\t"
                ".equ input8_2, r44\n\t"
                ".equ input8_3, r45\n\t"
                ".equ input8_4, r46\n\t"
                ".equ input8_5, r47\n\t"
                ".equ input8_6, r48\n\t"
                ".equ input8_7, r49\n\t"
                ".equ input8_8, r50\n\t"

                ".equ sad_1, r51\n\t"
                ".equ sad_2, r52\n\t"
                ".equ sad_3, r53\n\t"
                ".equ sad_4, r54\n\t"
                ".equ sad_5, r55\n\t"
                ".equ sad_6, r56\n\t"
                ".equ sad_7, r57\n\t"
                ".equ sad_8, r58\n\t"

                ".equ vs1_rsum_l, r59\n\t"
                ".equ vs1_rsum_h, r60\n\t"
                ".equ vs1_h, r61\n\t"
                ".equ vs2_wsum_ll, r62\n\t"
                ".equ vs2_wsum_lh, r63\n\t"
                ".equ vs2_wsum_hl, r64\n\t"
                ".equ vs2_wsum_hh, r65\n\t"

                ".equ unpck_01, r66\n\t"
                ".equ unpck_02, r67\n\t"
                ".equ unpck_03, r68\n\t"
                ".equ unpck_04, r69\n\t"
                ".equ unpck_05, r70\n\t"
                ".equ unpck_06, r71\n\t"
                ".equ unpck_07, r72\n\t"
                ".equ unpck_08, r73\n\t"
                ".equ unpck_09, r74\n\t"
                ".equ unpck_10, r75\n\t"
                ".equ unpck_11, r76\n\t"
                ".equ unpck_12, r77\n\t"
                ".equ unpck_13, r78\n\t"
                ".equ unpck_14, r79\n\t"
                ".equ unpck_15, r80\n\t"
                ".equ unpck_16, r81\n\t"

                "mov	%6 = ar.pfs\n\t"
                /* force creation of a new register frame */
                "br.call.sptk.many %7 = 8f\n\t"

                "nop.m 0;;\n\t"
                "mov	ar.pfs = %6\n\t"

                ".subsection 2\n"
                "8:\n\t"
                "alloc	old_frame_state = ar.pfs, 3, 47, 0, 0;;\n\t"
                "ld8	order_hilo = [order_ptr], 8\n\t"
                "nop.i 0;;\n\t"

                "ld8	order_hihi = [order_ptr], 8;;\n\t"
                "ld8	order_lolo = [order_ptr], 8\n\t"
                "nop.i 0;;\n\t"

                "ld8	order_lohi = [order_ptr], 8\n\t"
                "mov	vs1_h = r0;;\n\t"
                "nop.i 0\n\t"

                "nop.m 0\n\t"
                "mov	vs1_rsum_l = r0\n\t"
                "mov	vs1_rsum_h = r0\n\t"

                "4:\n\t"
                "add	buf_ptr_2 = 8, buf_ptr_1\n\t"
                "add	buf_ptr_3 = 16, buf_ptr_1\n\t"
                "add	buf_ptr_4 = 24, buf_ptr_1\n\t"

                "ld8	input8_1 = [buf_ptr_1], 32\n\t"
                "mov	unpck_13 = inner_loop_count\n\t"
                "mov	vs2_wsum_ll = r0;;\n\t"

                "ld8	input8_2 = [buf_ptr_2], 32\n\t"
                "cmp4.ltu	p7,p6 = k, unpck_13\n\t"
                "mov	vs2_wsum_lh = r0\n\t"

                "mov	vs2_wsum_hl = r0;;\n\t"
                "(p6) mov	unpck_15 = inner_loop_count\n\t"
                "(p7) mov	unpck_15 = k;;\n\t"

                "mov	unpck_16 = buf_ptr_1\n\t"
                "and	unpck_14 = -16, unpck_15\n\t"
                "and	loop_rem = 63, unpck_15;;\n\t"

                "mov	vs2_wsum_hh = r0\n\t"
                "shr.u	unpck_13 = unpck_15, 6\n\t"
                "add	unpck_16 = unpck_15, unpck_16;;\n\t"

                "cmp4.eq	p7,p6 = 0, unpck_13\n\t"
                "sub	k = k, unpck_14\n\t"
                "add	unpck_16 = -33, unpck_16;;\n\t"

                /* prefetch end of buffer for this iterration,
                 * to make sure OS sees possible page faults so
                 * OS can swap page in, because speculative
                 * loads will not.
                 */
                "lfetch.fault	[unpck_16]\n\t"
                "nop.i	1\n\t"
                "(p7) br.cond.dpnt.few 5f\n\t"

                "ld8	input8_3 = [buf_ptr_3], 32\n\t"
                "mov	unpck_01 = r0\n\t"
                "add	unpck_13 = -1, unpck_13\n\t"

                "ld8	input8_4 = [buf_ptr_4], 32\n\t"
                "mov	unpck_09 = r0\n\t"
                "mov	unpck_02 = r0\n\t"

                "mov	unpck_10 = r0\n\t"
                "mov	unpck_03 = r0\n\t"
                "mov	unpck_11 = r0\n\t"

                "mov	unpck_04 = r0\n\t"
                "mov	unpck_12 = r0;;\n\t"
                "mov.i	ar.lc = unpck_13\n"

                "1:\n\t"
                "ld8	input8_5 = [buf_ptr_1], 32\n\t"
                "padd2	unpck_01 = unpck_01, unpck_09\n\t"
                "psad1	sad_1 = r0, input8_1\n\t"

                "ld8	input8_6 = [buf_ptr_2], 32\n\t"
                "padd2	unpck_02 = unpck_02, unpck_10\n\t"
                "psad1	sad_2 = r0, input8_2\n\t"

                "ld8	input8_7 = [buf_ptr_3], 32\n\t"
                "padd2	unpck_03 = unpck_03, unpck_11\n\t"
                "psad1	sad_3 = r0, input8_3\n\t"

                "ld8	input8_8 = [buf_ptr_4], 32\n\t"
                "padd2	unpck_04 = unpck_04, unpck_12\n\t"
                "psad1	sad_4 = r0, input8_4\n\t"

                "padd4	vs1_rsum_l = vs1_rsum_l, vs1_l\n\t"
                "padd4	vs1_rsum_h = vs1_rsum_h, vs1_h;;\n\t"
                "psad1	sad_5 = r0, input8_5\n\t"

                "padd2	vs2_wsum_ll = vs2_wsum_ll, unpck_01\n\t"
                "padd2	vs2_wsum_lh = vs2_wsum_lh, unpck_02\n\t"
                "unpack1.l	unpck_05 = r0, input8_3\n\t"

                "padd4	vs1_l = vs1_l, sad_1\n\t"
                "psad1	sad_6 = r0, input8_6\n\t"
                "unpack1.h	unpck_06 = r0, input8_3\n\t"

                "padd4	vs1_h = vs1_h, sad_2\n\t"
                "unpack1.l	unpck_01 = r0, input8_1\n\t"
                "unpack1.h	unpck_02 = r0, input8_1\n\t"

                "padd2	vs2_wsum_hl = vs2_wsum_hl, unpck_03\n\t"
                "unpack1.l	unpck_07 = r0, input8_4\n\t"
                "unpack1.h	unpck_08 = r0, input8_4\n\t"

                "padd2	vs2_wsum_hh = vs2_wsum_hh, unpck_04\n\t"
                "unpack1.l	unpck_09 = r0, input8_5\n\t"
                "unpack1.h	unpck_10 = r0, input8_5;;\n\t"

                "padd4	vs1_rsum_l = vs1_rsum_l, vs1_l\n\t"
                "unpack1.l	unpck_03 = r0, input8_2\n\t"
                "unpack1.h	unpck_04 = r0, input8_2\n\t"

                "padd4	vs1_rsum_h = vs1_rsum_h, vs1_h\n\t"
                "unpack1.l	unpck_11 = r0, input8_6\n\t"
                "unpack1.h	unpck_12 = r0, input8_6\n\t"

                "padd2	unpck_01 = unpck_01, unpck_05\n\t"
                "unpack1.l	unpck_13 = r0, input8_7;;\n\t"
                "mux2	vs1_rsum_h = vs1_rsum_h, 0x4e\n\t"

                "padd2	unpck_02 = unpck_02, unpck_06\n\t"
                "mux2	vs1_rsum_l = vs1_rsum_l, 0x4e\n\t"
                "unpack1.h	unpck_14 = r0, input8_7\n\t"

                "padd4	vs1_l = vs1_l, sad_3\n\t"
                "unpack1.h	unpck_16 = r0, input8_8\n\t"
                "unpack1.l	unpck_15 = r0, input8_8\n\t"

                "padd2	unpck_03 = unpck_03, unpck_07\n\t"
                "padd4	vs1_h = vs1_h, sad_4\n\t"
                "psad1	sad_7 = r0, input8_7\n\t"

                "ld8.s	input8_1 = [buf_ptr_1], 32\n\t"
                "padd2	unpck_04 = unpck_04, unpck_08\n\t"
                "psad1	sad_8 = r0, input8_8\n\t"

                "ld8.s	input8_2 = [buf_ptr_2], 32\n\t"
                "padd2	unpck_09 = unpck_09, unpck_13;;\n\t"
                "padd2	unpck_10 = unpck_10, unpck_14\n\t"

                "ld8.s	input8_3 = [buf_ptr_3], 32\n\t"
                "padd4	vs1_rsum_l = vs1_rsum_l, vs1_l\n\t"
                "padd4	vs1_rsum_h = vs1_rsum_h, vs1_h\n\t"

                "ld8.s	input8_4 = [buf_ptr_4], 32\n\t"
                "padd4	vs1_l = vs1_l, sad_5\n\t"
                "padd4	vs1_h = vs1_h, sad_6;;\n\t"

                "padd4	vs1_rsum_l = vs1_rsum_l, vs1_l\n\t"
                "padd4	vs1_rsum_h = vs1_rsum_h, vs1_h\n\t"
                "padd4	vs1_l = vs1_l, sad_7\n\t"

                "padd4	vs1_h = vs1_h, sad_8\n\t"
                "padd2	unpck_11 = unpck_11, unpck_15\n\t"
                "padd2	unpck_12 = unpck_12, unpck_16\n\t"

                "nop.m 0\n\t"
                "nop.i 0\n\t"
                "br.cloop.sptk.many 1b\n\t"

                "padd2	unpck_01 = unpck_01, unpck_09\n\t"
                "padd2	unpck_02 = unpck_02, unpck_10;;\n\t"
                "padd2	unpck_03 = unpck_03, unpck_11\n\t"

                "padd2	unpck_04 = unpck_04, unpck_12\n\t"
                "padd2	vs2_wsum_ll = vs2_wsum_ll, unpck_01\n\t"
                "padd2	vs2_wsum_lh = vs2_wsum_lh, unpck_02;;\n\t"

                "padd2	vs2_wsum_hl = vs2_wsum_hl, unpck_03\n\t"
                "padd2	vs2_wsum_hh = vs2_wsum_hh, unpck_04\n\t"
                "nop.i 0\n\t"

                "5:\n\t"
                "cmp4.gtu	p7,p6 = 16, loop_rem\n\t"
                "add	buf_ptr_1 = -16, buf_ptr_1\n\t"
                "add	buf_ptr_2 = -16, buf_ptr_2;;\n\t"

                "nop.m 0\n\t"
                "nop.i 0\n\t"
                "(p7) br.cond.dptk.few 2f\n"

                "add	loop_rem = -16, loop_rem;;\n\t"
                "nop.m 0\n\t"
                "shr.u	unpck_15 = loop_rem, 4;;\n\t"

                "nop.m 0\n\t"
                "mov.i ar.lc = unpck_15\n\t"
                "nop.i 0\n\t"

                "3:\n\t"
                "padd4	vs1_rsum_l = vs1_rsum_l, vs1_l\n\t"
                "psad1	sad_1 = r0, input8_1;;\n\t"
                "psad1	sad_2 = r0, input8_2\n\t"

                "padd4	vs1_rsum_h = vs1_rsum_h, vs1_h\n\t"
                "unpack1.l	unpck_01 = r0, input8_1;;\n\t"
                "mux2	vs1_rsum_l = vs1_rsum_l, 0x4e\n\t"

                "padd4 vs1_l = vs1_l, sad_1\n\t"
                "unpack1.h	unpck_02 = r0, input8_1\n\t"
                "mux2	vs1_rsum_h = vs1_rsum_h, 0x4e\n\t"

                "padd4 vs1_h = vs1_h, sad_2\n\t"
                "unpack1.l	unpck_03 = r0, input8_2\n\t"
                "unpack1.h	unpck_04 = r0, input8_2\n\t"

                "ld8.s input8_1 = [buf_ptr_1], 16\n\t"
                "padd2	vs2_wsum_ll = vs2_wsum_ll, unpck_01;;\n\t"
                "padd2	vs2_wsum_lh = vs2_wsum_lh, unpck_02\n\t"

                "ld8.s input8_2 = [buf_ptr_2], 16\n\t"
                "padd2	vs2_wsum_hl = vs2_wsum_hl, unpck_03\n\t"
                "padd2	vs2_wsum_hh = vs2_wsum_hh, unpck_04\n\t"

                "nop.m 0\n\t"
                "nop.i 0\n\t"
                "br.cloop.sptk.few 3b\n"

                "2:\n\t"
                "add	buf_ptr_1 = -16, buf_ptr_1\n\t"
                "pmpy2.r	unpck_01 = vs2_wsum_ll, order_lolo\n\t"
                "pmpy2.l	unpck_02 = vs2_wsum_ll, order_lolo;;\n\t"

                "add	buf_ptr_2 = -16, buf_ptr_2\n\t"
                "pmpy2.r	unpck_03 = vs2_wsum_lh, order_lohi\n\t"
                "pmpy2.l	unpck_04 = vs2_wsum_lh, order_lohi\n\t"

                "padd4	unpck_01 = unpck_01, unpck_02\n\t"
                "pmpy2.r	unpck_05 = vs2_wsum_hl, order_hilo\n\t"
                "pmpy2.l	unpck_06 = vs2_wsum_hl, order_hilo;;\n\t"

                "padd4	unpck_03 = unpck_03, unpck_04\n\t"
                "pmpy2.r	unpck_07 = vs2_wsum_hh, order_hihi\n\t"
                "pmpy2.l	unpck_08 = vs2_wsum_hh, order_hihi\n\t"

                "padd4	unpck_05 = unpck_05, unpck_06;;\n\t"
                "padd4	unpck_07 = unpck_07, unpck_08\n\t"
                "padd4	unpck_01 = unpck_01, unpck_03;;\n\t"

                "padd4	unpck_05 = unpck_05, unpck_07\n\t"
                "padd4	vs2 = vs2, unpck_01;;\n\t"
                "padd4	vs2 = vs2, unpck_05\n\t"

                "cmp4.ltu	p7,p6 = 16, k\n\t"
                "nop.m 0\n\t"
                "nop.i 0\n\t;;"

                "nop.m 0\n\t"
                "nop.i 0\n\t"
                "(p7) br.cond.dptk.few 4b\n\t"

                "padd4	vs1_l = vs1_l, vs1_h\n\t"
                "mix2.r	unpck_01 = r0, vs1_rsum_l\n\t"
                "mix2.l	unpck_02 = r0, vs1_rsum_l;;\n\t"

                "mov	vs1_h = r0\n\t"
                "mix2.r	unpck_03 = r0, vs1_rsum_h\n\t"
                "mix2.l	unpck_04 = r0, vs1_rsum_h;;\n\t"

                "nop.m 0\n\t"
                "mix2.r	unpck_05 = r0, vs1_l\n\t"
                "mix2.l	unpck_06 = r0, vs1_l\n\t"

                "psub4	unpck_01 = unpck_01, unpck_02\n\t"
                "psub4	unpck_03 = unpck_03, unpck_04\n\t"
                "shl	unpck_02 = unpck_02, 4;;\n\t"

                "padd4	vs1_rsum_l = unpck_01, unpck_02\n\t"
                "psub4	unpck_05 = unpck_05, unpck_06\n\t"
                "shl	unpck_04 = unpck_04, 4;;\n\t"

                "padd4	vs1_rsum_h = unpck_03, unpck_04\n\t"
                "shl	unpck_06 = unpck_06, 4;;\n\t"

                "padd4	vs1_l = unpck_05, unpck_06\n\t"
                "padd4	vs1_rsum_l = vs1_rsum_l, vs1_rsum_h;;\n\t"
                "shl	vs1_rsum_l = vs1_rsum_l, 4;;\n\t"

                "padd4	vs2 = vs2, vs1_rsum_l\n\t"
                "add	len = len, k;;\n\t"
                "mix2.r	unpck_01 = r0, vs2\n\t"

                "mov	vs1_rsum_l = r0\n\t"
                "mov	unpck_15 = %8\n\t"
                "mix2.l	unpck_02 = r0, vs2;;\n\t"

                "psub4	unpck_01 = unpck_01, unpck_02\n\t"
                "shl	unpck_02 = unpck_02, 4;;\n\t"
                "padd4	vs2 = unpck_01, unpck_02\n\t"

                "cmp4.ltu	p7,p6 = len, unpck_15;;\n\t"
                "(p6) mov k = %8\n\t"
                "(p7) mov k = len;;\n\t"

                "sub	len = len, k\n\t"
                "cmp4.ltu	p7,p6 = 16, k\n\t"
                "mov	vs1_rsum_h = r0;;\n\t"

                "nop.m 0\n\t"
                "nop.i 0\n\t"
                "(p7) br.cond.dptk 4b\n\t"

                "nop.m 0\n\t"
                "mov	ar.pfs = old_frame_state;;\n\t"
                "nop.i 0\n\t"

                "nop.m 0\n\t"
                "nop.i 0\n\t"
                "br.ret.sptk.many %7\n\t"

                "nop.m 22\n"
                "nop.i 23\n"
                "nop.i 24\n"
                /*
                 * ??? .previous seems to be buggy. It
                 * removes the last bundle from this
                 * section and inserts it into the previous...
                 */
                ".previous\n\t"
                : /* %0 */ "=r" (l_buf),
                  /* %1 */ "=r" (l_k),
                  /* %2 */ "=r" (l_len),
                  /* %3 */ "=r" (l_ord),
                  /* %4 */ "=r" (l_vs1),
                  /* %5 */ "=r" (l_vs2),
                  /* %6 */ "=r" (l_frame),
                  /* %7 */ "=b" (br_reg)
                : /* %8 */ "i" (VNMAX),
                  /*  */ "0" (buf),
                  /*  */ "1" (k),
                  /*  */ "2" (len),
                  /*  */ "3" (scale_order),
                  /*  */ "4" (vs1),
                  /*  */ "5" (vs2)
                : "p6", "p7", "ar.lc"
            );
            buf = l_buf;
            k = l_k;
            len = l_len;
            vs1 = l_vs1;
            vs2 = l_vs2;
        }

        /* complete piece left? */
        if (k >= SOULL) {
            unsigned long long a, b, c, x, y;

            in8 = *(const unsigned long long *)buf;
            asm (
                "shladd	%1=%0,3,%1\n\t"
                "psad1	%3=r0, %7\n\t"
                "unpack1.l	%5=r0, %7;;\n\t"

                "add	%0=%0, %3\n\t"
                "unpack1.h	%6=r0, %7\n\t"
                "pmpy2.r	%2=%5, %9;;\n\t"

                "add	%1=%1, %2\n\t"
                "pmpy2.r	%4=%6, %8\n\t"
                "pmpy2.l	%3=%5, %9;;\n\t"

                "add	%2=%3, %4\n\t"
                "pmpy2.l	%3=%6, %8;;\n\t"
                "add	%1=%1, %3\n\t"
                : /* %0 */ "=&r" (vs1),
                  /* %1 */ "=&r" (vs2),
                  /* %2 */ "=&r" (a),
                  /* %3 */ "=&r" (b),
                  /* %4 */ "=&r" (c),
                  /* %5 */ "=&r" (x),
                  /* %6 */ "=&r" (y)
                : /* %7 */ "r" (in8),
                  /* %8 */ "r" (scale_order->d[1]),
                  /* %9 */ "r" (scale_order->d[0]),
                  /*  */ "0" (vs1),
                  /*  */ "1" (vs2)
            );
            vs2 += a;
            buf += SOULL;
            k   -= SOULL;
        }

        if (likely(k)) {
            unsigned long long a, b, c, x, y;
            /* get input data */
            in8 = *(const unsigned long long *)buf;
            unsigned int n = SOULL - k;

            /* swizzle data in place */
            if (!host_is_bigendian())
                in8 <<= n * CHAR_BIT;
            else
                in8 >>= n * CHAR_BIT;

            /* add k times vs1 for this trailer */
            vs2 += vs1 * k;

            asm (
                /* add 8 byte horizontal and add to old qword */
                "psad1	%3=r0, %7\n\t"
                "unpack1.l	%5=r0, %7;;\n\t"

                /* apply order, widen to 32 bit */
                "add	%0=%0, %3\n\t"
                "unpack1.h	%6=r0, %7\n\t"
                "pmpy2.r	%2=%5, %9;;\n\t"

                "add	%1=%1, %2\n\t"
                "pmpy2.r	%4=%6, %8\n\t"
                "pmpy2.l	%3=%5, %9;;\n\t"

                "add	%2=%3, %4\n\t"
                "pmpy2.l	%3=%6, %8;;\n\t"
                "add	%1=%1, %3\n\t"
                : /* %0 */ "=&r" (vs1),
                  /* %1 */ "=&r" (vs2),
                  /* %2 */ "=&r" (a),
                  /* %3 */ "=&r" (b),
                  /* %4 */ "=&r" (c),
                  /* %5 */ "=&r" (x),
                  /* %6 */ "=&r" (y)
                : /* %7 */ "r" (in8),
                  /* %8 */ "r" (scale_order->d[1]),
                  /* %9 */ "r" (scale_order->d[0]),
                  /*  */ "0" (vs1),
                  /*  */ "1" (vs2)
            );
            vs2 += a;

            buf += k;
            k -= k;
        }

        /* vs1 is one giant 64 bit sum, but we only calc to 32 bit */
        s1 = vs1;
        /* add both vs2 sums */
        asm (
            "unpack4.h	%0=r0, %1\n\t"
            "unpack4.l	%1=r0, %1\n\t"
            : /* %0 */ "=r" (s2),
              /* %1 */ "=r" (vs2)
            : /* %2 */ "1" (vs2)
        );
        s2 += vs2;
        /* modulo again in scalar code */
    }

    if (unlikely(len)) do {
        s1 += *buf++;
        s2 += s1;
    } while (--len);
    MOD28(s1);
    MOD28(s2);
    return (s2 << 16) | s1;
}
#endif
