/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   x86 implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2009-2012 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include "x86.h"

#if GCC_VERSION_GE(203)
#  define GCC_ATTR_ALIGNED(x) __attribute__((__aligned__(x)))
#else
#  define VEC_NO_GO
#endif

/* inline asm, so only on GCC (or compatible) */
#if defined(__GNUC__) && !defined(VEC_NO_GO)
#  define HAVE_ADLER32_VEC
#  define MIN_WORK 64

/* ========================================================================= */
local const struct { short d[24]; } vord GCC_ATTR_ALIGNED(16) = {
    {1,1,1,1,1,1,1,1,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1}
};

/* ========================================================================= */
local const struct { char d[16]; } vord_b GCC_ATTR_ALIGNED(16) = {
    {16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1}
};

/* ========================================================================= */
local noinline const Bytef *adler32_jumped(buf, s1, s2, k)
    const Bytef *buf;
    unsigned int *s1;
    unsigned int *s2;
    unsigned int k;
{
    unsigned int t;
    unsigned n = k % 16;
    buf += n;
    k = (k / 16) + 1;

    __asm__ __volatile__ (
#  ifdef __x86_64__
#    define CLOB "&"
            "lea	1f(%%rip), %q4\n\t"
            "lea	(%q4,%q5,8), %q4\n\t"
            "jmp	*%q4\n\t"
#  else
#    ifndef __PIC__
#      define CLOB
            "lea	1f(,%5,8), %4\n\t"
#    else
#      define CLOB
            "lea	1f-3f(,%5,8), %4\n\t"
            "call	9f\n"
            "3:\n\t"
#    endif
            "jmp	*%4\n\t"
#    ifdef __PIC__
            ".p2align 1\n"
            "9:\n\t"
            "addl	(%%esp), %4\n\t"
            "ret\n\t"
#    endif
#  endif
            ".p2align 1\n"
            "2:\n\t"
#  ifdef __i386
            ".byte 0x3e\n\t"
#  endif
            "add	$0x10, %2\n\t"
            ".p2align 1\n"
            "1:\n\t"
            /* 128 */
            "movzbl	-16(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /* 120 */
            "movzbl	-15(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /* 112 */
            "movzbl	-14(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /* 104 */
            "movzbl	-13(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*  96 */
            "movzbl	-12(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*  88 */
            "movzbl	-11(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*  80 */
            "movzbl	-10(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*  72 */
            "movzbl	 -9(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*  64 */
            "movzbl	 -8(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*  56 */
            "movzbl	 -7(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*  48 */
            "movzbl	 -6(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*  40 */
            "movzbl	 -5(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*  32 */
            "movzbl	 -4(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*  24 */
            "movzbl	 -3(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*  16 */
            "movzbl	 -2(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*   8 */
            "movzbl	 -1(%2), %4\n\t"	/* 4 */
            "add	%4, %0\n\t"	/* 2 */
            "add	%0, %1\n\t"	/* 2 */
            /*   0 */
            "dec	%3\n\t"
            "jnz	2b"
        : /* %0 */ "=R" (*s1),
          /* %1 */ "=R" (*s2),
          /* %2 */ "=abdSD" (buf),
          /* %3 */ "=c" (k),
          /* %4 */ "="CLOB"R" (t)
        : /* %5 */ "r" (16 - n),
          /*    */ "0" (*s1),
          /*    */ "1" (*s2),
          /*    */ "2" (buf),
          /*    */ "3" (k)
        : "cc", "memory"
    );

    return buf;
}



#  if 0 && (HAVE_BINUTILS-0) >= 222
        /*
         * 2013 Intel will hopefully bring the Haswell CPUs,
         * which hopefully will have AVX2, which brings integer
         * ops to the full width AVX regs.
         */
        "2:\n\t"
        "mov	$256, %1\n\t"
        "cmp	%1, %3\n\t"
        "cmovb	%3, %1\n\t"
        "and	$-32, %1\n\t"
        "sub	%1, %3\n\t"
        "shr	$5, %1\n\t"
        "vpxor	%%xmm6, %%xmm6\n\t"
        ".p2align 4,,7\n"
        ".p2align 3\n"
        "1:\n\t"
        "vmovdqa	(%0), %%ymm0\n\t"
        "prefetchnta	0x70(%0)\n\t"
        "vpaddd	%%ymm3, %%ymm7, %%ymm7\n\t"
        "add	$32, %0\n\t"
        "dec	%1\n\t"
        "vpsadbw	%%ymm4, %%ymm0, %%ymm1\n\t"
        "vpmaddubsw	%%ymm5, %%ymm0, %%ymm0\n\t"
        "vpaddd	%%ymm1, %%ymm3, %%ymm3\n\t"
        "vpaddw	%%ymm0, %%ymm6, %%ymm6\n\t"
        "jnz	1b\n\t"
        "vpunpckhwd	%%ymm4, %%ymm6, %%xmm0\n\t"
        "vpunpcklwd	%%ymm4, %%ymm6, %%ymm6\n\t"
        "vpaddd	%%ymm0, %%ymm2, %%ymm2\n\t"
        "vpaddd	%%ymm6, %%ymm2, %%ymm2\n\t"
        "cmp	$32, %3\n\t"
        "jg	2b\n\t"
        avx2_chop
        ...
#  endif

#  if 0
        /*
         * Will XOP processors have SSSE3/AVX??
         * And what is the unaligned load performance?
         */
        "prefetchnta	0x70(%0)\n\t"
        "lddqu	(%0), %%xmm0\n\t"
        "vpaddd	%%xmm3, %%xmm5, %%xmm5\n\t"
        "sub	$16, %3\n\t"
        "add	$16, %0\n\t"
        "cmp	$15, %3\n\t"
        "vphaddubd	%%xmm0, %%xmm1\n\t" /* A */
        "vpmaddubsw %%xmm4, %%xmm0, %%xmm0\n\t"/* AVX! */ /* 1 */
        "vphadduwd	%%xmm0, %%xmm0\n\t" /* 2 */
        "vpaddd	%%xmm1, %%xmm3, %%xmm3\n\t" /* B: A+B => hadd+acc or vpmadcubd w. mul = 1 */
        "vpaddd	%%xmm0, %%xmm2, %%xmm2\n\t" /* 3: 1+2+3 => vpmadcubd w. mul = 16,15,14... */
        "jg	1b\n\t"
        xop_chop
        xop_chop
        xop_chop
        setup
        "jg	1b\n\t"
        "vphaddudq	%%xmm2, %%xmm0\n\t"
        "vphaddudq	%%xmm3, %%xmm1\n\t"
        "pshufd	$0xE6, %%xmm0, %%xmm2\n\t"
        "pshufd	$0xE6, %%xmm1, %%xmm3\n\t"
        "paddd	%%xmm0, %%xmm2\n\t"
        "paddd	%%xmm1, %%xmm3\n\t"
        "movd	%%xmm2, %2\n\t"
        "movd	%%xmm3, %1\n\t"
#  endif

/* ========================================================================= */
#  define TO_STR2(x) #x
#  define TO_STR(x) TO_STR2(x)
#  if (HAVE_BINUTILS-0) >= 217
#   define PMADDUBSW_XMM5_XMM0 "pmaddubsw	%%xmm5, %%xmm0\n\t"
#   define PALIGNR_XMM0_XMM0(o) "palignr	$("TO_STR(o)"), %%xmm0, %%xmm0\n\t"
#   define PALIGNR_XMM1_XMM0(o) "palignr	$("TO_STR(o)"), %%xmm1, %%xmm0\n\t"
#  else
#   define PMADDUBSW_XMM5_XMM0 ".byte 0x66, 0x0f, 0x38, 0x04, 0xc5\n\t"
#   define PALIGNR_XMM0_XMM0(o) ".byte 0x66, 0x0f, 0x3a, 0x0f, 0xc0, "TO_STR(o)"\n\t"
#   define PALIGNR_XMM1_XMM0(o) ".byte 0x66, 0x0f, 0x3a, 0x0f, 0xc1, "TO_STR(o)"\n\t"
#  endif
#  ifdef __x86_64__
#   define NREG "q"
#  else
#   define NREG ""
#  endif
local uLong adler32_SSSE3(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1 = adler & 0xffff;
    unsigned int s2 = (adler >> 16) & 0xffff;
    unsigned int k,t;

    __asm__ __volatile__ (
            "jb	9f\n\t"				/* if(k < 32) goto OUT */
            "prefetchnta	0x70(%0)\n\t"
            "movdqa	%6, %%xmm5\n\t"		/* get vord_b */
            "movd	%k2, %%xmm2\n\t"	/* init vector sum vs2 with s2 */
            "movd	%k1, %%xmm3\n\t"	/* init vector sum vs1 with s1 */
            "pxor	%%xmm4, %%xmm4\n"	/* zero */
            "mov	%k0, %k3\n\t"		/* copy buf pointer */
            "and	$-16, %0\n\t"		/* align buf pointer down */
            "xor	%k0, %k3\n\t"		/* extract num_misaligned */
            "jz	4f\n\t"				/* no misalignment? goto start of loop */
            "movdqa	(%0), %%xmm0\n\t"	/* fetch input data as first arg */
            "pxor	%%xmm1, %%xmm1\n\t"	/* clear second arg */
            "mov	%3, %5\n\t"		/* copy num_misaligned as last arg */
            "call	ssse3_align_l\n"	/* call ssse3 left alignment helper */
            "sub	$16, %k3\n\t"		/* build valid bytes ... */
            "add	$16, %0\n\t"		/* advance input data pointer */
            "neg	%k3\n\t"		/* ... out of num_misaligned */
            "sub	%k3, %k4\n\t"		/* len -= valid bytes */
            "imul	%k1, %k3\n\t"		/* valid_bytes *= s1 */
            "movd	%k3, %%xmm1\n\t"	/* transfer to sse */
            "pslldq	$12, %%xmm1\n\t"	/* mov into high dword */
            "por	%%xmm1, %%xmm2\n\t"	/* stash s1 times valid_bytes in vs2 */
            "movdqa	%%xmm0, %%xmm1\n\t"	/* make a copy of the input data */
            PMADDUBSW_XMM5_XMM0		/* multiply all input bytes by vord_b bytes, add adjecent results to words */
            "psadbw	%%xmm4, %%xmm1\n\t"	/* subtract zero from every byte, add 8 bytes to a sum */
            "movdqa	%%xmm0, %%xmm6\n\t"	/* copy word mul result */
            "paddd	%%xmm1, %%xmm3\n\t"	/* vs1 += psadbw */
            "punpckhwd	%%xmm4, %%xmm0\n\t"	/* zero extent upper words to dwords */
            "punpcklwd	%%xmm4, %%xmm6\n\t"	/* zero extent lower words to dwords */
            "paddd	%%xmm0, %%xmm2\n\t"	/* vs2 += vs2_i.upper */
            "paddd	%%xmm6, %%xmm2\n\t"	/* vs2 += vs2_i.lower */
            "jmp	4f\n\t"
#  if defined(__ELF__) && !defined(__clang__)
            ".subsection 2\n\t"
#  endif
            ".p2align 2\n"
            /*
             * helper function to align data in sse reg
             * will keep %5 bytes in xmm0 on the left
             * and fill the rest with bytes from xmm1
             * (note: little-endian -> left == higher addr)
             */
            "ssse3_align_l:\n\t"
            "sub	$1, %5\n\t"	/* k -= 1 */
            "shl	$4, %5\n\t"	/* k *= 16 */
#  ifdef __x86_64__
            "lea	7f(%%rip), %%r11\n\t"
            "lea	(%%r11,%q5, 1), %q5\n\t"
            "jmp	*%q5\n\t"
#  else
#   ifndef __PIC__
            "lea	7f(%5), %5\n\t"
#   else
            "lea	7f-1f(%5), %5\n\t"
            "call	2f\n\t"
            "1:\n\t"
#   endif
            "jmp	*%5\n\t"
#  endif
            ".p2align 2\n"
            /*
             * helper function to align data in sse reg
             * will push %5 bytes in xmm0 to the left
             * and fill the rest with bytes from xmm1
             * (note: little-endian -> left == higher addr)
             */
            "ssse3_align_r:\n\t"
            "sub	$15, %5\n\t"
            "neg	%5\n\t"		/* k = 16 - (k - 1) */
            "shl	$4, %5\n\t"	/* k *= 16 */
#  ifdef __x86_64__
            "lea	6f(%%rip), %%r11\n\t"
            "lea	(%%r11,%q5, 1), %q5\n\t"
            "jmp	*%q5\n\t"
#  else
#   ifndef __PIC__
            "lea	6f(%5), %5\n\t"
#   else
            "lea	6f-1f(%5), %5\n\t"
            "call	2f\n\t"
            "1:\n\t"
#   endif
            "jmp	*%5\n\t"
#   ifdef __PIC__
            "2:\n\t"
            "addl	(%%esp), %5\n\t"
            "ret\n\t"
#   endif
#  endif

#  define ALIGNSTEP(o) \
    PALIGNR_XMM0_XMM0(o) \
    ".p2align 3\n\t" \
    PALIGNR_XMM1_XMM0(16-o) \
    "ret\n\t" \
    ".p2align 4\n\t"

            ".p2align 4\n"
            "7:\n\t"		/* 0 */
            PALIGNR_XMM0_XMM0(1) /* 6 */
            ".p2align 3\n\t" /* 2 */
            "6:\n\t"
            PALIGNR_XMM1_XMM0(16-1) /* 6 */
            "ret\n\t" /* 1 */
            ".p2align 4\n\t"	/* 16 */
            ALIGNSTEP(2)	/* 32 */
            ALIGNSTEP(3)	/* 48 */
            ALIGNSTEP(4)	/* 64 */
            ALIGNSTEP(5)	/* 80 */
            ALIGNSTEP(6)	/* 96 */
            ALIGNSTEP(7)	/* 112 */
            ALIGNSTEP(8)	/* 128 */
            ALIGNSTEP(9)	/* 144 */
            ALIGNSTEP(10)	/* 160 */
            ALIGNSTEP(11)	/* 176 */
            ALIGNSTEP(12)	/* 192 */
            ALIGNSTEP(13)	/* 208 */
            ALIGNSTEP(14)	/* 224 */
            ALIGNSTEP(15)	/* 256 */
#  undef ALIGNSTEP
            ".p2align 2\n"
            /*
             * reduction function to bring a vector sum within the range of BASE
             * This does no full reduction! When the sum is large, a number > BASE
             * is the result. To do a full reduction call multiple times.
             */
            "sse2_chop:\n\t"
            "movdqa	%%xmm0, %%xmm1\n\t"	/* y = x */
            "pslld	$16, %%xmm1\n\t"	/* y <<= 16 */
            "psrld	$16, %%xmm0\n\t"	/* x >>= 16 */
            "psrld	$16, %%xmm1\n\t"	/* y >>= 16 */
            "psubd	%%xmm0, %%xmm1\n\t"	/* y -= x */
            "pslld	$4, %%xmm0\n\t"		/* x <<= 4 */
            "paddd	%%xmm1, %%xmm0\n\t"	/* x += y */
            "ret\n\t"
#  if defined(__ELF__) && !defined(__clang__)
            ".previous\n\t"
#  endif
            ".p2align 2\n"
            "6:\n\t"
            "mov	$128, %1\n\t"		/* inner_k = 128 bytes till vs2_i overflows */
            "cmp	%1, %3\n\t"
            "cmovb	%3, %1\n\t"		/* inner_k = k >= inner_k ? inner_k : k */
            "and	$-16, %1\n\t"		/* inner_k = ROUND_TO(inner_k, 16) */
            "sub	%1, %3\n\t"		/* k -= inner_k */
            "shr	$4, %1\n\t"		/* inner_k /= 16 */
            "mov	$1, %5\n\t"		/* outer_k = 1 */
            "jmp	5f\n"			/* goto loop start */
            "3:\n\t"
            "pxor	%%xmm7, %%xmm7\n\t"	/* zero vs1_round_sum */
            "mov	%3, %5\n\t"		/* determine full inner_k (==8) rounds from k */
            "and	$-128, %5\n\t"		/* outer_k = ROUND_TO(outer_k, 128) */
            "sub	%5, %3\n\t"		/* k -= outer_k */
            "shr	$7, %5\n\t"		/* outer_k /= 128 */
            "jz         6b\n\t"			/* if outer_k == 0 handle trailer */
            ".p2align 3,,3\n\t"
            ".p2align 2\n"
            "2:\n\t"
            "mov	$8, %1\n"		/* inner_k = 8 */
            "5:\n\t"
            "pxor	%%xmm6, %%xmm6\n\t"	/* zero vs2_i */
            ".p2align 4,,7\n"
            ".p2align 3\n"
            "1:\n\t"
            "movdqa	(%0), %%xmm0\n\t"	/* fetch input data */
            "prefetchnta	0x70(%0)\n\t"
            "paddd	%%xmm3, %%xmm7\n\t"	/* vs1_round_sum += vs1 */
            "add	$16, %0\n\t"		/* advance input data pointer */
            "dec	%1\n\t"			/* decrement inner_k */
            "movdqa	%%xmm0, %%xmm1\n\t"	/* make a copy of the input data */
            PMADDUBSW_XMM5_XMM0		/* multiply all input bytes by vord_b bytes, add adjecent results to words */
            "psadbw	%%xmm4, %%xmm1\n\t"	/* subtract zero from every byte, add 8 bytes to a sum */
            "paddw	%%xmm0, %%xmm6\n\t"	/* vs2_i += in * vorder_b */
            "paddd	%%xmm1, %%xmm3\n\t"	/* vs1 += psadbw */
            "jnz	1b\n\t"			/* repeat if inner_k != 0 */
            "movdqa	%%xmm6, %%xmm0\n\t"	/* copy vs2_i */
            "punpckhwd	%%xmm4, %%xmm0\n\t"	/* zero extent vs2_i upper words to dwords */
            "punpcklwd	%%xmm4, %%xmm6\n\t"	/* zero extent vs2_i lower words to dwords */
            "paddd	%%xmm0, %%xmm2\n\t"	/* vs2 += vs2_i.upper */
            "paddd	%%xmm6, %%xmm2\n\t"	/* vs2 += vs2_i.lower */
            "dec	%5\n\t"			/* decrement outer_k  */
            "jnz	2b\n\t"			/* repeat with inner_k = 8 if outer_k != 0 */
            "cmp	$15, %3\n\t"
            "ja	6b\n\t"				/* if(k > 15) repeat */
            "movdqa	%%xmm7, %%xmm0\n\t"	/* move vs1_round_sum */
            "call	sse2_chop\n\t"		/* chop vs1_round_sum */
            "pslld	$4, %%xmm0\n\t"		/* vs1_round_sum *= 16 */
            "paddd	%%xmm2, %%xmm0\n\t"	/* vs2 += vs1_round_sum */
            "call	sse2_chop\n\t"		/* chop again */
            "movdqa	%%xmm0, %%xmm2\n\t"	/* move vs2 back in place */
            "movdqa	%%xmm3, %%xmm0\n\t"	/* move vs1 */
            "call	sse2_chop\n\t"		/* chop */
            "movdqa	%%xmm0, %%xmm3\n\t"	/* move vs1 back in place */
            "add	%3, %4\n"		/* len += k */
            "4:\n\t"	/* top of loop */
            "mov	%7, %3\n\t"		/* get max. byte count VNMAX till v1_round_sum overflows */
            "cmp	%3, %4\n\t"
            "cmovb	%4, %3\n\t"		/* k = len >= VNMAX ? k : len */
            "sub	%3, %4\n\t"		/* len -= k */
            "cmp	$15, %3\n\t"
            "ja	3b\n\t"				/* if(k > 15) repeat */
            "test	%k3, %k3\n\t"		/* test for 0 */
            "jz	5f\n\t"				/* if (k == 0) goto OUT */
            "movdqa	(%0), %%xmm0\n\t"	/* fetch remaining input data as first arg */
            "add	%"NREG"3, %"NREG"0\n\t"		/* add remainder to to input data pointer */
            "movdqa	%%xmm4, %%xmm1\n\t"	/* set second arg 0 */
            "mov	%k3, %k5\n\t"		/* copy remainder as last arg */
            "call	ssse3_align_r\n\t"	/* call ssse3 right alignment helper */
            "movdqa	%%xmm4, %%xmm7\n\t"	/* sum = 0 */
            "movdqa	%%xmm3, %%xmm1\n\t"	/* t = vs1 */
            /* russian peasant mul, k is small */
            ".p2align 2\n"
            "6:\n\t"
            "shr	$1, %k3\n\t"		/* k >>= 1 */
            "jnc	7f\n\t"
            "paddd	%%xmm1, %%xmm7\n\t"	/* add t to sum if 1 bit shifted out of k */
            "7:\n\t"
            "paddd	%%xmm1, %%xmm1\n\t"	/* t *= 2 */
            "jnz	6b\n\t"			/* while(k != 0) */
            "paddd	%%xmm7, %%xmm2\n\t"	/* vs2 += k * vs1 */
            "movdqa	%%xmm0, %%xmm1\n\t"	/* make a copy of the input data */
            PMADDUBSW_XMM5_XMM0		/* multiply all input bytes by vord_b bytes, add adjecent results to words */
            "psadbw	%%xmm4, %%xmm1\n\t"	/* subtract zero from every byte, add 8 bytes to a sum */
            "movdqa	%%xmm0, %%xmm6\n\t"	/* copy word mul result */
            "paddd	%%xmm1, %%xmm3\n\t"	/* vs1 += psadbw */
            "punpckhwd	%%xmm4, %%xmm0\n\t"	/* zero extent upper words to dwords */
            "punpcklwd	%%xmm4, %%xmm6\n\t"	/* zero extent lower words to dwords */
            "paddd	%%xmm0, %%xmm2\n\t"	/* vs2 += vs2_i.upper */
            "paddd	%%xmm6, %%xmm2\n\t"	/* vs2 += vs2_i.lower */
            "5:\n\t"	/* OUT */
            "pshufd	$0xEE, %%xmm3, %%xmm1\n\t"	/* collect vs1 & vs2 in lowest vector member */
            "pshufd	$0xEE, %%xmm2, %%xmm0\n\t"
            "paddd	%%xmm3, %%xmm1\n\t"
            "paddd	%%xmm2, %%xmm0\n\t"
            "pshufd	$0xE5, %%xmm0, %%xmm2\n\t"
            "paddd	%%xmm0, %%xmm2\n\t"
            "movd	%%xmm1, %1\n\t"		/* mov vs1 to s1 */
            "movd	%%xmm2, %2\n"		/* mov vs2 to s2 */
            "9:"
        : /* %0 */ "=r" (buf),
          /* %1 */ "=r" (s1),
          /* %2 */ "=r" (s2),
          /* %3 */ "=r" (k),
          /* %4 */ "=r" (len),
          /* %5 */ "=r" (t)
        : /* %6 */ "m" (vord_b),
          /*
           * somewhere between 5 & 6, psadbw 64 bit sums ruin the party
           * spreading the sums with palignr only brings it to 7 (?),
           * while introducing an op into the main loop (2800 ms -> 3200 ms)
           */
          /* %7 */ "i" (5*NMAX),
          /*    */ "0" (buf),
          /*    */ "1" (s1),
          /*    */ "2" (s2),
          /*    */ "4" (len)
        : "cc", "memory"
#  ifdef __x86_64__
          , "r11"
#  endif
#  ifdef __SSE__
          , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#  endif
    );

    if (unlikely(k))
        buf = adler32_jumped(buf, &s1, &s2, k);
    MOD28(s1);
    MOD28(s2);
    return (s2 << 16) | s1;
}
#  undef PMADDUBSW_XMM5_XMM0
#  undef PALIGNR_XMM0_XMM0
#  undef PALIGNR_XMM1_XMM0
#  undef NREG

/* ========================================================================= */
local uLong adler32_SSE2(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1 = adler & 0xffff;
    unsigned int s2 = (adler >> 16) & 0xffff;
    unsigned int k;

    k    = ALIGN_DIFF(buf, 16);
    len -= k;
    if (k)
        buf = adler32_jumped(buf, &s1, &s2, k);

    __asm__ __volatile__ (
            "mov	%6, %3\n\t"
            "cmp	%3, %4\n\t"
            "cmovb	%4, %3\n\t"
            "sub	%3, %4\n\t"
            "cmp	$16, %3\n\t"
            "jb	8f\n\t"
            "prefetchnta	0x70(%0)\n\t"
            "movd	%1, %%xmm4\n\t"
            "movd	%2, %%xmm3\n\t"
            "pxor	%%xmm2, %%xmm2\n\t"
            "pxor	%%xmm5, %%xmm5\n\t"
            ".p2align 2\n"
            "3:\n\t"
            "pxor	%%xmm6, %%xmm6\n\t"
            "pxor	%%xmm7, %%xmm7\n\t"
            "mov	$2048, %1\n\t"		/* get byte count till vs2_{l|h}_word overflows */
            "cmp	%1, %3\n\t"
            "cmovb	%3, %1\n"
            "and	$-16, %1\n\t"
            "sub	%1, %3\n\t"
            "shr	$4, %1\n\t"
            ".p2align 4,,7\n"
            ".p2align 3\n"
            "1:\n\t"
            "prefetchnta	0x70(%0)\n\t"
            "movdqa	(%0), %%xmm0\n\t"	/* fetch input data */
            "paddd	%%xmm4, %%xmm5\n\t"	/* vs1_round_sum += vs1 */
            "add	$16, %0\n\t"
            "dec	%1\n\t"
            "movdqa	%%xmm0, %%xmm1\n\t"	/* copy input data */
            "psadbw	%%xmm2, %%xmm0\n\t"	/* add all bytes horiz. */
            "paddd	%%xmm0, %%xmm4\n\t"	/* add that to vs1 */
            "movdqa	%%xmm1, %%xmm0\n\t"	/* copy input data */
            "punpckhbw	%%xmm2, %%xmm1\n\t"	/* zero extent input upper bytes to words */
            "punpcklbw	%%xmm2, %%xmm0\n\t"	/* zero extent input lower bytes to words */
            "paddw	%%xmm1, %%xmm7\n\t"	/* vs2_h_words += in_high_words */
            "paddw	%%xmm0, %%xmm6\n\t"	/* vs2_l_words += in_low_words */
            "jnz	1b\n\t"
            "cmp	$15, %3\n\t"
            "pmaddwd	32+%5, %%xmm7\n\t"	/* multiply vs2_h_words with order, add adjecend results */
            "pmaddwd	16+%5, %%xmm6\n\t"	/* multiply vs2_l_words with order, add adjecend results */
            "paddd	%%xmm7, %%xmm3\n\t"	/* add to vs2 */
            "paddd	%%xmm6, %%xmm3\n\t"	/* add to vs2 */
            "jg	3b\n\t"
            "movdqa	%%xmm5, %%xmm0\n\t"
            "pxor	%%xmm5, %%xmm5\n\t"
            "call	sse2_chop\n\t"
            "pslld	$4, %%xmm0\n\t"
            "paddd	%%xmm3, %%xmm0\n\t"
            "call	sse2_chop\n\t"
            "movdqa	%%xmm0, %%xmm3\n\t"
            "movdqa	%%xmm4, %%xmm0\n\t"
            "call	sse2_chop\n\t"
            "movdqa	%%xmm0, %%xmm4\n\t"
            "add	%3, %4\n\t"
            "mov	%6, %3\n\t"
            "cmp	%3, %4\n\t"
            "cmovb	%4, %3\n"
            "sub	%3, %4\n\t"
            "cmp	$15, %3\n\t"
            "jg	3b\n\t"
            "pshufd	$0xEE, %%xmm4, %%xmm1\n\t"
            "pshufd	$0xEE, %%xmm3, %%xmm0\n\t"
            "paddd	%%xmm4, %%xmm1\n\t"
            "paddd	%%xmm3, %%xmm0\n\t"
            "pshufd	$0xE5, %%xmm0, %%xmm3\n\t"
            "paddd	%%xmm0, %%xmm3\n\t"
            "movd	%%xmm1, %1\n\t"
            "movd	%%xmm3, %2\n"
            "8:\n\t"
        : /* %0 */ "=r" (buf),
          /* %1 */ "=r" (s1),
          /* %2 */ "=r" (s2),
          /* %3 */ "=r" (k),
          /* %4 */ "=r" (len)
        : /* %5 */ "m" (vord),
          /* %6 */ "i" (5*NMAX),
          /*    */ "0" (buf),
          /*    */ "1" (s1),
          /*    */ "2" (s2),
          /*    */ "3" (k),
          /*    */ "4" (len)
        : "cc", "memory"
#  ifdef __SSE__
          , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#  endif
    );

    if (unlikely(k))
        buf = adler32_jumped(buf, &s1, &s2, k);
    MOD28(s1);
    MOD28(s2);
    return (s2 << 16) | s1;
}

#  if 0
/* ========================================================================= */
/*
 * The SSE2 version above is faster on my CPUs (Athlon64, Core2,
 * P4 Xeon, K10 Sempron), but has instruction stalls only a
 * Out-Of-Order-Execution CPU can solve.
 * So this Version _may_ be better for the new old thing, Atom.
 */
local noinline uLong adler32_SSE2_no_oooe(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1 = adler & 0xffff;
    unsigned int s2 = (adler >> 16) & 0xffff;
    unsigned int k;

    k    = ALIGN_DIFF(buf, 16);
    len -= k;
    if (k)
        buf = adler32_jumped(buf, &s1, &s2, k);

    __asm__ __volatile__ (
            "mov	%6, %3\n\t"
            "cmp	%3, %4\n\t"
            "cmovb	%4, %3\n\t"
            "sub	%3, %4\n\t"
            "cmp	$16, %3\n\t"
            "jb	8f\n\t"
            "movdqa	16+%5, %%xmm6\n\t"
            "movdqa	32+%5, %%xmm5\n\t"
            "prefetchnta	16(%0)\n\t"
            "pxor	%%xmm7, %%xmm7\n\t"
            "movd	%1, %%xmm4\n\t"
            "movd	%2, %%xmm3\n\t"
            ".p2align 3,,3\n\t"
            ".p2align 2\n"
            "1:\n\t"
            "prefetchnta	32(%0)\n\t"
            "movdqa	(%0), %%xmm1\n\t"
            "sub	$16, %3\n\t"
            "movdqa	%%xmm4, %%xmm2\n\t"
            "add	$16, %0\n\t"
            "movdqa	%%xmm1, %%xmm0\n\t"
            "cmp	$15, %3\n\t"
            "pslld	$4, %%xmm2\n\t"
            "paddd	%%xmm3, %%xmm2\n\t"
            "psadbw	%%xmm7, %%xmm0\n\t"
            "paddd	%%xmm0, %%xmm4\n\t"
            "movdqa	%%xmm1, %%xmm0\n\t"
            "punpckhbw	%%xmm7, %%xmm1\n\t"
            "punpcklbw	%%xmm7, %%xmm0\n\t"
            "movdqa	%%xmm1, %%xmm3\n\t"
            "pmaddwd	%%xmm6, %%xmm0\n\t"
            "paddd	%%xmm2, %%xmm0\n\t"
            "pmaddwd	%%xmm5, %%xmm3\n\t"
            "paddd	%%xmm0, %%xmm3\n\t"
            "jg	1b\n\t"
            "movdqa	%%xmm3, %%xmm0\n\t"
            "call	sse2_chop\n\t"
            "call	sse2_chop\n\t"
            "movdqa	%%xmm0, %%xmm3\n\t"
            "movdqa	%%xmm4, %%xmm0\n\t"
            "call	sse2_chop\n\t"
            "movdqa	%%xmm0, %%xmm4\n\t"
            "add	%3, %4\n\t"
            "mov	%6, %3\n\t"
            "cmp	%3, %4\n\t"
            "cmovb	%4, %3\n\t"
            "sub	%3, %4\n\t"
            "cmp	$15, %3\n\t"
            "jg	1b\n\t"
            "pshufd	$0xEE, %%xmm3, %%xmm0\n\t"
            "pshufd	$0xEE, %%xmm4, %%xmm1\n\t"
            "paddd	%%xmm3, %%xmm0\n\t"
            "pshufd	$0xE5, %%xmm0, %%xmm2\n\t"
            "paddd	%%xmm4, %%xmm1\n\t"
            "movd	%%xmm1, %1\n\t"
            "paddd	%%xmm0, %%xmm2\n\t"
            "movd	%%xmm2, %2\n"
            "8:"
        : /* %0 */ "=r" (buf),
          /* %1 */ "=r" (s1),
          /* %2 */ "=r" (s2),
          /* %3 */ "=r" (k),
          /* %4 */ "=r" (len)
        : /* %5 */ "m" (vord),
          /* %6 */ "i" (NMAX + NMAX/3),
          /*    */ "0" (buf),
          /*    */ "1" (s1),
          /*    */ "2" (s2),
          /*    */ "4" (len)
        : "cc", "memory"
#  ifdef __SSE__
          , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#  endif
    );

    if (unlikely(k))
        buf = adler32_jumped(buf, &s1, &s2, k);
    MOD28(s1);
    MOD28(s2);
    return (s2 << 16) | s1;
}
#  endif

#  ifndef __x86_64__
/* ========================================================================= */
/*
 * SSE version to help VIA-C3_2, P2 & P3
 */
local uLong adler32_SSE(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1 = adler & 0xffff;
    unsigned int s2 = (adler >> 16) & 0xffff;
    unsigned int k;

    k    = ALIGN_DIFF(buf, 8);
    len -= k;
    if (k)
        buf = adler32_jumped(buf, &s1, &s2, k);

    __asm__ __volatile__ (
            "mov	%6, %3\n\t"
            "cmp	%3, %4\n\t"
            "cmovb	%4, %3\n\t"
            "sub	%3, %4\n\t"
            "cmp	$8, %3\n\t"
            "jb	8f\n\t"
            "movd	%1, %%mm4\n\t"
            "movd	%2, %%mm3\n\t"
            "pxor	%%mm2, %%mm2\n\t"
            "pxor	%%mm5, %%mm5\n\t"
#    ifdef __ELF__
            ".subsection 2\n\t"
#    else
            "jmp	7f\n\t"
#    endif
            ".p2align 2\n"
            "mmx_chop:\n\t"
            "movq	%%mm0, %%mm1\n\t"
            "pslld	$16, %%mm1\n\t"
            "psrld	$16, %%mm0\n\t"
            "psrld	$16, %%mm1\n\t"
            "psubd	%%mm0, %%mm1\n\t"
            "pslld	$4, %%mm0\n\t"
            "paddd	%%mm1, %%mm0\n\t"
            "ret\n\t"
#    ifdef __ELF__
            ".previous\n\t"
#    else
            "7:\n\t"
#    endif
            ".p2align	2\n"
            "3:\n\t"
            "pxor	%%mm6, %%mm6\n\t"
            "pxor	%%mm7, %%mm7\n\t"
            "mov	$1024, %1\n\t"
            "cmp	%1, %3\n\t"
            "cmovb	%3, %1\n"
            "and	$-8, %1\n\t"
            "sub	%1, %3\n\t"
            "shr	$3, %1\n\t"
            ".p2align 4,,7\n"
            ".p2align 3\n"
            "1:\n\t"
            "movq	(%0), %%mm0\n\t"
            "paddd	%%mm4, %%mm5\n\t"
            "add	$8, %0\n\t"
            "dec	%1\n\t"
            "movq	%%mm0, %%mm1\n\t"
            "psadbw	%%mm2, %%mm0\n\t"
            "paddd	%%mm0, %%mm4\n\t"
            "movq	%%mm1, %%mm0\n\t"
            "punpckhbw	%%mm2, %%mm1\n\t"
            "punpcklbw	%%mm2, %%mm0\n\t"
            "paddw	%%mm1, %%mm7\n\t"
            "paddw	%%mm0, %%mm6\n\t"
            "jnz	1b\n\t"
            "cmp	$7, %3\n\t"
            "pmaddwd	40+%5, %%mm7\n\t"
            "pmaddwd	32+%5, %%mm6\n\t"
            "paddd	%%mm7, %%mm3\n\t"
            "paddd	%%mm6, %%mm3\n\t"
            "jg	3b\n\t"
            "movq	%%mm5, %%mm0\n\t"
            "pxor	%%mm5, %%mm5\n\t"
            "call	mmx_chop\n\t"
            "pslld	$3, %%mm0\n\t"
            "paddd	%%mm3, %%mm0\n\t"
            "call	mmx_chop\n\t"
            "movq	%%mm0, %%mm3\n\t"
            "movq	%%mm4, %%mm0\n\t"
            "call	mmx_chop\n\t"
            "movq	%%mm0, %%mm4\n\t"
            "add	%3, %4\n\t"
            "mov	%6, %3\n\t"
            "cmp	%3, %4\n\t"
            "cmovb	%4, %3\n"
            "sub	%3, %4\n\t"
            "cmp	$7, %3\n\t"
            "jg	3b\n\t"
            "movd	%%mm4, %1\n\t"
            "psrlq	$32, %%mm4\n\t"
            "movd	%%mm3, %2\n\t"
            "psrlq	$32, %%mm3\n\t"
            "movd	%%mm4, %4\n\t"
            "add	%4, %1\n\t"
            "movd	%%mm3, %4\n\t"
            "add	%4, %2\n\t"
            "emms\n"
            "8:\n\t"
        : /* %0 */ "=r" (buf),
          /* %1 */ "=r" (s1),
          /* %2 */ "=r" (s2),
          /* %3 */ "=r" (k),
          /* %4 */ "=r" (len)
        : /* %5 */ "m" (vord),
          /* %6 */ "i" ((5*NMAX)/2),
          /*    */ "0" (buf),
          /*    */ "1" (s1),
          /*    */ "2" (s2),
          /*    */ "3" (k),
          /*    */ "4" (len)
        : "cc", "memory"
#    ifdef __MMX__
          , "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
#    endif
                               );

    if (unlikely(k))
        buf = adler32_jumped(buf, &s1, &s2, k);
    MOD28(s1);
    MOD28(s2);
    return (s2 << 16) | s1;
}

/* ========================================================================= */
/*
 * Processors which only have MMX will prop. not like this
 * code, they are so old, they are not Out-Of-Order
 * (maybe except AMD K6, Cyrix, Winchip/VIA).
 * I did my best to get at least 1 instruction between result -> use
 */
local uLong adler32_MMX(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1 = adler & 0xffff;
    unsigned int s2 = (adler >> 16) & 0xffff;
    unsigned int k;

    k    = ALIGN_DIFF(buf, 8);
    len -= k;
    if (k)
        buf = adler32_jumped(buf, &s1, &s2, k);

    __asm__ __volatile__ (
            "mov	%6, %3\n\t"
            "cmp	%3, %4\n\t"
            "jae	6f\n\t"
            "mov	%4, %3\n"
            "6:\n\t"
            "sub	%3, %4\n\t"
            "cmp	$8, %3\n\t"
            "jb	8f\n\t"
            "sub	$8, %%esp\n\t"
            "movd	%1, %%mm4\n\t"
            "movd	%2, %%mm2\n\t"
            "movq	%5, %%mm3\n"
            "5:\n\t"
            "movq	%%mm2, %%mm0\n\t"
            "pxor	%%mm2, %%mm2\n\t"
            "pxor	%%mm5, %%mm5\n\t"
            ".p2align 2\n"
            "3:\n\t"
            "movq	%%mm0, (%%esp)\n\t"
            "pxor	%%mm6, %%mm6\n\t"
            "pxor	%%mm7, %%mm7\n\t"
            "mov	$1024, %1\n\t"
            "cmp	%1, %3\n\t"
            "jae	4f\n\t"
            "mov	%3, %1\n"
            "4:\n\t"
            "and	$-8, %1\n\t"
            "sub	%1, %3\n\t"
            "shr	$3, %1\n\t"
            ".p2align 4,,7\n\t"
            ".p2align 3\n"
            "1:\n\t"
            "movq	(%0), %%mm0\n\t"
            "paddd	%%mm4, %%mm5\n\t"
            "add	$8, %0\n\t"
            "dec	%1\n\t"
            "movq	%%mm0, %%mm1\n\t"
            "punpcklbw	%%mm2, %%mm0\n\t"
            "punpckhbw	%%mm2, %%mm1\n\t"
            "paddw	%%mm0, %%mm6\n\t"
            "paddw	%%mm1, %%mm0\n\t"
            "paddw	%%mm1, %%mm7\n\t"
            "pmaddwd	%%mm3, %%mm0\n\t"
            "paddd	%%mm0, %%mm4\n\t"
            "jnz	1b\n\t"
            "movq	(%%esp), %%mm0\n\t"
            "cmp	$7, %3\n\t"
            "pmaddwd	32+%5, %%mm6\n\t"
            "pmaddwd	40+%5, %%mm7\n\t"
            "paddd	%%mm6, %%mm0\n\t"
            "paddd	%%mm7, %%mm0\n\t"
            "jg	3b\n\t"
            "movq	%%mm0, %%mm2\n\t"
            "movq	%%mm5, %%mm0\n\t"
            "call	mmx_chop\n\t"
            "pslld	$3, %%mm0\n\t"
            "paddd	%%mm2, %%mm0\n\t"
            "call	mmx_chop\n\t"
            "movq	%%mm0, %%mm2\n\t"
            "movq	%%mm4, %%mm0\n\t"
            "call	mmx_chop\n\t"
            "movq	%%mm0, %%mm4\n\t"
            "add	%3, %4\n\t"
            "mov	%6, %3\n\t"
            "cmp	%3, %4\n\t"
            "jae	2f\n\t"
            "mov	%4, %3\n"
            "2:\n\t"
            "sub	%3, %4\n\t"
            "cmp	$7, %3\n\t"
            "jg	5b\n\t"
            "add	$8, %%esp\n\t"
            "movd	%%mm4, %1\n\t"
            "psrlq	$32, %%mm4\n\t"
            "movd	%%mm2, %2\n\t"
            "psrlq	$32, %%mm2\n\t"
            "movd	%%mm4, %4\n\t"
            "add	%4, %1\n\t"
            "movd	%%mm2, %4\n\t"
            "add	%4, %2\n\t"
            "emms\n"
            "8:\n\t"
        : /* %0 */ "=r" (buf),
          /* %1 */ "=r" (s1),
          /* %2 */ "=r" (s2),
          /* %3 */ "=r" (k),
          /* %4 */ "=r" (len)
        : /* %5 */ "m" (vord),
          /* %6 */ "i" (4*NMAX),
          /*    */ "0" (buf),
          /*    */ "1" (s1),
          /*    */ "2" (s2),
          /*    */ "3" (k),
          /*    */ "4" (len)
        : "cc", "memory"
#    ifdef __MMX__
          , "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
#    endif
    );

    if (unlikely(k))
        buf = adler32_jumped(buf, &s1, &s2, k);
    MOD28(s1);
    MOD28(s2);
    return (s2 << 16) | s1;
}
#  endif

/* ========================================================================= */
local uLong adler32_x86(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    /* split Adler-32 into component sums */
    unsigned int s1 = adler & 0xffff;
    unsigned int s2 = (adler >> 16) & 0xffff;
    unsigned int n;

    do {
        /* find maximum, len or NMAX */
        n = len < NMAX ? len : NMAX;
        len -= n;

        /* do it */
        buf = adler32_jumped(buf, &s1, &s2, n);
        /* modulo */
        MOD(s1);
        MOD(s2);
    } while (likely(len));

    /* return recombined sums */
    return (s2 << 16) | s1;
}

/* ========================================================================= */
#define NO_ADLER32_GE16
local noinline uLong adler32_ge16(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    /* split Adler-32 into component sums */
    unsigned int s1 = adler & 0xffff;
    unsigned int s2 = (adler >> 16) & 0xffff;

    /* simply do it, we do not expect more then NMAX as len */
    adler32_jumped(buf, &s1, &s2, len);
    /* actually we expect much less, MOD28 it */
    MOD28(s1);
    MOD28(s2);

    /* return recombined sums */
    return (s2 << 16) | s1;
}

/* ========================================================================= */
/*
 * Knot it all together with a runtime switch
 */
/* ========================================================================= */
/* function enum */
enum adler32_types
{
    T_ADLER32_RTSWITCH = 0,
    T_ADLER32_X86,
#  ifndef __x86_64__
    T_ADLER32_MMX,
    T_ADLER32_SSE,
#  endif
    T_ADLER32_SSE2,
    T_ADLER32_SSSE3,
    T_ADLER32_MAX
};

/* ========================================================================= */
/* Decision table */
local const struct test_cpu_feature tfeat_adler32_vec[] =
{
    /* func                             flags   features       */
    {T_ADLER32_SSSE3,         0, {CFB(CFEATURE_CMOV), CFB(CFEATURE_SSSE3)}},
    {T_ADLER32_SSE2,          0, {CFB(CFEATURE_SSE2)|CFB(CFEATURE_CMOV), 0}},
#  ifndef __x86_64__
    {T_ADLER32_SSE,           0, {CFB(CFEATURE_SSE)|CFB(CFEATURE_CMOV), 0}},
    {T_ADLER32_MMX,           0, {CFB(CFEATURE_MMX), 0}},
#  endif
    {T_ADLER32_X86, CFF_DEFAULT, { 0, 0}},
};

/* ========================================================================= */
/* Prototypes */
local uLong adler32_vec_runtimesw(uLong adler, const Bytef *buf, uInt len);

/* ========================================================================= */
/* Function pointer table */
local uLong (*const adler32_ptr_tab[])(uLong adler, const Bytef *buf, uInt len) =
{
    adler32_vec_runtimesw,
    adler32_x86,
#  ifndef __x86_64__
    adler32_MMX,
    adler32_SSE,
#  endif
    adler32_SSE2,
    adler32_SSSE3,
};

/* ========================================================================= */
#  if _FORTIFY_SOURCE-0 > 0
/* Runtime decide var */
local enum adler32_types adler32_f_type = T_ADLER32_RTSWITCH;
#  else
/* Runtime Function pointer */
local uLong (*adler32_vec_ptr)(uLong adler, const Bytef *buf, uInt len) = adler32_vec_runtimesw;
#  endif

/* ========================================================================= */
/* Constructor to init the decide var early */
local GCC_ATTR_CONSTRUCTOR void adler32_vec_select(void)
{
    enum adler32_types lf_type =
        _test_cpu_feature(tfeat_adler32_vec, sizeof (tfeat_adler32_vec)/sizeof (tfeat_adler32_vec[0]));
#  if _FORTIFY_SOURCE-0 > 0
    adler32_f_type = lf_type;
#  else
    adler32_vec_ptr = adler32_ptr_tab[lf_type];
#  endif
}

/* ========================================================================= */
/* Jump function */
local noinline uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    /*
     * Protect us from memory corruption. As long as the function pointer table
     * resides in rodata, with a little bounding we can prevent arb. code
     * execution (overwritten vtable pointer). We still may crash if the corruption
     * is within bounds (or the cpudata gets corrupted too) and we jump into an
     * function with unsupported instr., but this should mitigate the worst case
     * scenario.
     * But it's more expensive than a simple function pointer, so only when more
     * security is wanted.
     */
#  if _FORTIFY_SOURCE-0 > 0
    enum adler32_types lf_type = adler32_f_type;
    /*
     * If the compiler is smart he creates a cmp + sbb + and, cmov have a high
     * latency and are not always avail.
     * Otherwise compiler logic is advanced enough to see what's happening here,
     * so there maybe is a reason why he changes this to a cmov...
     * (or he simply does not see he can create a conditional -1/0 the cheap way)
     *
     * Maybe change it to an unlikely() cbranch? Which still leaves the question
     * what's the mispredition propability, esp. with lots of different x86
     * microarchs and not always perfect CFLAGS (-march/-mtune) to arrange the
     * code to the processors liking.
     */
    lf_type &= likely((unsigned)lf_type < (unsigned)T_ADLER32_MAX) ? -1 : 0;
    return adler32_ptr_tab[lf_type](adler, buf, len);
#  else
    return adler32_vec_ptr(adler, buf, len);
#  endif
}

/* ========================================================================= */
/*
 * the runtime switcher is a little racy, but this is OK,
 * it should normaly not run if the constructor works, and
 * we are on x86, which isn't that picky about ordering
 */
local uLong adler32_vec_runtimesw(uLong adler, const Bytef *buf, uInt len)
{
    adler32_vec_select();
    return adler32_vec(adler, buf, len);
}
#endif
