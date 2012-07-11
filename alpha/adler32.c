/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   alpha implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2010-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#if defined(__GNUC__) && defined(__alpha_max__)
#  define HAVE_ADLER32_VEC
#  define MIN_WORK 32
#  define VNMAX (2*NMAX+((9*NMAX)/10))

#  define SOUL (sizeof(unsigned long))

#  if GCC_VERSION_GE(303)
#    define unpkbw  __builtin_alpha_unpkbw
#    define perr    __builtin_alpha_perr
#  else
/* ========================================================================= */
local inline unsigned long unpkbw(unsigned long a)
{
    unsigned long r;
    __asm__ (".arch ev6; unpkbw	%r1, %0" : "=r" (r) : "rJ" (a));
    return r;
}

/* ========================================================================= */
local inline unsigned long perr(unsigned long a, unsigned long b)
{
    unsigned long r;
    __asm__ (".arch ev6; perr	%r1, %r2, %0" : "=r" (r) : "rJ" (a), "rJ" (b));
    return r;
}
#  endif

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
                unsigned long a, b, c, d, e, f, g, h;
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
                    vs1 += perr(in, 0);
                    /* extract */
                    vs2l += unpkbw(in);
                    vs2h += unpkbw(in >> 32);
                    buf += SOUL;
                } while (--j);
                /* split vs2 */
                if(host_is_bigendian()) {
                    a = (vs2h >> 48) & 0x0000ffff;
                    b = (vs2h >> 32) & 0x0000ffff;
                    c = (vs2h >> 16) & 0x0000ffff;
                    d = (vs2h      ) & 0x0000ffff;
                    e = (vs2l >> 48) & 0x0000ffff;
                    f = (vs2l >> 32) & 0x0000ffff;
                    g = (vs2l >> 16) & 0x0000ffff;
                    h = (vs2l      ) & 0x0000ffff;
                } else {
                    a = (vs2l      ) & 0x0000ffff;
                    b = (vs2l >> 16) & 0x0000ffff;
                    c = (vs2l >> 32) & 0x0000ffff;
                    d = (vs2l >> 48) & 0x0000ffff;
                    e = (vs2h      ) & 0x0000ffff;
                    f = (vs2h >> 16) & 0x0000ffff;
                    g = (vs2h >> 32) & 0x0000ffff;
                    h = (vs2h >> 48) & 0x0000ffff;
                }
                /* mull&add vs2 horiz. */
                vs2 += 8*a + 7*b + 6*c + 5*d + 4*e + 3*f + 2*g + 1*h;
            } while (k >= SOUL);
            /* chop vs1 round sum before multiplying by 8 */
            CHOP(vs1_r);
            /* add vs1 for this round (8 times) */
            vs2 += vs1_r * 8;
            /* CHOP both sums */
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
#endif
