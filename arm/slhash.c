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

#if defined(__ARM_NEON__) && defined(__ARMEL__)
/*
 * Big endian NEON qwords are kind of broken.
 * They are big endian within the dwords, but WRONG
 * (really??) way round between lo and hi.
 * Creating some kind of PDP11 middle endian.
 *
 * This is madness and unsupportable. For this reason
 * GCC wants to disable qword endian specific patterns.
 */
#  include <arm_neon.h>

#  define SOVUSQ sizeof(uint16x8_t)
#  define SOVUS sizeof(uint16x4_t)
#  define HAVE_SLHASH_VEC

local void update_hoffset_l(Posf *p, uInt wsize, unsigned n)
{
    unsigned int i;
    uint32x4_t vwsize;

    vwsize = vdupq_n_u32(wsize);

    if (ALIGN_DOWN_DIFF(p, SOVUSQ)) {
        uint32x4_t in = vmovl_u16(*(uint16x4_t *)p);
        in  = vqsubq_u32(in, vwsize);
        *(uint16x4_t *)p = vmovn_u32(in);
        p += SOVUS/sizeof(*p);
        n -= SOVUS/sizeof(*p);
    }
    i  = n / (SOVUSQ/sizeof(*p));
    n %= SOVUSQ/sizeof(*p);

    do {
        uint16x8_t in8 = *(uint16x8_t *)p;
        uint32x4_t inl = vmovl_u16(vget_low_u16(in8));
        uint32x4_t inh = vmovl_u16(vget_high_u16(in8));
        inl = vqsubq_u32(inl, vwsize);
        inh = vqsubq_u32(inh, vwsize);
        in8 = vcombine_u16(vmovn_u32(inl), vmovn_u32(inh));
        *(uint16x8_t *)p = in8;
        p += SOVUSQ/sizeof(*p);
    } while (--i);

    if (n >= SOVUS/sizeof(*p)) {
        uint32x4_t in = vmovl_u16(*(uint16x4_t *)p);
        in  = vqsubq_u32(in, vwsize);
        *(uint16x4_t *)p = vmovn_u32(in);
        p += SOVUS/sizeof(*p);
        n -= SOVUS/sizeof(*p);
    }
    if (unlikely(n)) do {
        register unsigned m = *p;
        *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
    } while (--n);
}

local void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    unsigned int i;
    uint16x8_t vwsize;

    i  = ALIGN_DIFF(p, SOVUS)/sizeof(Pos);
    n -= i;
    if (unlikely(i)) do {
        register unsigned m = *p;
        *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
    } while (--i);

    if(unlikely(wsize > (1<<16)-1)) {
        update_hoffset_l(p, wsize, n);
        return;
    }
    vwsize = vdupq_n_u16(wsize);

    if (ALIGN_DOWN_DIFF(p, SOVUSQ)) {
        uint16x4_t in4 = *(uint16x4_t *)p;
        in4 = vqsub_u16(in4, vget_low_u16(vwsize));
        *(uint16x4_t *)p = in4;
        p += SOVUS/sizeof(*p);
        n -= SOVUS/sizeof(*p);
    }
    i  = n / (SOVUSQ/sizeof(*p));
    n %= SOVUSQ/sizeof(*p);
    do {
        *(uint16x8_t *)p = vqsubq_u16(*(uint16x8_t *)p, vwsize);
        p += SOVUSQ/sizeof(*p);
    } while (--i);

    if (n >= SOVUS/sizeof(*p)) {
        uint16x4_t in4 = *(uint16x4_t *)p;
        in4 = vqsub_u16(in4, vget_low_u16(vwsize));
        *(uint16x4_t *)p = in4;
        p += SOVUS/sizeof(*p);
        n -= SOVUS/sizeof(*p);
    }
    if (unlikely(n)) do {
        register unsigned m = *p;
        *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
    } while (--n);
}
#elif defined(__IWMMXT__)
#  define HAVE_SLHASH_VEC
#  ifndef __GNUC__
/* GCC doesn't take it's own intrinsic header and ICEs if forced to */
#    include <mmintrin.h>
#  else
typedef unsigned long long __m64;

local inline __m64 _mm_subs_pu16(__m64 a, __m64 b)
{
    __m64 ret;
    asm ("wsubhus %0, %1, %2" : "=y" (ret) : "y" (a), "y" (b));
    return ret;
}
#  endif
#  define SOV4 (sizeof(__m64))

/* ========================================================================= */
local inline void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    if (likely(wsize < (1<<16))) {
        unsigned int i, f, wst;
        __m64 vwsize;

        wst = (wsize  << 16) | (wsize  & 0x0000ffff);
        vwsize = (__m64)(((unsigned long long)wst  << 32) | wst);
        /* align */
        f = (unsigned)ALIGN_DIFF(p, SOV4);
        if (unlikely(f)) {
            f /= sizeof(*p);
            n -= f;
            do {
                register unsigned m = *p;
                *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
            } while (--f);
        }

        /* do it */
        i  = n / (SOV4/sizeof(*p));
        n %= SOV4/sizeof(*p);
        if (i & 1) {
            __m64 x = _mm_subs_pu16(*(__m64 *)p, vwsize);
            *(__m64 *)p = x;
            p += SOV4/sizeof(*p);
            i--;
        }
        i /= 2;
        do {
            __m64 x1, x2;
            x1 = ((__m64 *)p)[0];
            x2 = ((__m64 *)p)[1];
            x1 = _mm_subs_pu16(x1, vwsize);
            x2 = _mm_subs_pu16(x2, vwsize);
            ((__m64 *)p)[0] = x1;
            ((__m64 *)p)[1] = x2;
            p += 2*(SOV4/sizeof(*p));
        } while (--i);

        /* handle trailer */
        if (unlikely(n)) {
            do {
                register unsigned m = *p;
                *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
            } while (--n);
        }
    } else {
        do {
            register unsigned m = *p;
            *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
        } while (--n);
    }
}
#elif defined(__GNUC__) && ( \
        defined(__thumb2__)  && ( \
            !defined(__ARM_ARCH_7__) && !defined(__ARM_ARCH_7M__) \
        ) || ( \
        !defined(__thumb__) && ( \
            defined(__ARM_ARCH_6__)   || defined(__ARM_ARCH_6J__)  || \
            defined(__ARM_ARCH_6T2__) || defined(__ARM_ARCH_6ZK__) || \
            defined(__ARM_ARCH_7A__)  || defined(__ARM_ARCH_7R__) \
        )) \
    )
#  define SOU32 (sizeof(unsigned int))
#  define HAVE_SLHASH_VEC

local noinline void update_hoffset_l(Posf *p, uInt wsize, unsigned n)
{
    do {
        register unsigned m = *p;
        *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
    } while (--n);
}

local void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    unsigned int i, vwsize;

    if(unlikely(wsize > (1<<16)-1)) {
        update_hoffset_l(p, wsize, n);
        return;
    }

    vwsize = wsize | (wsize << 16);
    if (ALIGN_DOWN_DIFF(p, SOU32)) {
        register unsigned m = *p;
        *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
        n--;
    }

    i  = n / (SOU32/sizeof(*p));
    n %= SOU32/sizeof(*p);
    if (i >= 4) {
        unsigned int j = i / 4;
        i %= 4;
        asm (
            "1:\n\t"
            "subs %1, #1\n\t"
            "ldmia %0, {r4-r7}\n\t"
            "uqsub16 r4, r4, %2\n\t"
            "uqsub16 r5, r5, %2\n\t"
            "uqsub16 r6, r6, %2\n\t"
            "uqsub16 r7, r7, %2\n\t"
            "stmia %0!, {r4-r7}\n\t"
            "bne 1b"
            : /* 0 */ "=r" (p),
              /* 1 */ "=r" (j)
            : /* 2 */ "r" (vwsize),
              /*  */ "0" (p),
              /*  */ "1" (j)
            : "r4", "r5", "r6", "r7"
        );
            unsigned int in = *(unsigned int *)p;
            *(unsigned int *)p = in;
    }
    if (i) do {
        unsigned int in = *(unsigned int *)p;
        asm ("uqsub16 %0, %1, %2" : "=r" (in) : "r" (in), "r" (vwsize));
        *(unsigned int *)p = in;
        p += SOU32/sizeof(*p);
    } while (--i);

    if (unlikely(n)) {
        register unsigned m = *p;
        *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
    }
}

#endif
