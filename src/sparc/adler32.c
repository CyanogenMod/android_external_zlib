/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   sparc implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2009-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/* we use inline asm, so GCC it is */
#if defined(HAVE_VIS) && defined(__GNUC__)
#  define HAVE_ADLER32_VEC
#  define MIN_WORK 512
#  define VNMAX ((5*NMAX)/2)
#  define SOVV (sizeof(unsigned long long))

/*
 * SPARC CPUs gained a SIMD extention with the UltraSPARC I, called
 * VIS, implemented in their FPU. We can build adler32 with it.
 *
 * But Note:
 * - Do not use it with Niagara or other CPUs like it. They have a
 *   shared FPU (T1: 1 FPU for all up to 8 cores, T2: 1 FPU for 8 threads)
 *   and to make matters worse the code does not seem to work there
 *   (binary which creates correct results on other SPARC creates wrong
 *   result on T1)
 * - There is no clear preprocesor define which tells us if we have VIS.
 *   Often the tool chain even disguises a sparcv9 as a sparcv8
 *   (pre-UltraSPARC) to not confuse the userspace.
 * - The code has a high startup cost
 * - The code only handles big endian
 *
 * For these reasons this code is not automatically enabled and you have
 * to define HAVE_VIS as a switch to enable it. We can not easily provide a
 * dynamic runtime switch. The CPU has make and model encoded in the Processor
 * Status Word (PSW), but reading the PSW is a privilidged instruction (same
 * as PowerPC...).
 */

/* ========================================================================= */
local inline unsigned long long fzero(void)
{
    unsigned long long t;
    asm ("fzero %0" : "=e" (t));
    return t;
}

/* ========================================================================= */
local inline unsigned long long fpmerge_lo(unsigned long long a, unsigned long long b)
{
    unsigned long long t;
    asm ("fpmerge	%L1, %L2, %0" : "=e" (t) : "f" (a), "f" (b));
    return t;
}

/* ========================================================================= */
local inline unsigned long long fpmerge_hi(unsigned long long a, unsigned long long b)
{
    unsigned long long t;
    asm ("fpmerge	%H1, %H2, %0" : "=e" (t) : "f" (a), "f" (b));
    return t;
}
#define pextlbh(x) fpmerge_lo(fzero(), x)
#define pexthbh(x) fpmerge_hi(fzero(), x)

/* ========================================================================= */
local inline unsigned long long fpadd32(unsigned long long a, unsigned long long b)
{
    unsigned long long t;
    asm ("fpadd32	%1, %2, %0" : "=e" (t) : "%e" (a), "e" (b));
    return t;
}

/* ========================================================================= */
local inline unsigned long long fpadd16(unsigned long long a, unsigned long long b)
{
    unsigned long long t;
    asm ("fpadd16	%1, %2, %0" : "=e" (t) : "%e" (a), "e" (b));
    return t;
}

/* ========================================================================= */
local inline unsigned long long fpsub32(unsigned long long a, unsigned long long b)
{
    unsigned long long t;
    asm ("fpsub32	%1, %2, %0" : "=e" (t) : "e" (a), "e" (b));
    return t;
}

/* ========================================================================= */
local inline unsigned long long pdist(unsigned long long a, unsigned long long b, unsigned long long acc)
{
    asm ("pdist	%1, %2, %0" : "=e" (acc) : "e" (a), "e" (b), "0" (acc));
    return acc;
}

/* ========================================================================= */
local inline unsigned long long fmuld8ulx16_lo(unsigned long long a, unsigned long long b)
{
    unsigned long long t;
    asm ("fmuld8ulx16	%L1, %L2, %0" : "=e" (t) : "f" (a), "f" (b));
    return t;
}

/* ========================================================================= */
local inline unsigned long long fmuld8ulx16_hi(unsigned long long a, unsigned long long b)
{
    unsigned long long t;
    asm ("fmuld8ulx16	%H1, %H2, %0" : "=e" (t) : "f" (a), "f" (b));
    return t;
}

/* ========================================================================= */
local inline unsigned long long fmuld8sux16_lo(unsigned long long a, unsigned long long b)
{
    unsigned long long t;
    asm ("fmuld8sux16	%L1, %L2, %0" : "=e" (t) : "f" (a), "f" (b));
    return t;
}

/* ========================================================================= */
local inline unsigned long long fmuld8sux16_hi(unsigned long long a, unsigned long long b)
{
    unsigned long long t;
    asm ("fmuld8sux16	%H1, %H2, %0" : "=e" (t) : "f" (a), "f" (b));
    return t;
}

/* ========================================================================= */
local inline unsigned long long fpmul16x16x32_lo(unsigned long long a, unsigned long long b)
{
    unsigned long long r_l = fmuld8ulx16_lo(a, b);
    unsigned long long r_h = fmuld8sux16_lo(a, b);
    return fpadd32(r_l, r_h);
}

/* ========================================================================= */
local inline unsigned long long fpmul16x16x32_hi(unsigned long long a, unsigned long long b)
{
    unsigned long long r_l = fmuld8ulx16_hi(a, b);
    unsigned long long r_h = fmuld8sux16_hi(a, b);
    return fpadd32(r_l, r_h);
}

/* ========================================================================= */
local inline unsigned long long fpand(unsigned long long a, unsigned long long b)
{
    unsigned long long t;
    asm ("fand	%1, %2, %0" : "=e" (t) : "e" (a), "e" (b));
    return t;
}

/* ========================================================================= */
local inline unsigned long long fpandnot2(unsigned long long a, unsigned long long b)
{
    unsigned long long t;
    asm ("fandnot2	%1, %2, %0" : "=e" (t) : "e" (a), "e" (b));
    return t;
}

/* ========================================================================= */
local inline void *alignaddr(const void *ptr, int off)
{
    void *t;
    asm volatile ("alignaddr	%1, %2, %0" : "=r" (t) : "r" (ptr), "r" (off));
    return t;
}

/* ========================================================================= */
local inline void *alignaddrl(const void *ptr, int off)
{
    void *t;
    asm volatile ("alignaddrl	%1, %2, %0" : "=r" (t) : "r" (ptr), "r" (off));
    return t;
}

/* ========================================================================= */
local inline unsigned long long faligndata(unsigned long long a, unsigned long long b)
{
    unsigned long long ret;
    asm volatile ("faligndata	%1, %2, %0" : "=e" (ret) : "e" (a), "e" (b));
    return ret;
}

/* ========================================================================= */
local inline unsigned long long vector_chop(unsigned long long x)
{
    unsigned long long mask = 0x0000ffff0000ffffull;
    unsigned long long y = fpand(x, mask);
    x = fpandnot2(x, mask);
    x >>= 16;
    y = fpsub32(y, x);
    x = fpadd32(x, x); /* << 1 */
    x = fpadd32(x, x); /* << 1 */
    x = fpadd32(x, x); /* << 1 */
    x = fpadd32(x, x); /* << 1 */
    return fpadd32(x, y);
}

/* ========================================================================= */
local noinline uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1, s2;

    s1 = adler & 0xffff;
    s2 = (adler >> 16) & 0xffff;

    if (likely(len >= 2 * SOVV)) {
        unsigned long long vorder_lo = 0x0004000300020001ULL;
        unsigned long long vorder_hi = 0x0008000700060005ULL;
        unsigned long long vs1, vs2;
        unsigned long long v0 = fzero();
        unsigned long long in;
        const unsigned char *o_buf;
        unsigned int k, f, n;

        /* align hard down */
        f = (unsigned int)ALIGN_DOWN_DIFF(buf, SOVV);
        n = SOVV - f;

        /* add n times s1 to s2 for start round */
        s2 += s1 * n;

        /* insert scalar start somehwere */
        vs1 = s1;
        vs2 = s2;

        k = len < VNMAX ? len : VNMAX;
        len -= k;

        /* get input data */
        o_buf = buf;
        buf = alignaddr(buf, 0);
        in = *(const unsigned long long *)buf;
        {
            unsigned long long vs2t;
            /* swizzle data in place */
            in = faligndata(in, v0);
            if(f) {
                alignaddrl(o_buf, 0);
                in = faligndata(v0, in);
            }
            /* add horizontal and acc */
            vs1 = pdist(in, v0, vs1);
            /* extract, apply order and acc */
            vs2t = pextlbh(in);
            vs2 = fpadd32(vs2, fpmul16x16x32_lo(vs2t, vorder_lo));
            vs2 = fpadd32(vs2, fpmul16x16x32_hi(vs2t, vorder_lo));
            vs2t = pexthbh(in);
            vs2 = fpadd32(vs2, fpmul16x16x32_lo(vs2t, vorder_hi));
            vs2 = fpadd32(vs2, fpmul16x16x32_hi(vs2t, vorder_hi));
        }
        buf += SOVV;
        k   -= n;

        if (likely(k >= SOVV)) do {
            unsigned long long vs1_r = fzero();
            do {
                unsigned long long vs2l = fzero(), vs2h = fzero();
                unsigned j;

                j = (k/SOVV) > 127 ? 127 : k/SOVV;
                k -= j * SOVV;
                do {
                    /* get input data */
                    in = *(const unsigned long long *)buf;
                    buf += SOVV;
                    /* add vs1 for this round */
                    vs1_r = fpadd32(vs1_r, vs1);
                    /* add horizontal and acc */
                    vs1 = pdist(in, v0, vs1);
                    /* extract */
                    vs2l = fpadd16(vs2l, pextlbh(in));
                    vs2h = fpadd16(vs2h, pexthbh(in));
                } while (--j);
                vs2 = fpadd32(vs2, fpmul16x16x32_lo(vs2l, vorder_lo));
                vs2 = fpadd32(vs2, fpmul16x16x32_hi(vs2l, vorder_lo));
                vs2 = fpadd32(vs2, fpmul16x16x32_lo(vs2h, vorder_hi));
                vs2 = fpadd32(vs2, fpmul16x16x32_hi(vs2h, vorder_hi));
            } while (k >= SOVV);
            /* chop vs1 round sum before multiplying by 8 */
            vs1_r = vector_chop(vs1_r);
            /* add vs1 for this round (8 times) */
            vs1_r = fpadd32(vs1_r, vs1_r); /* *2 */
            vs1_r = fpadd32(vs1_r, vs1_r); /* *4 */
            vs1_r = fpadd32(vs1_r, vs1_r); /* *8 */
            vs2   = fpadd32(vs2, vs1_r);
            /* chop both sums */
            vs2 = vector_chop(vs2);
            vs1 = vector_chop(vs1);
            len += k;
            k = len < VNMAX ? len : VNMAX;
            len -= k;
        } while (likely(k >= SOVV));

        if (likely(k)) {
            /* handle trailer */
            unsigned long long t, r, vs2t;
            unsigned int k_m;

            /* get input data */
            in = *(const unsigned long long *)buf;

            /* swizzle data in place */
            alignaddr(buf, k);
            in = faligndata(v0, in);

            /* add k times vs1 for this trailer */
            /* since VIS muls are painfull, use the russian peasant method, k is small */
            t = 0;
            r = vs1;
            k_m = k;
            do {
                if(k_m & 1)
                    t = fpadd32(t, r); /* add to result if odd */
                r = fpadd32(r, r); /* *2 */
                k_m >>= 1; /* /2 */
            } while (k_m);
            vs2 = fpadd32(vs2, t);

            /* add horizontal and acc */
            vs1 = pdist(in, v0, vs1);
            /* extract, apply order and acc */
            vs2t = pextlbh(in);
            vs2  = fpadd32(vs2, fpmul16x16x32_lo(vs2t, vorder_lo));
            vs2  = fpadd32(vs2, fpmul16x16x32_hi(vs2t, vorder_lo));
            vs2t = pexthbh(in);
            vs2  = fpadd32(vs2, fpmul16x16x32_lo(vs2t, vorder_hi));
            vs2  = fpadd32(vs2, fpmul16x16x32_hi(vs2t, vorder_hi));

            buf += k;
            k   -= k;
        }
        /* vs1 is one giant 64 bit sum, but we only calc to 32 bit */
        s1 = vs1;
        /* add both vs2 sums */
        s2 = (vs2 & 0xFFFFFFFFUL) + (vs2 >> 32);
    }

    if (unlikely(len)) do {
        s1 += *buf++;
        s2 += s1;
    } while (--len);
    /* at this point we should not have so big s1 & s2 */
    MOD28(s1);
    MOD28(s2);
    /* return recombined sums */
    return (s2 << 16) | s1;
}
#endif
