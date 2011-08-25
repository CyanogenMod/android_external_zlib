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

/* we use a bunch of inline asm and GCC vector internals, so GCC it is */
#ifdef __GNUC__
#  ifdef __mips_loongson_vector_rev
#    define HAVE_SLHASH_VEC
#    include <loongson.h>
#    define SOV4 (sizeof(uint16x4_t))

/* ========================================================================= */
local inline void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    if (likely(wsize < (1<<16))) {
        unsigned int i, f, wst;
        uint16x4_t vwsize;

        wst = (wsize  << 16) | (wsize  & 0x0000ffff);
        vwsize = (uint16x4_t)(((unsigned long long)wst  << 32) | wst);
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
            uint16x4_t x = psubush(*(uint16x4_t *)p, vwsize);
            *(uint16x4_t *)p = x;
            p += SOV4/sizeof(*p);
            i--;
        }
        i /= 2;
        do {
            uint16x4_t x1, x2;
            x1 = ((uint16x4_t *)p)[0];
            x2 = ((uint16x4_t *)p)[1];
            x1 = psubush(x1, vwsize);
            x2 = psubush(x2, vwsize);
            ((uint16x4_t *)p)[0] = x1;
            ((uint16x4_t *)p)[1] = x2;
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
#  elif defined(__mips_dspr2)
#    define HAVE_SLHASH_VEC
typedef short v2u16 __attribute__((vector_size(4)));
#    define SOV2 (sizeof(v2u16))

/* ========================================================================= */
local inline void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    if (likely(wsize < (1<<16))) {
        unsigned int i, f;
        v2u16 vwsize;

        vwsize = (v2u16)((wsize  << 16) | (wsize  & 0x0000ffff));
        /* align */
        f = (unsigned)ALIGN_DIFF(p, SOV2);
        if (unlikely(f)) {
            register unsigned m = *p;
            *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
            n--;
        }

        /* do it */
        i  = n / (SOV2/sizeof(*p));
        n %= SOV2/sizeof(*p);
        if (unlikely(i & 1)) {
            v2u16 x = __builtin_mips_subu_s_ph(*(v2u16 *)p, vwsize);
            *(v2u16 *)p = x;
            p += SOV2/sizeof(*p);
            i--;
        }
        i /= 2;
        do {
            v2u16 x1, x2;
            x1 = ((v2u16 *)p)[0];
            x2 = ((v2u16 *)p)[1];
            x1 = __builtin_mips_subu_s_ph(x1, vwsize);
            x2 = __builtin_mips_subu_s_ph(x2, vwsize);
            ((v2u16 *)p)[0] = x1;
            ((v2u16 *)p)[1] = x2;
            p += 2*(SOV2/sizeof(*p));
        } while (--i);

        /* handle trailer */
        if (unlikely(n)) {
            register unsigned m = *p;
            *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
        }
    } else {
        do {
            register unsigned m = *p;
            *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
        } while (--n);
    }
}
#  endif
#endif
