/*
 * Id: fbrop.h,v 1.1 1999/11/02 03:54:45 keithp Exp $
 *
 * Copyright © 1998 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/fb/fbrop.h,v 1.4 2000/02/23 20:29:46 dawes Exp $ */

#ifndef _FBROP_H_
#define _FBROP_H_

#include <asm/byteorder.h>
#define MB86290_32_SWAP(c) (__cpu_to_le16((__u16)(c)) | (__cpu_to_le16(((__u32)(c)) >> 16) << 16))
#define MB86290_16_SWAP(c) (__cpu_to_le16((__u16)(c)))
extern unsigned long MB86290_fbstart;
extern unsigned long MB86290_fbend;

typedef struct _mergeRopBits {
    FbBits   ca1, cx1, ca2, cx2;
} FbMergeRopRec, *FbMergeRopPtr;

extern const FbMergeRopRec	FbMergeRopBits[16];

#define FbDeclareMergeRop() FbBits   _ca1, _cx1, _ca2, _cx2;
#define FbDeclarePrebuiltMergeRop()	FbBits	_cca, _ccx;

#define FbInitializeMergeRop(alu,pm) {\
    const FbMergeRopRec  *_bits; \
    _bits = &FbMergeRopBits[alu]; \
    _ca1 = _bits->ca1 &  pm; \
    _cx1 = _bits->cx1 | ~pm; \
    _ca2 = _bits->ca2 &  pm; \
    _cx2 = _bits->cx2 &  pm; \
}

#define FbDestInvarientRop(alu,pm)  ((pm) == FB_ALLONES && \
				     (((alu) >> 1 & 5) == ((alu) & 5)))

#define FbDestInvarientMergeRop()   (_ca1 == 0 && _cx1 == 0)

/* AND has higher precedence than XOR */

#define FbDoMergeRop(src, dst, src_adr, dst_adr) { \
    if ( ((unsigned long)(src_adr) >= MB86290_fbstart) && ((unsigned long)(src_adr) <= MB86290_fbend) && \
         (((unsigned long)(dst_adr) < MB86290_fbstart) || ((unsigned long)(dst_adr) > MB86290_fbend)) ) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(src); \
	dst = (((dst) & (((__tmp0) & _ca1) ^ _cx1)) ^ (((__tmp0) & _ca2) ^ _cx2)); \
    } \
    else if ( ((unsigned long)(dst_adr) >= MB86290_fbstart) && ((unsigned long)(dst_adr) <= MB86290_fbend) && \
              (((unsigned long)(src_adr) < MB86290_fbstart) || ((unsigned long)(src_adr) > MB86290_fbend)) ) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(dst); \
	FbBits __tmp1 = (((__tmp0) & (((src) & _ca1) ^ _cx1)) ^ (((src) & _ca2) ^ _cx2)); \
	dst = MB86290_32_SWAP(__tmp1); \
    } \
    else if ( ((unsigned long)(src_adr) >= MB86290_fbstart) && ((unsigned long)(src_adr) <= MB86290_fbend) && \
              (((unsigned long)(dst_adr) >= MB86290_fbstart) || ((unsigned long)(dst_adr) <= MB86290_fbend)) ) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(dst); \
	FbBits __tmp1 = MB86290_32_SWAP(src); \
	FbBits __tmp2 = (((__tmp0) & (((__tmp1) & _ca1) ^ _cx1)) ^ (((__tmp1) & _ca2) ^ _cx2)); \
	dst = MB86290_32_SWAP(__tmp2); \
    } \
    else \
    { \
	dst = (((dst) & (((src) & _ca1) ^ _cx1)) ^ (((src) & _ca2) ^ _cx2)); \
    } \
}

#define FbDoDestInvarientMergeRop(src, dst, src_adr, dst_adr) { \
    if ( ((unsigned long)(src_adr) >= MB86290_fbstart) && ((unsigned long)(src_adr) <= MB86290_fbend) && \
         (((unsigned long)(dst_adr) < MB86290_fbstart) || ((unsigned long)(dst_adr) > MB86290_fbend)) ) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(src); \
	dst = (((__tmp0) & _ca2) ^ _cx2); \
    } \
    else if ( ((unsigned long)(dst_adr) >= MB86290_fbstart) && ((unsigned long)(dst_adr) <= MB86290_fbend) && \
              (((unsigned long)(src_adr) < MB86290_fbstart) || ((unsigned long)(src_adr) > MB86290_fbend)) ) \
    { \
	FbBits __tmp0 = (((src) & _ca2) ^ _cx2); \
	dst = MB86290_32_SWAP(__tmp0); \
    } \
    else if ( ((unsigned long)(src_adr) >= MB86290_fbstart) && ((unsigned long)(src_adr) <= MB86290_fbend) && \
              (((unsigned long)(dst_adr) >= MB86290_fbstart) || ((unsigned long)(dst_adr) <= MB86290_fbend)) ) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(src); \
	FbBits __tmp1 = (((__tmp0) & _ca2) ^ _cx2); \
	dst = MB86290_32_SWAP(__tmp1); \
    } \
    else \
    { \
	dst = (((src) & _ca2) ^ _cx2); \
    } \
}

#define FbDoMaskMergeRop(src, dst, mask, src_adr, dst_adr) { \
    if ( ((unsigned long)(src_adr) >= MB86290_fbstart) && ((unsigned long)(src_adr) <= MB86290_fbend) && \
         (((unsigned long)(dst_adr) < MB86290_fbstart) || ((unsigned long)(dst_adr) > MB86290_fbend)) ) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(src); \
	dst = (((dst) & ((((__tmp0) & _ca1) ^ _cx1) | ~(mask))) ^ ((((__tmp0) & _ca2) ^ _cx2) & (mask))); \
    } \
    else if ( ((unsigned long)(dst_adr) >= MB86290_fbstart) && ((unsigned long)(dst_adr) <= MB86290_fbend) && \
              (((unsigned long)(src_adr) < MB86290_fbstart) || ((unsigned long)(src_adr) > MB86290_fbend)) ) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(dst); \
	FbBits __tmp1 = (((__tmp0) & ((((src) & _ca1) ^ _cx1) | ~(mask))) ^ ((((src) & _ca2) ^ _cx2) & (mask))); \
	dst = MB86290_32_SWAP(__tmp1); \
    } \
    else if ( ((unsigned long)(src_adr) >= MB86290_fbstart) && ((unsigned long)(src_adr) <= MB86290_fbend) && \
              (((unsigned long)(dst_adr) >= MB86290_fbstart) || ((unsigned long)(dst_adr) <= MB86290_fbend)) ) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(dst); \
	FbBits __tmp1 = MB86290_32_SWAP(src); \
	FbBits __tmp2 = (((__tmp0) & ((((__tmp1) & _ca1) ^ _cx1) | ~(mask))) ^ ((((__tmp1) & _ca2) ^ _cx2) & (mask))); \
	dst = MB86290_32_SWAP(__tmp2); \
    } \
    else \
    { \
	dst = (((dst) & ((((src) & _ca1) ^ _cx1) | ~(mask))) ^ ((((src) & _ca2) ^ _cx2) & (mask))); \
    } \
}

#ifndef FBNOPIXADDR
#define FbDoLeftMaskByteMergeRop(dst, src, lb, l) { \
    FbBits __xor = ((src) & _ca2) ^ _cx2; \
    FbDoLeftMaskByteRRop(dst,lb,l,((src) & _ca1) ^ _cx1,__xor); \
}
#else
#define FbDoLeftMaskByteMergeRop(dst, src, lb, l, dst_adr, src_adr) \
    FbDoMaskMergeRop(src, *dst, l, src_adr, dst_adr)
#endif

#ifndef FBNOPIXADDR
#define FbDoRightMaskByteMergeRop(dst, src, rb, r) { \
    FbBits __xor = ((src) & _ca2) ^ _cx2; \
    FbDoRightMaskByteRRop(dst,rb,r,((src) & _ca1) ^ _cx1,__xor); \
}
#else
#define FbDoRightMaskByteMergeRop(dst, src, rb, r, dst_adr, src_adr) \
    FbDoMaskMergeRop(src, *dst, r, src_adr, dst_adr)
#endif

#define FbDoRRop_Origin(dst, and, xor) \
    (((dst) & (and)) ^ (xor))

#define FbDoRRop(dst, and, xor, dst_adr) { \
    if (((unsigned long)(dst_adr) >= MB86290_fbstart) && \
	((unsigned long)(dst_adr) <= MB86290_fbend)) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(dst); \
	FbBits __tmp1 = (((__tmp0) & (and)) ^ (xor)); \
	dst = MB86290_32_SWAP(__tmp1); \
    } \
    else \
    { \
	dst = (((dst) & (and)) ^ (xor)); \
    } \
}

#define FbDoMaskRRop_Origin(dst, and, xor, mask) \
    (((dst) & ((and) | ~(mask))) ^ (xor & mask))

#define FbDoMaskRRop(dst, and, xor, mask, dst_adr) { \
    if (((unsigned long)(dst_adr) >= MB86290_fbstart) && \
	((unsigned long)(dst_adr) <= MB86290_fbend)) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(dst); \
	FbBits __tmp1 = (((__tmp0) & ((and) | ~(mask))) ^ (xor & mask)); \
	dst = MB86290_32_SWAP(__tmp1); \
    } \
    else \
    { \
	dst = (((dst) & ((and) | ~(mask))) ^ (xor & mask)); \
    } \
}

#if 0
#define FbDoWrite16(dst, src, dst_adr, src_adr) { \
    if ( (((unsigned long)(src_adr) >= MB86290_fbstart) && ((unsigned long)(src_adr) <= MB86290_fbend) && \
         (((unsigned long)(dst_adr) < MB86290_fbstart) || ((unsigned long)(dst_adr) > MB86290_fbend))) || \
         (((unsigned long)(dst_adr) >= MB86290_fbstart) && ((unsigned long)(dst_adr) <= MB86290_fbend) && \
         (((unsigned long)(src_adr) < MB86290_fbstart) || ((unsigned long)(src_adr) > MB86290_fbend))) ) \
    { \
	dst = MB86290_16_SWAP(src); \
    } \
    else \
    { \
	dst = src; \
    } \
}
#endif

#define FbDoWrite16(dst, src, adr) { \
    if (((unsigned long)(adr) >= MB86290_fbstart) && \
	((unsigned long)(adr) <= MB86290_fbend)) \
    { \
	dst = MB86290_16_SWAP(src); \
    } \
    else \
    { \
	dst = src; \
    } \
}

#if 0
#define FbDoWrite32(dst, src, dst_adr, src_adr) { \
    if ( (((unsigned long)(src_adr) >= MB86290_fbstart) && ((unsigned long)(src_adr) <= MB86290_fbend) && \
         (((unsigned long)(dst_adr) < MB86290_fbstart) || ((unsigned long)(dst_adr) > MB86290_fbend))) || \
         (((unsigned long)(dst_adr) >= MB86290_fbstart) && ((unsigned long)(dst_adr) <= MB86290_fbend) && \
         (((unsigned long)(src_adr) < MB86290_fbstart) || ((unsigned long)(src_adr) > MB86290_fbend))) ) \
    { \
	dst = MB86290_32_SWAP(src); \
    } \
    else \
    { \
	dst = src; \
    } \
}
#endif

#define FbDoWrite32(dst, src, adr) { \
    if (((unsigned long)(adr) >= MB86290_fbstart) && \
	((unsigned long)(adr) <= MB86290_fbend)) \
    { \
	dst = MB86290_32_SWAP(src); \
    } \
    else \
    { \
	dst = src; \
    } \
}

/*
 * Take a single bit (0 or 1) and generate a full mask
 */
#define fbFillFromBit(b,t)	(~((t) ((b) & 1)-1))

#define fbXorT(rop,fg,pm,t) ((((fg) & fbFillFromBit((rop) >> 1,t)) | \
			      (~(fg) & fbFillFromBit((rop) >> 3,t))) & (pm))

#define fbAndT(rop,fg,pm,t) ((((fg) & fbFillFromBit (rop ^ (rop>>1),t)) | \
			      (~(fg) & fbFillFromBit((rop>>2) ^ (rop>>3),t))) | \
			     ~(pm))

#define fbXor(rop,fg,pm)	fbXorT(rop,fg,pm,FbBits)

#define fbAnd(rop,fg,pm)	fbAndT(rop,fg,pm,FbBits)

#define fbXorStip(rop,fg,pm)    fbXorT(rop,fg,pm,FbStip)

#define fbAndStip(rop,fg,pm)	fbAndT(rop,fg,pm,FbStip)

/*
 * Stippling operations; 
 */

extern const FbBits	fbStipple16Bits[256];	/* half of table */
#define FbStipple16Bits(b) \
    (fbStipple16Bits[(b)&0xff] | fbStipple16Bits[(b) >> 8] << FB_HALFUNIT)
extern const FbBits	fbStipple8Bits[256];
extern const FbBits	fbStipple4Bits[16];
extern const FbBits	fbStipple2Bits[4];
extern const FbBits	fbStipple1Bits[2];
extern const FbBits	*const fbStippleTable[];

#define FbStippleRRop(dst, b, fa, fx, ba, bx, dst_adr) { \
    if (((unsigned long)(dst_adr) >= MB86290_fbstart) && \
	((unsigned long)(dst_adr) <= MB86290_fbend)) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(dst); \
	FbBits __tmp1 = (FbDoRRop_Origin(__tmp0, fa, fx) & b) | (FbDoRRop_Origin(__tmp0, ba, bx) & ~b); \
	dst = MB86290_32_SWAP(__tmp1); \
    } \
    else \
    { \
	dst = (FbDoRRop_Origin(dst, fa, fx) & b) | (FbDoRRop_Origin(dst, ba, bx) & ~b); \
    } \
}

#define FbStippleRRopMask(dst, b, fa, fx, ba, bx, m, dst_adr) { \
    if (((unsigned long)(dst_adr) >= MB86290_fbstart) && \
	((unsigned long)(dst_adr) <= MB86290_fbend)) \
    { \
	FbBits __tmp0 = MB86290_32_SWAP(dst); \
	FbBits __tmp1 = (FbDoMaskRRop_Origin(__tmp0, fa, fx, m) & (b)) | (FbDoMaskRRop_Origin(__tmp0, ba, bx, m) & ~(b)); \
	dst = MB86290_32_SWAP(__tmp1); \
    } \
    else \
    { \
	dst = (FbDoMaskRRop_Origin(dst, fa, fx, m) & (b)) | (FbDoMaskRRop_Origin(dst, ba, bx, m) & ~(b)); \
    } \
}

#define FbDoLeftMaskByteStippleRRop(dst, b, fa, fx, ba, bx, lb, l) { \
    FbBits  __xor = ((fx) & (b)) | ((bx) & ~(b)); \
    FbDoLeftMaskByteRRop(dst, lb, l, ((fa) & (b)) | ((ba) & ~(b)), __xor); \
}

#define FbDoRightMaskByteStippleRRop(dst, b, fa, fx, ba, bx, rb, r) { \
    FbBits  __xor = ((fx) & (b)) | ((bx) & ~(b)); \
    FbDoRightMaskByteRRop(dst, rb, r, ((fa) & (b)) | ((ba) & ~(b)), __xor); \
}

#define FbOpaqueStipple(b, fg, bg) (((fg) & (b)) | ((bg) & ~(b)))
    
/*
 * Compute rop for using tile code for 1-bit dest stipples; modifies
 * existing rop to flip depending on pixel values
 */
#define FbStipple1RopPick(alu,b)    (((alu) >> (2 - (((b) & 1) << 1))) & 3)

#define FbOpaqueStipple1Rop(alu,fg,bg)    (FbStipple1RopPick(alu,fg) | \
					   (FbStipple1RopPick(alu,bg) << 2))

#define FbStipple1Rop(alu,fg)	    (FbStipple1RopPick(alu,fg) | 4)

#endif
