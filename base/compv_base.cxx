/* Copyright (C) 2016-2017 Doubango Telecom <https://www.doubango.org>
* File author: Mamadou DIOP (Doubango Telecom, France).
* License: GPLv3. For commercial license please contact us.
* Source code: https://github.com/DoubangoTelecom/compv
* WebSite: http://compv.org
*/
#include "compv/base/compv_base.h"
#include "compv/base/compv_cpu.h"
#include "compv/base/compv_debug.h"
#include "compv/base/time/compv_time.h"
#include "compv/base/math/compv_math_utils.h"
#include "compv/base/parallel/compv_parallel.h"
#include "compv/base/image/compv_image_decoder.h"

COMPV_NAMESPACE_BEGIN()

bool CompVBase::s_bInitialized = false;
bool CompVBase::s_bInitializing = false;
#if defined(COMPV_OS_WINDOWS)
bool CompVBase::s_bBigEndian = false;
#else
bool CompVBase::s_bBigEndian = true;
#endif
bool CompVBase::s_bTesting = false;
bool CompVBase::s_bMathTrigFast = true;
bool CompVBase::s_bMathFixedPoint = true;

CompVBase::CompVBase()
{

}

CompVBase::~CompVBase()
{

}

COMPV_ERROR_CODE CompVBase::init(int32_t numThreads /*= -1*/)
{
	if (s_bInitialized || s_bInitializing) {
		return COMPV_ERROR_CODE_S_OK;
	}

	COMPV_ERROR_CODE err_ = COMPV_ERROR_CODE_S_OK;
	s_bInitializing = true;

	COMPV_DEBUG_INFO("Initializing [base] modules (v %s)...", COMPV_VERSION_STRING);

	// Make sure sizeof(compv_scalar_t) is correct
#if defined(COMPV_ASM) || defined(COMPV_INTRINSIC)
	if (sizeof(compv_scalar_t) != sizeof(void*)) {
		COMPV_DEBUG_ERROR("sizeof(compv_scalar_t)= #%zu not equal to sizeof(void*)= #%zu", sizeof(compv_scalar_t), sizeof(void*));
		return COMPV_ERROR_CODE_E_SYSTEM;
	}
	// https://en.wikipedia.org/wiki/Single-precision_floating-point_format
	if (sizeof(compv_float32_t) != 4) {
		COMPV_DEBUG_ERROR("sizeof(compv_float32_t)= #%zu not equal to 4", sizeof(compv_float32_t));
		return COMPV_ERROR_CODE_E_SYSTEM;
	}
	// https://en.wikipedia.org/wiki/Double-precision_floating-point_format
	if (sizeof(compv_float64_t) != 8) {
		COMPV_DEBUG_ERROR("sizeof(compv_float64_t)= #%zu not equal to 8", sizeof(compv_float64_t));
		return COMPV_ERROR_CODE_E_SYSTEM;
	}
#endif
	COMPV_DEBUG_INFO("sizeof(compv_scalar_t)= #%zu", sizeof(compv_scalar_t));
	COMPV_DEBUG_INFO("sizeof(float)= #%zu", sizeof(float));

	// endianness
	// https://developer.apple.com/library/mac/documentation/Darwin/Conceptual/64bitPorting/MakingCode64-BitClean/MakingCode64-BitClean.html
#if TARGET_RT_LITTLE_ENDIAN
	s_bBigEndian = false;
#elif TARGET_RT_BIG_ENDIAN
	s_bBigEndian = true;
#else
	static const short kWord = 0x4321;
	s_bBigEndian = ((*(int8_t *)&kWord) != 0x21);
#	if defined(COMPV_OS_WINDOWS)
	if (s_bBigEndian) {
		COMPV_DEBUG_WARN("Big endian on Windows machine. Is it right?");
	}
#	endif
#endif

	// rand()
	srand((unsigned int)CompVTime::getNowMills());

	/* Make sure heap debugging is disabled (release mode only) */
#if COMPV_OS_WINDOWS && defined(_MSC_VER) && defined(NDEBUG)
	if (IsDebuggerPresent()) {
		// TODO(dmi): Looks like this feature is OFF (by default) on VS2015
		DWORD size = GetEnvironmentVariable(TEXT("_NO_DEBUG_HEAP"), NULL, 0);
		bool bHeapDebuggingDisabled = false;
		if (size) {
			TCHAR* _NO_DEBUG_HEAP = (TCHAR*)CompVMem::malloc(size * sizeof(TCHAR));
			if (_NO_DEBUG_HEAP) {
				size = GetEnvironmentVariable(TEXT("_NO_DEBUG_HEAP"), _NO_DEBUG_HEAP, size);
				if (size) {
					bHeapDebuggingDisabled = (_NO_DEBUG_HEAP[0] == TEXT('1'));
				}
				CompVMem::free((void**)&_NO_DEBUG_HEAP);
			}
		}
		if (!bHeapDebuggingDisabled) {
			COMPV_DEBUG_INFO("/!\\ Heap debugging enabled on release mode while running your app from Visual Studio. You may experiment performance issues.\n"
				"Consider disabling this feature: Configuration Properties->Debugging->Environment: _NO_DEBUG_HEAP=1\n"
				"Must be set on the app (executable) itself.");
		}
	}
#endif

	/* Print Android API version */
#if COMPV_OS_ANDROID
	COMPV_DEBUG_INFO("[Base] module: android API version: %d", __ANDROID_API__);
#endif

	/* Image handlers initialization */
	COMPV_CHECK_CODE_BAIL(err_ = CompVImageDecoder::init());

	/* CPU features initialization */
	COMPV_CHECK_CODE_BAIL(err_ = CompVCpu::init());
	COMPV_DEBUG_INFO("CPU features: %s", CompVCpu::getFlagsAsString(CompVCpu::getFlags()));
	COMPV_DEBUG_INFO("CPU cores: #%d", CompVCpu::getCoresCount());
	COMPV_DEBUG_INFO("CPU cache1: line size: #%dB, size :#%dKB", CompVCpu::getCache1LineSize(), CompVCpu::getCache1Size() >> 10);
#if defined(COMPV_ARCH_X86)
	// even if we are on X64 CPU it's possible that we're running a 32-bit binary
#	if defined(COMPV_ARCH_X64)
	COMPV_DEBUG_INFO("Binary type: X86_64");
#	else
	COMPV_DEBUG_INFO("Binary type: X86_32");
	if (CompVCpu::isSupported(kCpuFlagX64)) {
		COMPV_DEBUG_INFO("/!\\Using 32bits binaries on 64bits machine: optimization issues");
	}
#	endif
#endif
#if defined(COMPV_INTRINSIC)
	COMPV_DEBUG_INFO("Intrinsic enabled");
#endif
#if defined(COMPV_ASM)
	COMPV_DEBUG_INFO("Assembler enabled");
#endif
#if defined __INTEL_COMPILER
	COMPV_DEBUG_INFO("Using Intel compiler");
#endif
	// https://msdn.microsoft.com/en-us/library/jj620901.aspx
#if defined(__AVX2__)
	COMPV_DEBUG_INFO("Code built with option /arch:AVX2");
#endif
#if defined(__AVX__)
	COMPV_DEBUG_INFO("Code built with option /arch:AVX");
#endif
#if defined(__FMA3__)
	COMPV_DEBUG_INFO("Code built with option /arch:FMA3");
#endif
#if defined(__SSE__)
	COMPV_DEBUG_INFO("Code built with option /arch:SSE");
#endif
#if defined(__SSE2__)
	COMPV_DEBUG_INFO("Code built with option /arch:SSE2");
#endif
#if defined(__SSE3__)
	COMPV_DEBUG_INFO("Code built with option /arch:SSE3");
#endif
#if defined(__SSSE3__)
	COMPV_DEBUG_INFO("Code built with option /arch:SSSE3");
#endif
#if defined(__SSE4_1__)
	COMPV_DEBUG_INFO("Code built with option /arch:SSE41");
#endif
#if defined(__SSE4_2__)
	COMPV_DEBUG_INFO("Code built with option /arch:SSE42");
#endif
#if defined(__ARM_NEON__)
	COMPV_DEBUG_INFO("Code built with option /arch:NEON");
#endif

	/* Math functions: Must be after CPU initialization */
	COMPV_CHECK_CODE_BAIL(err_ = CompVMathUtils::init());
	COMPV_DEBUG_INFO("Math Fast Trig.: %s", CompVBase::isMathTrigFast() ? "true" : "fast");
	COMPV_DEBUG_INFO("Math Fixed Point: %s", CompVBase::isMathFixedPoint() ? "true" : "fast");

	/* Memory alignment */
	COMPV_DEBUG_INFO("Default alignment: #%d", COMPV_SIMD_ALIGNV_DEFAULT);
	COMPV_DEBUG_INFO("Best alignment: #%d", CompVMem::getBestAlignment());

	/* Memory management */
	COMPV_CHECK_CODE_BAIL(err_ = CompVMem::init());

#if 0
	/* Features */
	COMPV_CHECK_CODE_BAIL(err_ = CompVFeature::init());
#endif

#if 0
	/* Matchers */
	COMPV_CHECK_CODE_BAIL(err_ = CompVMatcher::init());
#endif

bail:
	s_bInitialized = COMPV_ERROR_CODE_IS_OK(err_);
	s_bInitializing = false;
	if (s_bInitialized) {
		// The next functions are called here because they recursively call "CompVBase::init()"
		// We call them now because "s_bInitialized" is already set to "true" and this is the way to avoid endless loops

		/* Parallel */
		COMPV_CHECK_CODE_BAIL(err_ = CompVParallel::init(numThreads)); // If error will go back to bail then "s_bInitialized" wil be set to false which means deInit() will be called
	}
	else {
		// cleanup if initialization failed
		CompVParallel::deInit();
	}

	COMPV_DEBUG_INFO("Base modules initialized");
	return err_;
}

COMPV_ERROR_CODE CompVBase::deInit()
{
	COMPV_DEBUG_INFO("DeInitializing base modules (v %s)...", COMPV_VERSION_STRING);

	s_bInitialized = false;
	s_bInitializing = false;

	CompVParallel::deInit();

	// TODO(dmi): deInit other modules (not an issue because there is no memory allocation)
	CompVMem::deInit();
	
	CompVImageDecoder::deInit();

	COMPV_DEBUG_INFO("Base modules deinitialized");

	return COMPV_ERROR_CODE_S_OK;
}


COMPV_ERROR_CODE CompVBase::setTestingModeEnabled(bool bTesting)
{
	COMPV_DEBUG_INFO("Engine testing mode = %s", bTesting ? "true" : "false");
	s_bTesting = bTesting;
	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVBase::setMathTrigFastEnabled(bool bMathTrigFast)
{
	COMPV_DEBUG_INFO("Engine math trig. fast = %s", bMathTrigFast ? "true" : "false");
	s_bMathTrigFast = bMathTrigFast;
	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVBase::setMathFixedPointEnabled(bool bMathFixedPoint)
{
	COMPV_DEBUG_INFO("Engine math trig. fast = %s", bMathFixedPoint ? "true" : "false");
	s_bMathFixedPoint = bMathFixedPoint;
	return COMPV_ERROR_CODE_S_OK;
}


bool CompVBase::isInitialized()
{
	return s_bInitialized;
}

bool CompVBase::isInitializing()
{
	return s_bInitializing;
}

bool CompVBase::isBigEndian()
{
	return s_bBigEndian;
}

bool CompVBase::isTestingMode()
{
	return s_bTesting;
}

bool CompVBase::isMathTrigFast()
{
	return s_bMathTrigFast;
}

bool CompVBase::isMathFixedPoint()
{
	return s_bMathFixedPoint;
}

COMPV_NAMESPACE_END()
