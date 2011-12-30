/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   ppc implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2009-2011 Jan Seiffert
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

# define SOVUC (sizeof(vector unsigned char))

/* can be propably more, since we do not have the x86 psadbw 64 bit sum */
# define VNMAX (6*NMAX)

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
    low  = vec_mulo((vector unsigned short)a, (vector unsigned short)b);
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
local noinline uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1, s2;

    s1 = adler & 0xffff;
    s2 = (adler >> 16) & 0xffff;

    if (likely(len >= 2*SOVUC)) {
        vector unsigned int   vsh = vec_splat_u32(4);
        vector unsigned char   v1 = vec_splat_u8(1);
        vector unsigned char vord;
        vector unsigned char   v0 = vec_splat_u8(0);
        vector unsigned int vs1, vs2;
        vector unsigned char in16, vord_a, v1_a, vperm;
        unsigned int f, n;
        unsigned int k, block_num;

        /*
         * if i understand the Altivec PEM right, little
         * endian impl. should have the data reversed on
         * load, so the big endian vorder works.
         */
        vord = vec_ident_rev() + v1;
        block_num = DIV_ROUNDUP(len, 512); /* 32 block size * 16 bytes */
        f  = 512;
        f |= block_num >= 256 ? 0 : block_num << 16;
        vec_dst(buf, f, 2);
        /*
         * Add stuff to achieve alignment
         */
        /* swizzle masks in place */
        vperm  = vec_lvsl(0, buf);
        vord_a = vec_perm(vord, v0, vperm);
        v1_a   = vec_perm(v1, v0, vperm);
        vperm  = vec_lvsr(0, buf);
        vord_a = vec_perm(v0, vord_a, vperm);
        v1_a   = vec_perm(v0, v1_a, vperm);

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

        /* get input data */
        in16 = vec_ldl(0, buf);

        /* mask out excess data, add 4 byte horizontal and add to old dword */
        vs1 = vec_msum(in16, v1_a, vs1);

        /* apply order, masking out excess data, add 4 byte horizontal and add to old dword */
        vs2 = vec_msum(in16, vord_a, vs2);

        buf += SOVUC;
        k -= n;

        if (likely(k >= SOVUC)) do {
            vector unsigned int vs1_r = vec_splat_u32(0);
            f  = 512;
            f |= block_num >= 256 ? 0 : block_num << 16;
            vec_dst(buf, f, 2);

            do {
                /* get input data */
                in16 = vec_ldl(0, buf);

                /* add vs1 for this round */
                vs1_r += vs1;

                /* add 4 byte horizontal and add to old dword */
                vs1 = vec_sum4s(in16, vs1);
                /* apply order, add 4 byte horizontal and add to old dword */
                vs2 = vec_msum(in16, vord, vs2);

                buf += SOVUC;
                k -= SOVUC;
            } while (k >= SOVUC);
            /* chop vs1 round sum before multiplying by 16 */
            vs1_r = vector_chop(vs1_r);
            /* add all vs1 for 16 times */
            vs2 += vec_sl(vs1_r, vsh);
            /* chop the vectors to something in the range of BASE */
            vs2 = vector_chop(vs2);
            vs1 = vector_chop(vs1);
            len += k;
            k = len < VNMAX ? (unsigned)len : VNMAX;
            block_num = DIV_ROUNDUP(len, 512); /* 32 block size * 16 bytes */
            len -= k;
        } while (likely(k >= SOVUC));

        if (likely(k)) {
            vector unsigned int vk;
            /*
             * handle trailer
             */
            f = SOVUC - k;
            /* swizzle masks in place */
            vperm  = vec_identl(f);
            vord_a = vec_perm(vord, v0, vperm);
            v1_a   = vec_perm(v1, v0, vperm);

            /* add k times vs1 for this trailer */
            vk = (vector unsigned int)vec_lvsl(0, (unsigned *)(intptr_t)k);
            vk = (vector unsigned)vec_mergeh(v0, (vector unsigned char)vk);
            vk = (vector unsigned)vec_mergeh((vector unsigned short)v0, (vector unsigned short)vk);
            vk = vec_splat(vk, 0);
            vs2 += vec_mullw(vs1, vk);

            /* get input data */
            in16 = vec_ldl(0, buf);

            /* mask out excess data, add 4 byte horizontal and add to old dword */
            vs1 = vec_msum(in16, v1_a, vs1);
            /* apply order, masking out excess data, add 4 byte horizontal and add to old dword */
            vs2 = vec_msum(in16, vord_a, vs2);

            buf += k;
            k -= k;
        }

        vec_dss(2);

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
