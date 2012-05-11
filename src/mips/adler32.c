/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   mips implementation
 * Copyright (C) 1995-2004 Mark Adler
 * Copyright (C) 2009-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/* we use a bunch of inline asm and GCC vector internals, so GCC it is */
#ifdef __GNUC__
#  if _MIPS_SZPTR < 64
#    define SZPRFX
#  else
#    define SZPRFX "d"
#  endif
#  include <limits.h>
#  ifdef __mips_loongson_vector_rev
#    define HAVE_ADLER32_VEC
#    define MIN_WORK 64

/*
 * The loongson vector stuff is basically a variant of MMX,
 * looks like ST had some licenses for that from their x86 days
 */
#    include <loongson.h>
#    define SOV8 (sizeof(uint8x8_t))
#    define VNMAX (4*NMAX)

/* GCCs loongson port looks like a quick hack.
 * It can output some simple vector instruction sequences,
 * but creates horrible stuff when you really use it.
 * So more coding by hand...
 */

/* ========================================================================= */
local inline uint32x2_t vector_chop(uint32x2_t x)
{
    uint32x2_t y;

    y = psllw_u(x, 16);
    y = psrlw_u(y, 16);
    x = psrlw_u(x, 16);
    y = psubw_u(y, x);
    x = psllw_u(x, 4);
    x = paddw_u(x, y);
    return x;
}

/* ========================================================================= */
local noinline uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1, s2;

    /* split Adler-32 into component sums */
    s1 = adler & 0xffff;
    s2 = (adler >> 16) & 0xffff;

    if(likely(len >= 2*SOV8))
    {
        /* Loongsons and their ST MMX foo are little endian */
        static const int16x4_t vord_lo = {8,7,6,5};
        static const int16x4_t vord_hi = {4,3,2,1};
        uint32x2_t vs2, vs1;
        int16x4_t in_lo, in_hi;
        uint8x8_t v0 = {0};
        uint8x8_t in8;
        unsigned f, n;
        unsigned k;

        /*
         * Add stuff to achieve alignment
         */
        /* align hard down */
        f = (unsigned) ALIGN_DOWN_DIFF(buf, SOV8);
        n = SOV8 - f;
        buf = (const unsigned char *)ALIGN_DOWN(buf, SOV8);

        /* add n times s1 to s2 for start round */
        s2 += s1 * n;

        k = len < VNMAX ? (unsigned)len : VNMAX;
        len -= k;

        /* insert scalar start somewhere */
        vs1 = (uint32x2_t)(unsigned long long)s1;
        vs2 = (uint32x2_t)(unsigned long long)s2;

        /* get input data */
        /* add all byte horizontal and add to old qword */
        /* apply order, add 4 byte horizontal and add to old dword */
        asm (
                "ldc1	%0, %9\n\t"
                "dsrl	%0, %0, %5\n\t"
                "dsll	%0, %0, %5\n\t"
                "biadd	%4, %0\n\t"
                "punpcklbh	%3, %0, %6\n\t"
                "paddw	%1, %1, %4\n\t"
                "punpckhbh	%4, %0, %6\n\t"
                "pmaddhw	%3, %3, %7\n\t"
                "pmaddhw	%4, %4, %8\n\t"
                "paddw	%2, %2, %3\n\t"
                "paddw	%2, %2, %4\n\t"
                : /* %0  */ "=&f" (in8),
                  /* %1  */ "=f" (vs1),
                  /* %2  */ "=f" (vs2),
                  /* %3  */ "=&f" (in_lo),
                  /* %4  */ "=&f" (in_hi)
                : /* %5  */ "f" (f * CHAR_BIT),
                  /* %6  */ "f" (v0),
                  /* %7  */ "f" (vord_lo),
                  /* %8  */ "f" (vord_hi),
                  /* %9  */ "m" (*buf),
                  /* %10 */ "1" (vs1),
                  /* %11 */ "2" (vs2)
        );
        buf += SOV8;
        k -= n;

        if (likely(k >= SOV8)) do {
            uint32x2_t vs1_r;
            uint16x4_t vs2_lo, vs2_hi;
            int t, j;

            /*
             * GCC generates horible loop code, so write
             * the core loop by hand...
             */
            __asm__ __volatile__ (
                    ".set noreorder\n\t"
                    "b	5f\n\t"
                    "xor	%3, %3, %3\n"
                    "2:\n\t"
                    "xor	%6, %6, %6\n\t"
                    "sll	%10, %11, 3\n\t"
                    "xor	%7, %7, %7\n\t"
                    "subu	%9, %9, %10\n"
                    "1:\n\t"
                    "ldc1	%0, (%8)\n\t"
                    SZPRFX"addiu	%8, %8, 8\n\t"
                    "addiu	%11, %11, -1\n\t"
                    "paddw	%3, %1, %3\n\t"
                    "biadd	%5, %0\n\t"
                    "punpcklbh	%4, %0, %12\n\t"
                    "paddw	%1, %5, %1\n\t"
                    "punpckhbh	%5, %0, %12\n\t"
                    "paddh	%6, %6, %4\n\t"
                    "bnez	%11, 1b\n\t"
                    "paddh	%7, %7, %5\n\t"
                    /* loop bottom  */
                    "pshufh	%1, %1, %15\n\t"
                    "sltiu	%10, %9, 8\n\t"
                    "pmaddhw	%4, %6, %13\n\t"
                    "pmaddhw	%5, %7, %14\n\t"
                    "paddw	%2, %2, %4\n\t"
                    "bnez	%10, 4f\n\t"
                    "paddw	%2, %2, %5\n"
                    "5:\n\t"
                    "sltiu	%10, %9, 1032\n\t"
                    "beqz	%10, 2b\n\t"
                    "li	%11, 128\n\t"
                    "b	2b\n\t"
                    "srl	%11, %9, 3\n"
                    "4:\n\t"
                    ".set reorder\n\t"
                    : /* %0  */ "=&f" (in8),
                      /* %1  */ "=f" (vs1),
                      /* %2  */ "=f" (vs2),
                      /* %3  */ "=&f" (vs1_r),
                      /* %4  */ "=&f" (in_lo),
                      /* %5  */ "=&f" (in_hi),
                      /* %6  */ "=&f" (vs2_lo),
                      /* %7  */ "=&f" (vs2_hi),
                      /* %8  */ "=d" (buf),
                      /* %9  */ "=r" (k),
                      /* %10 */ "=r" (t),
                      /* %11 */ "=r" (j)
                    : /* %12 */ "f" (v0),
                      /* %13 */ "f" (vord_lo),
                      /* %14 */ "f" (vord_hi),
                      /* %15 */ "f" (0x4e),
                      /* %15 */ "1" (vs1),
                      /* %16 */ "2" (vs2),
                      /* %17 */ "8" (buf),
                      /* %18 */ "9" (k)
            );
            /* And the rest of the generated code also looks awful.
             * Looks like GCC is missing instruction patterns for:
             * - 64 bit shifts in loongson copro regs
             * - logic in loongson copro regs
             * and to make things much worse, GCC seems to be missing
             * a loongson copro register <-> copro register move
             * pattern (for example using an or instruction), instead
             * GCC always moves over the GPR.
             *
             * But still, let the compiler handle this, we get some
             * extra moves between copro regs and GPR, but save us
             * a lot of work.
             * And maybe some day some one will fix this...
             */

            /* chop vs1 round sum before multiplying by 8 */
            vs1_r = vector_chop(vs1_r);
            /* add all vs1 for 8 times */
            vs2 = paddw_u(psllw_u(vs1_r, 3), vs2);
            /* chop the vectors to something in the range of BASE */
            vs2 = vector_chop(vs2);
            vs1 = vector_chop(vs1);
            len += k;
            k = len < VNMAX ? (unsigned)len : VNMAX;
            len -= k;
        } while (likely(k >= SOV8));

        if (likely(k)) {
            uint32x2_t vk;
            /* handle trailer */
            f = SOV8 - k;

            vk = (uint32x2_t)(unsigned long long)k;

            /* get input data */
            /* add all byte horizontal and add to old qword */
            /* add k times vs1 for this trailer */
            /* apply order, add 4 byte horizontal and add to old dword */
            __asm__ (
                    "ldc1	%0, %11\n\t"
                    "pmuluw	%3, %1, %6\n\t"
                    "pshufh	%1, %1, %10\n\t"
                    "paddw	%2, %2, %3\n\t"
                    "pmuluw	%3, %1, %6\n\t"
                    "dsll	%0, %0, %5\n\t"
                    "biadd	%4, %0\n\t"
                    "paddw	%2, %2, %3\n\t"
                    "punpcklbh	%3, %0, %7\n\t"
                    "paddw	%1, %1, %4\n\t"
                    "punpckhbh	%4, %0, %7\n\t"
                    "pmaddhw	%3, %3, %8\n\t"
                    "pmaddhw	%4, %4, %9\n\t"
                    "paddw	%2, %2, %3\n\t"
                    "paddw	%2, %2, %4\n\t"
                    : /* %0  */ "=&f" (in8),
                      /* %1  */ "=f" (vs1),
                      /* %2  */ "=f" (vs2),
                      /* %3  */ "=&f" (in_lo),
                      /* %4  */ "=&f" (in_hi)
                    : /* %5  */ "f" (f * CHAR_BIT),
                      /* %6  */ "f" (vk),
                      /* %7  */ "f" (v0),
                      /* %8  */ "f" (vord_lo),
                      /* %9  */ "f" (vord_hi),
                      /* %10 */ "f" (0x4e),
                      /* %11 */ "m" (*buf),
                      /* %12 */ "1" (vs1),
                      /* %13 */ "2" (vs2)
            );

            buf += k;
            k -= k;
        }

        /* add horizontal */
        vs1 = paddw_u(vs1, (uint32x2_t)pshufh_u((uint16x4_t)v0, (uint16x4_t)vs1, 0x4E));
        vs2 = paddw_u(vs2, (uint32x2_t)pshufh_u((uint16x4_t)v0, (uint16x4_t)vs2, 0x4E));
        /* shake and roll */
        s1 = (unsigned int)(unsigned long long)vs1;
        s2 = (unsigned int)(unsigned long long)vs2;
        /* modulo again in scalar code */
    }

    /* handle a possible trailer */
    if (unlikely(len)) do {
        s1 += *buf++;
        s2 += s1;
    } while (--len);
    MOD28(s1);
    MOD28(s2);

    /* return recombined sums */
    return (s2 << 16) | s1;
}

#  elif defined(__mips_dsp)
#    define HAVE_ADLER32_VEC
#    define MIN_WORK 16
#    define VNMAX ((5*NMAX)/2)

typedef signed char v4i8 __attribute__((vector_size(4)));
#    define SOV4 (sizeof(v4i8))

/* ========================================================================= */
local noinline uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1, s2;

    /* split Adler-32 into component sums */
    s1 = adler & 0xffff;
    s2 = (adler >> 16) & 0xffff;

    if (likely(len >= 2*SOV4)) {
        static const v4i8 v1 = {1,1,1,1};
        v4i8 vord_lo, vord_hi, vord;
        /*
         * yes, our sums should stay within 32 bit, that accs are
         * 64 bit is to no (real) use to us.
         */
        unsigned int vs1, vs2, vs1h, vs2h;
        v4i8 in4;
        unsigned f, n;
        unsigned k;
        unsigned int srl;

        if(host_is_bigendian()) {
            vord_lo = (v4i8){8,7,6,5};
            vord_hi = (v4i8){4,3,2,1};
        } else {
            vord_lo = (v4i8){5,6,7,8};
            vord_hi = (v4i8){1,2,3,4};
        }
        vord = vord_hi;
        /*
         * Add stuff to achieve alignment
         */
        /* align hard down */
        f = (unsigned) ALIGN_DOWN_DIFF(buf, SOV4);
        n = SOV4 - f;
        buf = (const unsigned char *)ALIGN_DOWN(buf, SOV4);

        /* add n times s1 to s2 for start round */
        s2 += s1 * n;

        k = len < VNMAX ? len : VNMAX;
        len -= k;

        /* insert scalar start somewhere */
        vs1 = s1;
        vs2 = s2;
        vs1h = 0;
        vs2h = 0;

        /* get input data */
        srl = *(const unsigned int *)buf;
        if(host_is_bigendian()) {
            srl <<= f * CHAR_BIT;
            srl >>= f * CHAR_BIT;
        } else {
            srl >>= f * CHAR_BIT;
            srl <<= f * CHAR_BIT;
        }
        in4 = (v4i8)srl;

        /* add all byte horizontal and add to old dword */
        vs1 = __builtin_mips_dpau_h_qbl(vs1, in4, v1);
        vs1 = __builtin_mips_dpau_h_qbr(vs1, in4, v1);

        /* apply order, add 4 byte horizontal and add to old dword */
        vs2 = __builtin_mips_dpau_h_qbl(vs2, in4, vord);
        vs2 = __builtin_mips_dpau_h_qbr(vs2, in4, vord);

        buf += SOV4;
        k -= n;

        if (likely(k >= 2*SOV4)) do {
            unsigned int vs1_r, vs1h_r, t1, t2;
            v4i8 in4h;

            /*
             * gcc and special regs...
             * Since the mips acc are special, gcc fears to treat.
             * Esp. gcc thinks it's cool to have the result ready in
             * the gpr at the end of this main loop, so he hoist a
             * move out of the acc and back in into the acc into the
             * loop. m(
             *
             * So do it by hand, unrolled two times...
             */
            __asm__ (
                    ".set noreorder\n\t"
                    "mflo	%10, %q0\n\t"
                    "xor	%6, %6, %6\n\t"
                    "xor	%7, %7, %7\n"
                    "1:\n\t"
                    "lw	%4,(%9)\n\t"
                    "lw	%5,4(%9)\n\t"
                    "addu	%6, %6, %10\n\t"
                    "dpau.h.qbl	%q0, %4, %14\n\t"
                    "addiu	%8, %8, -8\n\t"
                    "mflo	%10, %q1\n\t"
                    "dpau.h.qbr	%q0, %4, %14\n\t"
                    "dpau.h.qbl	%q1, %5, %14\n\t"
                    "sltu	%11, %8, 8\n\t"
                    "dpau.h.qbl	%q3, %4, %13\n\t"
                    "addu	%7, %7 ,%10\n\t"
                    "dpau.h.qbl	%q2, %5, %12\n\t"
                    SZPRFX"addiu	%9, %9, 8\n\t"
                    "mflo	%10, %q0\n\t"
                    "dpau.h.qbr	%q1, %5, %14\n\t"
                    "dpau.h.qbr	%q3, %4, %13\n\t"
                    "beqz	%11, 1b\n\t"
                    "dpau.h.qbr	%q2, %5, %12\n\t"
                    ".set reorder"
                    : /* %0  */ "=a" (vs1),
                      /* %1  */ "=A" (vs1h),
                      /* %2  */ "=A" (vs2),
                      /* %3  */ "=A" (vs2h),
                      /* %4  */ "=&r" (in4),
                      /* %5  */ "=&r" (in4h),
                      /* %6  */ "=&r" (vs1_r),
                      /* %7  */ "=&r" (vs1h_r),
                      /* %8  */ "=r" (k),
                      /* %9  */ "=d" (buf),
                      /* %10 */ "=r" (t1),
                      /* %11 */ "=r" (t2)
                    : /* %12 */ "r" (vord_lo),
                      /* %13 */ "r" (vord_hi),
                      /* %14 */ "r" (v1),
                      /* %  */ "0" (vs1),
                      /* %  */ "1" (vs1h),
                      /* %  */ "2" (vs2),
                      /* %  */ "3" (vs2h),
                      /* %  */ "8" (k),
                      /* %  */ "9" (buf)
            );

            /* chop vs1 round sum before multiplying by 8 */
            CHOP(vs1_r);
            CHOP(vs1h_r);
            /* add all vs1 for 8 times */
            vs2  += vs1_r  * 8;
            vs2h += vs1h_r * 8;
            /* chop the vectors to something in the range of BASE */
            CHOP(vs2);
            CHOP(vs2h);
            CHOP(vs1);
            CHOP(vs1h);
            len += k;
            k = len < VNMAX ? len : VNMAX;
            len -= k;
        } while (likely(k >= 2*SOV4));
        vs1 += vs1h;
        vs2 += vs2h;
        /* a complete piece left? */
        if (likely(k >= SOV4)) {
            /* get input data */
            in4 = *(const v4i8 *)buf;
            vs2 += vs1 * 4;

            /* add all byte horizontal and add to old dword */
            vs1 = __builtin_mips_dpau_h_qbl(vs1, in4, v1);
            vs1 = __builtin_mips_dpau_h_qbr(vs1, in4, v1);

            /* apply order, add 4 byte horizontal and add to old dword */
            vs2 = __builtin_mips_dpau_h_qbl(vs2, in4, vord);
            vs2 = __builtin_mips_dpau_h_qbr(vs2, in4, vord);

            k -= SOV4;
            buf += SOV4;
        }

        if (likely(k)) {
            unsigned int vk;

            /*
             * handle trailer
             */
            f = SOV4 - k;

            /* add k times vs1 for this trailer */
            vk = vs1 * k;

            /* get input data */
            srl = *(const unsigned int *)buf;
            if(host_is_bigendian()) {
                srl >>= f * CHAR_BIT;
                srl <<= f * CHAR_BIT;
            } else {
                srl <<= f * CHAR_BIT;
                srl >>= f * CHAR_BIT;
            }
            in4 = (v4i8)srl;

            /* add all byte horizontal and add to old dword */
            vs1 = __builtin_mips_dpau_h_qbl(vs1, in4, v1);
            vs1 = __builtin_mips_dpau_h_qbr(vs1, in4, v1);

            /* apply order, add 4 byte horizontal and add to old dword */
            vs2 = __builtin_mips_dpau_h_qbl(vs2, in4, vord);
            vs2 = __builtin_mips_dpau_h_qbr(vs2, in4, vord);

            vs2 += vk;

            buf += k;
            k -= k;
        }

        /* shake and roll */
        s1 = (unsigned int)vs1;
        s2 = (unsigned int)vs2;
        /* modulo again in scalar code */
    }

    /* handle a possible trailer */
    if (unlikely(len)) do {
        s1 += *buf++;
        s2 += s1;
    } while (--len);
    /* s2 should be small here */
    MOD28(s1);
    MOD28(s2);

    /* return recombined sums */
    return (s2 << 16) | s1;
}
#  endif
#endif
