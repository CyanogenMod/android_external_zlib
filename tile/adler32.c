/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   Tile implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#if defined(__GNUC__)
#  if defined(__tilegx__)
#    define HAVE_ADLER32_VEC
#    define MIN_WORK 32
// TODO: VNMAX could prop. be a little higher?
#    define VNMAX (2*NMAX+((9*NMAX)/10))

#    define SOUL (sizeof(unsigned long))

/* ========================================================================= */
local inline unsigned long v1ddotpua(unsigned long d, unsigned long a, unsigned long b)
{
    return __insn_v1ddotpua(d, a, b);
}

/* ========================================================================= */
local inline unsigned long v4shl(unsigned long a, unsigned long b)
{
    return __insn_v4shl(a, b);
}

/* ========================================================================= */
local inline unsigned long v4shru(unsigned long a, unsigned long b)
{
    return __insn_v4shru(a, b);
}

/* ========================================================================= */
local inline unsigned long v4sub(unsigned long a, unsigned long b)
{
    return __insn_v4sub(a, b);
}

/* ========================================================================= */
local inline unsigned long v4add(unsigned long a, unsigned long b)
{
    return __insn_v4add(a, b);
}

/* ========================================================================= */
local inline unsigned long vector_chop(unsigned long x)
{
    unsigned long y;

    y = v4shl(x, 16);
    y = v4shru(y, 16);
    x = v4shru(x, 16);
    y = v4sub(y, x);
    x = v4shl(x, 4);
    x = v4add(x, y);
    return x;
}

/* ========================================================================= */
local noinline uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1, s2;
    unsigned k;

    /* split Adler-32 into component sums */
    s1 = adler & 0xffff;
    s2 = (adler >> 16) & 0xffff;

    /* align input */
    k    = ALIGN_DIFF(buf, SOUL);
    len -= k;
    if (k) do {
        s1 += *buf++;
        s2 += s1;
    } while (--k);

    k = len < VNMAX ? len : VNMAX;
    len -= k;
    if (likely(k >= 2 * SOUL)) {
        const unsigned long v1 = 0x0101010101010101ul;
        unsigned long vs1 = s1, vs2 = s2;
        unsigned long vorder;

        if(host_is_bigendian())
            vorder = 0x0807060504030201;
        else
            vorder = 0x0102030405060708;

        do {
            unsigned long vs1_r = 0;
            do {
                /* get input data */
                unsigned long in = *(const unsigned long *)buf;
                /* add vs1 for this round */
                vs1_r += vs1; /* we don't overflow, so normal add */
                /* add horizontal & acc vs1 */
                vs1 = v1ddotpua(vs1, in, v1); /* only into x0 pipeline */
                /* mul, add & acc vs2 */
                vs2 = v1ddotpua(vs2, in, vorder); /* only in x0 pipe */
                buf += SOUL;
                k -= SOUL;
            } while (k >= SOUL);
            /* chop vs1 round sum before multiplying by 8 */
            vs1_r = vector_chop(vs1_r);
            /* add vs1 for this round (8 times) */
            vs1_r = v4shl(vs1_r, 3);
            vs2 += vs1_r;
            /* chop both sums */
            vs2 = vector_chop(vs2);
            vs1 = vector_chop(vs1);
            len += k;
            k = len < VNMAX ? len : VNMAX;
            len -= k;
        } while (likely(k >= SOUL));
        s1 = (vs1 & 0xffffffff) + (vs1 >> 32);
        s2 = (vs2 & 0xffffffff) + (vs2 >> 32);
    }

    /* handle trailer */
    if (unlikely(k)) do {
        s1 += *buf++;
        s2 += s1;
    } while (--k);
    /* at this point s1 & s2 should be in range */
    MOD28(s1);
    MOD28(s2);

    /* return recombined sums */
    return (s2 << 16) | s1;
}
#  else
#    define HAVE_ADLER32_VEC
#    define MIN_WORK 32
// TODO: VNMAX could be higher
#    define VNMAX NMAX

#    define SOUL (sizeof(unsigned long))

/* ========================================================================= */
local inline unsigned long sadab_u(unsigned long d, unsigned long a, unsigned long b)
{
    return __insn_sadab_u(d, a, b);
}

/* ========================================================================= */
local inline unsigned long inthb(unsigned long a, unsigned long b)
{
    return __insn_inthb(a, b);
}

/* ========================================================================= */
local inline unsigned long intlb(unsigned long a, unsigned long b)
{
    return __insn_intlb(a, b);
}

/* ========================================================================= */
local noinline uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1, s2;
    unsigned k;

    /* split Adler-32 into component sums */
    s1 = adler & 0xffff;
    s2 = (adler >> 16) & 0xffff;

    /* align input */
    k    = ALIGN_DIFF(buf, SOUL);
    len -= k;
    if (k) do {
        s1 += *buf++;
        s2 += s1;
    } while (--k);

    k = len < VNMAX ? len : VNMAX;
    len -= k;
    if (likely(k >= 2 * SOUL)) {
        unsigned long vs1 = s1, vs2 = s2;

        do {
            unsigned long vs1_r = 0;
            do {
                unsigned long a, b, c, d;
                unsigned long vs2l = 0, vs2h = 0;
                unsigned j;

                j = k > 257 * SOUL ? 257 : k/SOUL;
                k -= j * SOUL;
                do {
                    /* get input data */
                    unsigned long in = *(const unsigned long *)buf;
                    /* add vs1 for this round */
                    vs1_r += vs1;
                    /* add horizontal */
                    vs1 = sadab_u(vs1, in, 0);
                    /* extract */
                    vs2l += intlb(0, in);
                    vs2h += inthb(0, in);
                    buf += SOUL;
                } while (--j);
                /* split vs2 */
                if(host_is_bigendian()) {
                    a = (vs2h >> 16) & 0x0000ffff;
                    b = (vs2h      ) & 0x0000ffff;
                    c = (vs2l >> 16) & 0x0000ffff;
                    d = (vs2l      ) & 0x0000ffff;
                } else {
                    a = (vs2l      ) & 0x0000ffff;
                    b = (vs2l >> 16) & 0x0000ffff;
                    c = (vs2h      ) & 0x0000ffff;
                    d = (vs2h >> 16) & 0x0000ffff;
                }
                /* mull&add vs2 horiz. */
                vs2 += 4*a + 3*b + 2*c + 1*d;
            } while (k >= SOUL);
            /* reduce vs1 round sum before multiplying by 4 */
            CHOP(vs1_r);
            /* add vs1 for this round (4 times) */
            vs2 += vs1_r * 4;
            /* reduce both sums */
            CHOP(vs2);
            CHOP(vs1);
            len += k;
            k = len < VNMAX ? len : VNMAX;
            len -= k;
        } while (likely(k >= SOUL));
        s1 = vs1;
        s2 = vs2;
    }

    /* handle trailer */
    if (unlikely(k)) do {
        s1 += *buf++;
        s2 += s1;
    } while (--k);
    /* at this point we should not have so big s1 & s2 */
    MOD28(s1);
    MOD28(s2);

    /* return recombined sums */
    return (s2 << 16) | s1;
}
#  endif
#endif
