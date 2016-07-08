/* Copyright (C) 2016-2017 Doubango Telecom <https://www.doubango.org>
* File author: Mamadou DIOP (Doubango Telecom, France).
* License: GPLv3. For commercial license please contact us.
* Source code: https://github.com/DoubangoTelecom/compv
* WebSite: http://compv.org
*/
#include "compv/math/compv_math_utils.h"
#include "compv/compv_cpu.h"

#include "compv/intrinsics/x86/math/compv_math_utils_intrin_sse2.h"
#include "compv/intrinsics/x86/math/compv_math_utils_intrin_ssse3.h"
#include "compv/intrinsics/x86/math/compv_math_utils_intrin_avx2.h"

#include <algorithm>

#if COMPV_ARCH_X86 && COMPV_ASM
COMPV_EXTERNC void MathUtilsAddAbs_16i16u_Asm_X86_SSSE3(const COMPV_ALIGNED(SSE) int16_t* a, const COMPV_ALIGNED(SSE) int16_t* b, COMPV_ALIGNED(SSE) uint16_t* r, compv::compv_uscalar_t width, compv::compv_uscalar_t height, COMPV_ALIGNED(SSE) compv::compv_uscalar_t stride);
COMPV_EXTERNC void MathUtilsAddAbs_16i16u_Asm_X86_AVX2(const COMPV_ALIGNED(AVX) int16_t* a, const COMPV_ALIGNED(AVX) int16_t* b, COMPV_ALIGNED(AVX) uint16_t* r, compv::compv_uscalar_t width, compv::compv_uscalar_t height, COMPV_ALIGNED(AVX) compv::compv_uscalar_t stride);
#endif /* COMPV_ARCH_X86 && COMPV_ASM */

COMPV_NAMESPACE_BEGIN()

bool CompVMathUtils::s_Initialized = false;

static compv_scalar_t maxVal_C(compv_scalar_t x, compv_scalar_t y);
static compv_scalar_t minVal_C(compv_scalar_t x, compv_scalar_t y);
static int32_t minArrayI32_C(const int32_t* array, compv_scalar_t count);
static compv_scalar_t clip3_C(compv_scalar_t min, compv_scalar_t max, compv_scalar_t val);
static compv_scalar_t clip2_C(compv_scalar_t max, compv_scalar_t val);
static void rand_C(uint32_t* r, compv_scalar_t count);

#if COMPV_ARCH_X86 && COMPV_ASM
COMPV_EXTERNC compv_scalar_t compv_mathutils_maxval_asm_x86_cmov(compv_scalar_t x, compv_scalar_t y);
COMPV_EXTERNC compv_scalar_t compv_mathutils_minval_asm_x86_cmov(compv_scalar_t x, compv_scalar_t y);
COMPV_EXTERNC compv_scalar_t compv_mathutils_clip3_asm_x86_cmov(compv_scalar_t min, compv_scalar_t max, compv_scalar_t val);
COMPV_EXTERNC compv_scalar_t compv_mathutils_clip2_asm_x86_cmov(compv_scalar_t max, compv_scalar_t val);
COMPV_EXTERNC void compv_mathutils_rand_asm_x86_rdrand(uint32_t* r, compv_scalar_t count);
#endif

compv_scalar_t(*CompVMathUtils::maxValFunc)(compv_scalar_t a, compv_scalar_t b) = maxVal_C;
compv_scalar_t(*CompVMathUtils::minValFunc)(compv_scalar_t a, compv_scalar_t b) = minVal_C;
int32_t(*CompVMathUtils::minArrayI32Func)(const int32_t* array, compv_scalar_t count) = minArrayI32_C;
compv_scalar_t(*CompVMathUtils::clip3Func)(compv_scalar_t min, compv_scalar_t max, compv_scalar_t val) = clip3_C;
compv_scalar_t(*CompVMathUtils::clip2Func)(compv_scalar_t max, compv_scalar_t val) = clip2_C;
void(*CompVMathUtils::randFunc)(uint32_t* r, compv_scalar_t count) = rand_C;

COMPV_ERROR_CODE CompVMathUtils::init()
{
    COMPV_ERROR_CODE err_ = COMPV_ERROR_CODE_S_OK;

    if (!CompVMathUtils::s_Initialized) {
        if (CompVCpu::isEnabled(kCpuFlagCMOV)) {
            COMPV_EXEC_IFDEF_ASM_X86(CompVMathUtils::maxValFunc = compv_mathutils_maxval_asm_x86_cmov);
            COMPV_EXEC_IFDEF_ASM_X86(CompVMathUtils::minValFunc = compv_mathutils_minval_asm_x86_cmov);
            COMPV_EXEC_IFDEF_ASM_X86(CompVMathUtils::clip3Func = compv_mathutils_clip3_asm_x86_cmov);
            COMPV_EXEC_IFDEF_ASM_X86(CompVMathUtils::clip2Func = compv_mathutils_clip2_asm_x86_cmov);
        }
#if 0
		// RDRAND isn't a PRNG but a TRNG and is slower than ANSI's rand()
		// https://software.intel.com/en-us/articles/intel-digital-random-number-generator-drng-software-implementation-guide/
		if (CompVCpu::isEnabled(kCpuFlagRDRAND)) {
			COMPV_EXEC_IFDEF_ASM_X86(randFunc = compv_mathutils_rand_asm_x86_rdrand);
		}
#endif
        CompVMathUtils::s_Initialized = true;
    }

    return err_;
}

static compv_scalar_t maxVal_C(compv_scalar_t x, compv_scalar_t y)
{
    COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED();
    return std::max(x, y);
}

static compv_scalar_t minVal_C(compv_scalar_t x, compv_scalar_t y)
{
    COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED();
    return std::min(x, y);
}

static int32_t minArrayI32_C(const int32_t* array, compv_scalar_t count)
{
    COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED();
    int32_t min = array[0];
    for (compv_scalar_t i = 1; i < count; ++i) {
        min = std::min(min, array[i]);
    }
    return min;
}

compv_scalar_t clip3_C(compv_scalar_t min, compv_scalar_t max, compv_scalar_t val)
{
    COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED();
    return CompVMathUtils::maxVal(min, CompVMathUtils::minVal(val, max));
}

compv_scalar_t clip2_C(compv_scalar_t max, compv_scalar_t val)
{
    return clip3_C(0, max, val);
}

void rand_C(uint32_t* r, compv_scalar_t count)
{
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED();
	compv_scalar_t i = 0;
	for (i = 0; i < count - 3; i += 4) {
		r[i] = static_cast<uint32_t>(rand());
		r[i + 1] = static_cast<uint32_t>(rand());
		r[i + 2] = static_cast<uint32_t>(rand());
		r[i + 3] = static_cast<uint32_t>(rand());
	}
	for (; i < count; i += 1) {
		r[i] = static_cast<uint32_t>(rand());
	}
}

template <>
COMPV_ERROR_CODE CompVMathUtils::addAbs(const int16_t* a, const int16_t* b, uint16_t*& r, size_t width, size_t height, size_t stride)
{
	if (!r) {
		r = (uint16_t*)CompVMem::malloc(height * stride * sizeof(uint16_t));
		COMPV_CHECK_EXP_RETURN(!r, COMPV_ERROR_CODE_E_OUT_OF_MEMORY);
	}
#if COMPV_ARCH_X86
	const size_t strideInBytes = stride * sizeof(int16_t);
	void(*MathUtilsAddAbs_16i16u)(const COMPV_ALIGNED(X) int16_t* a, const COMPV_ALIGNED(X) int16_t* b, COMPV_ALIGNED(X) uint16_t* r, compv_uscalar_t width, compv_uscalar_t height, COMPV_ALIGNED(X) compv_uscalar_t stride) = NULL;
	if (width >= 8 && COMPV_IS_ALIGNED_SSE(strideInBytes) && COMPV_IS_ALIGNED_SSE(a) && COMPV_IS_ALIGNED_SSE(b) && COMPV_IS_ALIGNED_SSE(r)) {
		if (CompVCpu::isEnabled(compv::kCpuFlagSSSE3)) {
			COMPV_EXEC_IFDEF_INTRIN_X86(MathUtilsAddAbs_16i16u = MathUtilsAddAbs_16i16u_Intrin_SSSE3);
			COMPV_EXEC_IFDEF_ASM_X86(MathUtilsAddAbs_16i16u = MathUtilsAddAbs_16i16u_Asm_X86_SSSE3);
		}
	}
	if (width >= 16 && COMPV_IS_ALIGNED_AVX(strideInBytes) && COMPV_IS_ALIGNED_AVX(a) && COMPV_IS_ALIGNED_AVX(b) && COMPV_IS_ALIGNED_AVX(r)) {
		if (CompVCpu::isEnabled(compv::kCpuFlagAVX2)) {
			//COMPV_EXEC_IFDEF_INTRIN_X86(MathUtilsAddAbs_16i16u = MathUtilsAddAbs_16i16u_Intrin_AVX2);
			//COMPV_EXEC_IFDEF_ASM_X86(MathUtilsAddAbs_16i16u = MathUtilsAddAbs_16i16u_Asm_X86_AVX2);
		}
	}
	if (MathUtilsAddAbs_16i16u) {
		MathUtilsAddAbs_16i16u((const int16_t*)a, (const int16_t*)b, (uint16_t*)r, (compv_uscalar_t)width, (compv_uscalar_t)height, (compv_uscalar_t)stride);
		return COMPV_ERROR_CODE_S_OK;
	}
#endif /* COMPV_ARCH_X86 */

	COMPV_CHECK_CODE_RETURN((CompVMathUtils::addAbs_C<int16_t, uint16_t>(a, b, r, width, height, stride)));
	return COMPV_ERROR_CODE_S_OK;
}

COMPV_NAMESPACE_END()
