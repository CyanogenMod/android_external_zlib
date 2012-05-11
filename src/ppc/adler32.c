/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   ppc implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2009-2012 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * We use the Altivec PIM vector stuff, but still, this is only
 * tested with GCC, and prop. uses some GCC specifics (like GCC
 * understands vector types and you can simply write a += b)
 */
#if defined(__ALTIVEC__) && defined(__GNUC__)
# define HAVE_ADLER32_VEC
/* it needs some bytes till the vec version gets up to speed... */
local noinline uLong adler32_ge16(uLong adler, const Bytef *buf, uInt len);
# define MIN_WORK 64
# include <altivec.h>

/*
 * Depending on length, this can be slower (short length < 64 bytes),
 * much faster (our beloved 128kb 22.2s generic to 3.4s vec, but cache
 * is important...), to a little faster (very long length, 1.6MB, 47.6s
 * to 36s), which is prop. only capped by memory bandwith.
 * (The orig. 128k case was slower in AltiVec, because AltiVec loads
 * are always uncached and trigger no HW prefetching, because that is
 * what you often need with mass data manipulation (not poisen your
 * cache, movntq), instead you have to do it for yourself (data stream
 * touch). With 128k it could be cleanly seen: no prefetch, half as slow
 * as generic, but comment out the memory load -> 3s. With proper prefetch
 * we are at 3.4s. So AltiVec can execute these "expensive" FMA quite
 * fast (even without fancy unrolling), only the data does not arrive
 * fast enough. In cases where the working set does not fit into cache
 * it simply cannot be delivered fast enough over the FSB/Mem).
 * Still we have to prefetch, or we are slow as hell.
 */
/*
 * Modern ultra SMT/SMP POWER processors (the BigIron ones) are better
 * at mem bandwidth and do not need prefetching (those instructions are
 * to be seen as nops, says the ISA doc).
 * But they are _not_ OutOfOrder Execution CPUs (lots of small, high
 * freq. "simple" cores). So unrolling with independent sums to prevent
 * pipeline stalls is necessary.
 * This also helps the OOOE CPUs a little bit, but was not crucial up
 * till now.
 *
 * The result is a breath taking speedup of 11.5 on a POWER7@3.55GHz
 * compared to the original adler32.
 */
# define SOVUC (sizeof(vector unsigned char))

/* lot of independent sums, lots of bytes can be accumulated */
# define VNMAX (28*NMAX)

/* ========================================================================= */
local inline vector unsigned char vec_identl(level)
    unsigned int level;
{
    return vec_lvsl(level, (const unsigned char *)0);
}

/* ========================================================================= */
local inline vector unsigned char vec_ident_rev(void)
{
    return vec_xor(vec_identl(0), vec_splat_u8(15));
}

/* ========================================================================= */
/* multiply two 32 bit ints, return the low 32 bit */
local inline vector unsigned int vec_mullw(vector unsigned int a, vector unsigned int b)
{
    vector unsigned int v16   = vec_splat_u32(-16);
    vector unsigned int v0_32 = vec_splat_u32(0);
    vector unsigned int swap, low, high;

    swap = vec_rl(b, v16);
     low = vec_mulo((vector unsigned short)a, (vector unsigned short)b);
    high = vec_msum((vector unsigned short)a, (vector unsigned short)swap, v0_32);
    high = vec_sl(high, v16);
    return vec_add(low, high);
}

/* ========================================================================= */
local inline vector unsigned int vector_chop(vector unsigned int x)
{
    vector unsigned int y;
    vector unsigned int vsh;

    vsh = vec_splat_u32(1);
    vsh = vec_sl(vsh, vec_splat_u32(4));

    y = vec_sl(x, vsh);
    y = vec_sr(y, vsh);
    x = vec_sr(x, vsh);
    y = vec_sub(y, x);
    x = vec_sl(x, vec_splat_u32(4));
    x = vec_add(x, y);
    return x;
}

/* ========================================================================= */
local inline vector unsigned int vector_load_one_u32(unsigned int x)
{
    vector unsigned int val = vec_lde(0, &x);
    vector unsigned char vperm, mask;

     mask = (vector unsigned char)vec_cmpgt(vec_identl(0), vec_splat_u8(3));
    vperm = (vector unsigned char)vec_splat((vector unsigned int)vec_lvsl(0, &x), 0);
      val = vec_perm(val, val, vperm);
      val = vec_sel(val, vec_splat_u32(0), (vector unsigned int)mask);
    return val;
}

/* ========================================================================= */
/*
 * PPC hates conditional branches.
 * Unfotunatly a cmov is not in the ISA, or brand new in POWER7
 * (and GCC is not generating it...).
 * So a little tricky bit magic is done.
 */
local inline unsigned int isel(int a, unsigned int x, unsigned int y)
{
    /*
     * If the right shift of a signed number is an arithmetic
     * shift in C is implementation defined.
     * Since PPC is a 2-complement arch and has the right
     * instructions to do arith. shifts, a compiler which
     * does not generate an arithmetic shift in the following
     * line can be considered ... obsolet.
     * On the other hand with a little luck a compiler can
     * change this back to isel or some subc pattern.
     */
    int mask = a >> 31; /* create mask of 0 or 0xffffffff */
    /* if a >= 0 return x, else y */
    return x + ((y - x) & mask);
}

/* ========================================================================= */
local noinline uLong adler32_vec_real(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    vector unsigned char vord[4];
    vector unsigned int vs1, vs2;
    unsigned int f, n;
    unsigned int k;
    unsigned int s1, s2;

    s1 = adler & 0xffff;
    s2 = (adler >> 16) & 0xffff;

    /*
     * start by firing of prefetches.
     * Try to not prefetch to much, maybe it is not such
     * a large len.
     * prefetching will stop at page bounderies, because
     * the prefetch engine does not know the virtual<>real
     * address mapping of the next page.
     * assume 4k pages
     */
    f = ALIGN_DIFF(buf, 4096);
    if(len > f) {
        unsigned int block_size = 1, block_num = DIV_ROUNDUP(f, 16);
        /* prefetch rest of page */
        while(block_num >= 256)
            block_size *= 2, block_num /= 2;
        f  = 8 << block_size; /* stride */
        f |= block_num << 16;
        f |= block_size << 24;
        vec_dstt(buf, f, 2);
        /* prefetch next page */
        block_size = 1;
        n = len - f > 4096 ? 4096 : len - f;
        block_num = DIV_ROUNDUP(n, 16);
        while(block_num >= 256)
            block_size *= 2, block_num /= 2;
        f  = 8 << block_size; /* stride */
        f |= block_num << 16;
        f |= block_size << 24;
        vec_dstt(buf, f, 1);
    } else {
        unsigned int block_size = 1, block_num = DIV_ROUNDUP(len, 16);
        while(block_num >= 256)
            block_size *= 2, block_num /= 2;
        f  = 8 << block_size; /* stride */
        f |= block_num << 16;
        f |= block_size << 24;
        vec_dstt(buf, f, 1);
    }
    /*
     * if i understand the Altivec PEM right, little
     * endian impl. should have the data reversed on
     * load, so the big endian vorder works.
     */
    {
        vector unsigned char v1 = vec_splat_u8(1), vt;
        vord[0] = vec_ident_rev() + v1;
             vt = vec_sl(vec_splat_u8(8), v1);
        vord[1] = vec_add(vord[0], vt);
        vord[2] = vec_add(vord[1], vt);
        vord[3] = vec_add(vord[2], vt);
    }

    /* align hard down */
    f = (unsigned) ALIGN_DOWN_DIFF(buf, SOVUC);
    n = SOVUC - f;
    buf = (const unsigned char *)ALIGN_DOWN(buf, SOVUC);

    /* add n times s1 to s2 for start round */
    s2 += s1 * n;

    k = len < VNMAX ? (unsigned)len : VNMAX;
    len -= k;

    /* insert scalar start somewhere */
    vs1 = vector_load_one_u32(s1);
    vs2 = vector_load_one_u32(s2);

    {
        vector unsigned char v0 = vec_splat_u8(0);
        vector unsigned char in16, vperm;

        /* get input data */
         in16 = vec_ldl(0, buf);
        /* Mask out exess data to achieve alignment */
        vperm = vec_lvsl(f, buf);
         in16 = vec_perm(in16, v0, vperm);
        vperm = vec_lvsr(f, buf);
         in16 = vec_perm(v0, in16, vperm);
        /* add 4 byte horizontal and add to old dword */
          vs1 = vec_sum4s(in16, vs1);
        /* apply order, add 4 byte horizontal and add to old dword */
          vs2 = vec_msum(in16, vord[0], vs2);
    }

    buf += SOVUC;
      k -= n;
    if (likely(k >= SOVUC)) do {
        if (k >= 8*SOVUC) {
            /* if length is larger, do it unrolled 4 times */
            vector unsigned int vsh4;
            vector unsigned int vs1_rm[4];
            vector unsigned int vs1m[4];
            vector unsigned int vs2m[4];
            const unsigned char *t = (const unsigned char *) ALIGN_DOWN(buf + 4096, 4096);

            vs1_rm[0] = vs1_rm[1] = vs1_rm[2] = vs1_rm[3] = vec_splat_u32(0);
            vs1m[0] = vs1;
            vs1m[1] = vs1m[2] = vs1m[3] = vs1_rm[0];
            vs2m[0] = vs2m[1] = vs2m[2] = vs1_rm[0];
            vs2m[3] = vs2;
            do {
                unsigned int i = isel(k - 8192, 8192, k);

                f  = 512;
                f |= 8 << 16;
                /* prefetch the next two pages */
#ifdef __GNUC__
                /* gcc optimizes the dstt away, prop because he already sees
                 * one above ... not good */
                __asm__ __volatile__("dstt %0, %1, %2" : : "r" (t), "r" (f), "i" (2));
                __asm__ __volatile__("dstt %0, %1, %2" : : "r" (t+4096), "r" (f), "i" (1));
#else
                vec_dstt(t, f, 2);
                vec_dstt(t+4096, f, 1);
#endif
                t += 8192;
                i /= 4*SOVUC;
                k -= i * 4*SOVUC;
                do {
                    vector unsigned char in16m[4];
                    /* get input data */
                     in16m[0] = vec_ldl(0*SOVUC, buf);
                     in16m[1] = vec_ldl(1*SOVUC, buf);
                     in16m[2] = vec_ldl(2*SOVUC, buf);
                     in16m[3] = vec_ldl(3*SOVUC, buf);

                    /* add vs1 for this round */
                    vs1_rm[0] = vec_add(vs1_rm[0], vs1m[0]);
                    vs1_rm[1] = vec_add(vs1_rm[1], vs1m[1]);
                    vs1_rm[2] = vec_add(vs1_rm[2], vs1m[2]);
                    vs1_rm[3] = vec_add(vs1_rm[3], vs1m[3]);

                    /* add 4 byte horizontal and add to old dword */
                      vs1m[0] = vec_sum4s(in16m[0], vs1m[0]);
                      vs1m[1] = vec_sum4s(in16m[1], vs1m[1]);
                      vs1m[2] = vec_sum4s(in16m[2], vs1m[2]);
                      vs1m[3] = vec_sum4s(in16m[3], vs1m[3]);
                    /* apply order, add 4 byte horizontal and add to old dword */
                      vs2m[0] = vec_msum(in16m[0], vord[3], vs2m[0]);
                      vs2m[1] = vec_msum(in16m[1], vord[2], vs2m[1]);
                      vs2m[2] = vec_msum(in16m[2], vord[1], vs2m[2]);
                      vs2m[3] = vec_msum(in16m[3], vord[0], vs2m[3]);

                    buf += 4*SOVUC;
                } while (--i);
            } while (k >= 4*SOVUC);
            /* chop vs1 round sum before multiplying by 64 */
            vs1_rm[0] = vector_chop(vs1_rm[0]);
            vs1_rm[1] = vector_chop(vs1_rm[1]);
            vs1_rm[2] = vector_chop(vs1_rm[2]);
            vs1_rm[3] = vector_chop(vs1_rm[3]);
            /* add all vs1 for 64 times */
            vsh4 = vec_splat_u32(6);
              vs2m[0] = vec_add(vs2m[0], vec_sl(vs1_rm[0], vsh4));
              vs2m[1] = vec_add(vs2m[1], vec_sl(vs1_rm[1], vsh4));
              vs2m[2] = vec_add(vs2m[2], vec_sl(vs1_rm[2], vsh4));
              vs2m[3] = vec_add(vs2m[3], vec_sl(vs1_rm[3], vsh4));
            /* chop the vectors to something in the range of BASE */
                  vs2 = vector_chop(vs2m[0] + vs2m[1] + vs2m[2] + vs2m[3]);
                  vs1 = vector_chop(vs1m[0] + vs1m[1] + vs1m[2] + vs1m[3]);
        } else {
            vector unsigned int vs1_r = vec_splat_u32(0), vsh;
            do {
                /* get input data */
                vector unsigned char in16 = vec_ldl(0, buf);

                /* add vs1 for this round */
                vs1_r = vec_add(vs1_r, vs1);
                /* add 4 byte horizontal and add to old dword */
                  vs1 = vec_sum4s(in16, vs1);
                /* apply order, add 4 byte horizontal and add to old dword */
                  vs2 = vec_msum(in16, vord[0], vs2);

                buf += SOVUC;
                  k -= SOVUC;
            } while (k >= SOVUC);
            /* chop vs1 round sum before multiplying by 16 */
            vs1_r = vector_chop(vs1_r);
            /* add all vs1 for 16 times */
              vsh = vec_splat_u32(4);
              vs2 = vec_add(vs2, vec_sl(vs1_r, vsh));
            /* chop the vectors to something in the range of BASE */
              vs2 = vector_chop(vs2);
              vs1 = vector_chop(vs1);
        }
        len += k;
           k = len < VNMAX ? (unsigned)len : VNMAX;
        len -= k;
    } while (likely(k >= SOVUC));

    vec_dss(1);
    vec_dss(2);

    if (likely(k)) {
        /* handle trailer */
        vector unsigned char in16, vperm;
        vector unsigned char   v0 = vec_splat_u8(0);
        /* transport k over to vec unit */
        vector unsigned int vk = (vector unsigned int)vec_lvsl(0, (unsigned *)(intptr_t)k);
        vk = (vector unsigned)vec_mergeh(v0, (vector unsigned char)vk);
        vk = (vector unsigned)vec_mergeh((vector unsigned short)v0, (vector unsigned short)vk);
        vk = vec_splat(vk, 0);
        /* add k times vs1 for this trailer */
        vs2 = vec_add(vs2, vec_mullw(vs1, vk));

        /* get input data */
         in16 = vec_ldl(0, buf);
        /* mask out excess data */
        vperm = vec_lvsr(0, (const unsigned char *)(SOVUC - k));
         in16 = vec_perm(v0, in16, vperm);
        /* add 4 byte horizontal and add to old dword */
          vs1 = vec_sum4s(in16, vs1);
        /* apply order, add 4 byte horizontal and add to old dword */
          vs2 = vec_msum(in16, vord[0], vs2);

        buf += k;
          k -= k;
    }

    /* add horizontal */
    /* stuff should be choped so no proplem with signed saturate */
    vs1 = (vector unsigned int)vec_sums((vector int)vs1, vec_splat_s32(0));
    vs2 = (vector unsigned int)vec_sums((vector int)vs2, vec_splat_s32(0));
    /* shake and roll */
    vs1 = vec_splat(vs1, 3);
    vs2 = vec_splat(vs2, 3);
    vec_ste(vs1, 0, &s1);
    vec_ste(vs2, 0, &s2);

    /* after horizontal add, modulo again in scalar code */
    MOD28(s1);
    MOD28(s2);

    return (s2 << 16) | s1;
}

/* ========================================================================= */
local noinline uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    /* Factor out short len handling, to get rid of one indent. While at it
       force GCC to create a lean stackframe for to short len by tailcall */
    if (unlikely(len < 2*SOVUC))
        return adler32_ge16(adler, buf, len);
    return adler32_vec_real(adler, buf, len);
}
#endif
