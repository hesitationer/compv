/* Copyright (C) 2016-2017 Doubango Telecom <https://www.doubango.org>
* File author: Mamadou DIOP (Doubango Telecom, France).
* License: GPLv3. For commercial license please contact us.
* Source code: https://github.com/DoubangoTelecom/compv
* WebSite: http://compv.org
*/
#if !defined(_COMPV_BASE_INTRIN_SSE_H_)
#define _COMPV_BASE_INTRIN_SSE_H_

#include "compv/base/compv_config.h"
#include "compv/base/compv_common.h"

#if defined(_COMPV_API_H_)
#error("This is a private file and must not be part of the API")
#endif

COMPV_NAMESPACE_BEGIN()

// -0: Sign bit (bit-63) to 1 (https://en.wikipedia.org/wiki/Double-precision_floating-point_format) and all other bites to 0
// not(-0.) = 0x7fffffffffffffff. r=0x7fffffffffffffff doesn't fill in 32bit register and this is why we use not(-0) instead of the result.
// we can also use: _mm_and_pd(a, _mm_load_pd(reinterpret_cast<const double*>(kAVXFloat64MaskAbs)))) which doesn't override the mask in asm and is faster
#define _mm_abs_pd_SSE2(a) _mm_andnot_pd(_mm_set1_pd(-0.), a)

static COMPV_INLINE __m128i _mm_mullo_epi32_SSE2(const __m128i &a, const __m128i &b)
{
    __m128i x, y;
    _mm_store_si128(&x, _mm_mul_epu32(a, b));
    _mm_store_si128(&y, _mm_mul_epu32(_mm_srli_si128(a, 4), _mm_srli_si128(b, 4)));
    return _mm_unpacklo_epi32(_mm_shuffle_epi32(x, 0x8), _mm_shuffle_epi32(y, 0x8));
}

#define _mm_cvtepi16_epi32_low_SSE2(a) _mm_srai_epi32(_mm_unpacklo_epi16(a, a), 16)
#define _mm_cvtepi16_epi32_hi_SSE2(a) _mm_srai_epi32(_mm_unpackhi_epi16(a, a), 16)

/*
; Macro used to convert 16 RGB to 16 RGBA samples
; 16 RGB samples requires 48 Bytes(3 XMM registers), will be converted to 16 RGBA samples
; requiring 64 Bytes (4 XMM registers)
; The aplha channel will contain garbage instead of 0xff because this macro is used to fetch samples in place
*/
#define COMPV_16xRGB_TO_16xRGBA_SSSE3_FAST(rgbPtr_, ymmRGBA0_, ymmRGBA1_, ymmRGBA2_, ymmRGBA3_, ymmMaskRgbToRgba_) \
	_mm_store_si128(&ymmRGBA3_, _mm_load_si128(reinterpret_cast<const __m128i*>(rgbPtr + 32))); \
	_mm_store_si128(&ymmRGBA0_, _mm_shuffle_epi8(_mm_load_si128(reinterpret_cast<const __m128i*>(rgbPtr_ + 0)), ymmMaskRgbToRgba_)); \
	_mm_store_si128(&ymmRGBA1_, _mm_alignr_epi8(_mm_load_si128(reinterpret_cast<const __m128i*>(rgbPtr_ + 16)), _mm_load_si128(reinterpret_cast<const __m128i*>(rgbPtr_ + 0)), 12)); \
	_mm_store_si128(&ymmRGBA1_, _mm_shuffle_epi8(ymmRGBA1_, ymmMaskRgbToRgba_)); \
	_mm_store_si128(&ymmRGBA2_, _mm_alignr_epi8(_mm_load_si128(reinterpret_cast<const __m128i*>(rgbPtr_ + 32)), _mm_load_si128(reinterpret_cast<const __m128i*>(rgbPtr_ + 16)), 8)); \
	_mm_store_si128(&ymmRGBA2_, _mm_shuffle_epi8(ymmRGBA2_, ymmMaskRgbToRgba_)); \
	_mm_store_si128(&ymmRGBA3_, _mm_alignr_epi8(ymmRGBA3_, ymmRGBA3_, 4)); \
	_mm_store_si128(&ymmRGBA3_, _mm_shuffle_epi8(ymmRGBA3_, ymmMaskRgbToRgba_));
// Next version not optimized as we load the masks for each call, use above version and load masks once
#define COMPV_16xRGB_TO_16xRGBA_SSSE3_SLOW(rgbPtr_, ymmRGBA0_, ymmRGBA1_, ymmRGBA2_, ymmRGBA3_) \
	COMPV_16xRGB_TO_16xRGBA_SSSE3_FAST(rgbPtr_, ymmRGBA0_, ymmRGBA1_, ymmRGBA2_, ymmRGBA3_, _mm_load_si128(reinterpret_cast<const __m128i*>(kShuffleEpi8_RgbToRgba_i32)))


/*
Interleaves two 128bits vectors.
From:
0 0 0 0 0 0 . . . .
1 1 1 1 1 1 . . . .
To:
0 1 0 1 0 1 . . . .
*/
#define COMPV_INTERLEAVE_I8_SSE2(_m0, _m1, _tmp) \
	_mm_store_si128(&_tmp, _mm_unpackhi_epi8(_m0, _m1)); \
	_mm_store_si128(&_m0, _mm_unpacklo_epi8(_m0, _m1)); \
	_mm_store_si128(&_m1, _tmp);

/*
Transpose a 4x16 matrix containing u8/i8 values.
From:
0 0 0 0 . .
1 1 1 1 . .
2 2 2 2 . .
3 3 3 3 . .
To:
0 1 2 3 . .
0 1 2 3 . .
0 1 2 3 . .
*/
#define COMPV_TRANSPOSE_I8_4X16_SSE2(_x0, _x1, _x2, _x3, _tmp) \
	COMPV_INTERLEAVE_I8_SSE2(_x0, _x2, _tmp) \
	COMPV_INTERLEAVE_I8_SSE2(_x1, _x3, _tmp) \
	COMPV_INTERLEAVE_I8_SSE2(_x0, _x1, _tmp) \
	COMPV_INTERLEAVE_I8_SSE2(_x2, _x3, _tmp)


/*
Transpose a 16x16 matrix containing u8/i8 values.
From:
0 0 0 0 . .
1 1 1 1 . .
2 2 2 2 . .
3 3 3 3 . .
To:
0 1 2 3 . .
0 1 2 3 . .
0 1 2 3 . .
*/
#define COMPV_TRANSPOSE_I8_16X16_SSE2(_x0, _x1, _x2, _x3, _x4, _x5, _x6, _x7, _x8, _x9, _x10, _x11, _x12, _x13, _x14, _x15, _tmp) \
	/* 1 * 5 * 9 * d */ \
	COMPV_TRANSPOSE_I8_4X16_SSE2(_x1, _x5, _x9, _x13, _tmp); \
	/* 3 * 7 * b * f */ \
	COMPV_TRANSPOSE_I8_4X16_SSE2(_x3, _x7, _x11, _x15, _tmp); \
	/* 0 * 4 * 8 * c */ \
	COMPV_TRANSPOSE_I8_4X16_SSE2(_x0, _x4, _x8, _x12, _tmp); \
	/* 2 * 6 * a * e */ \
	COMPV_TRANSPOSE_I8_4X16_SSE2(_x2, _x6, _x10, _x14, _tmp); \
	/* 1 * 3 * 5 * 7 * 9 * b * d * f */ \
	COMPV_INTERLEAVE_I8_SSE2(_x1, _x3, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x5, _x7, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x9, _x11, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x13, _x15, _tmp); \
	/* 0 * 2 * 4 * 6 * 8 * a * c * e */ \
	COMPV_INTERLEAVE_I8_SSE2(_x0, _x2, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x4, _x6, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x8, _x10, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x12, _x14, _tmp); \
	/* 0 * 1 * 2 * 3 * 4 * 5 * 6 * 7 * 8 * 9 * a * b * c * d * e * f */ \
	COMPV_INTERLEAVE_I8_SSE2(_x0, _x1, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x2, _x3, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x4, _x5, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x6, _x7, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x8, _x9, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x10, _x11, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x12, _x13, _tmp); \
	COMPV_INTERLEAVE_I8_SSE2(_x14, _x15, _tmp);

COMPV_NAMESPACE_END()

#endif /* _COMPV_BASE_INTRIN_SSE_H_ */