/* Copyright (C) 2016-2018 Doubango Telecom <https://www.doubango.org>
* File author: Mamadou DIOP (Doubango Telecom, France).
* License: GPLv3. For commercial license please contact us.
* Source code: https://github.com/DoubangoTelecom/compv
* WebSite: http://compv.org
*/

/* @description
This class implement LSL (Light Speed Labeling) algorithm.
Some literature about LSL:
- Light Speed Labeling: https://www.lri.fr/~lacas/Publications/JRTIP10.pdf
- Parallel Light Speed Labeling: https://hal.archives-ouvertes.fr/hal-01361188/document
*/

#include "compv/core/ccl/compv_core_ccl_lsl.h"
#include "compv/core/compv_core.h"
#include "compv/base/parallel/compv_parallel.h"
#include "compv/base/compv_cpu.h"

#include "compv/core/ccl/intrin/x86/compv_core_ccl_lsl_intrin_sse2.h"

#define COMPV_CCL_LSL_STEP1_MIN_SAMPLES_PER_THREAD	(20*20)
#define COMPV_CCL_LSL_STEP20_MIN_SAMPLES_PER_THREAD	(30*30)

#define COMPV_THIS_CLASSNAME	"CompVConnectedComponentLabelingLSL"

COMPV_NAMESPACE_BEGIN()

// X64
#if COMPV_ASM && COMPV_ARCH_X64
COMPV_EXTERNC void CompVConnectedComponentLabelingLSL_Step1Algo13SegmentRLE_8u32s_Asm_X64_CMOV(const uint8_t* Xi, int32_t* RLCi, int32_t* ERi, int32_t* b1, int32_t* er1, const int32_t width);
#endif /* COMPV_ASM && COMPV_ARCH_X64 */

static const compv_ccl_indice_t kCompVConnectedComponentLabelingLSLBachgroundLabel = 0; // Must be zero because of calloc()

CompVConnectedComponentLabelingLSL::CompVConnectedComponentLabelingLSL()
	:CompVConnectedComponentLabeling(static_cast<compv_ccl_indice_t>(COMPV_LSL_ID))
{

}

CompVConnectedComponentLabelingLSL::~CompVConnectedComponentLabelingLSL()
{

}

COMPV_ERROR_CODE CompVConnectedComponentLabelingLSL::set(int id, const void* valuePtr, size_t valueSize) /*Overrides(CompVCaps)*/
{
	COMPV_CHECK_EXP_RETURN(!valuePtr || !valueSize, COMPV_ERROR_CODE_E_INVALID_PARAMETER);
	switch (id) {
	case COMPV_LSL_SET_INT_TYPE: {
		COMPV_CHECK_EXP_RETURN(valueSize != sizeof(int), COMPV_ERROR_CODE_E_INVALID_PARAMETER);
		int type = *reinterpret_cast<const int*>(valuePtr);

		return COMPV_ERROR_CODE_S_OK;
	}
	default:
		COMPV_DEBUG_ERROR_EX(COMPV_THIS_CLASSNAME, "Set with id %d not implemented", id);
		return COMPV_ERROR_CODE_E_NOT_IMPLEMENTED;
	}
}

template<typename T>
static void CompVConnectedComponentLabelingLSL_Step1Algo13SegmentRLE_C(const T* Xi, compv_ccl_indice_t* RLCi, compv_ccl_indice_t* ERi, compv_ccl_indice_t* b1, compv_ccl_indice_t* er1, const compv_ccl_indice_t width)
{
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No SIMD or GPU implementation could be found");
#define SET_RLC_1(mm, ii) \
		if ((mm)) { \
			RLCi[er] = ((ii) - b); \
			b ^= 1;  \
			++er; \
		} \
		ERi[(ii)] = er

	compv_ccl_indice_t i, b = *b1, er = *er1;
	const compv_ccl_indice_t width4 = width & -4;
	for (i = 1; i < width4; i += 4) {
		SET_RLC_1(Xi[i - 1] ^ Xi[i], i);
		SET_RLC_1(Xi[i] ^ Xi[i + 1], i + 1);
		SET_RLC_1(Xi[i + 1] ^ Xi[i + 2], i + 2);
		SET_RLC_1(Xi[i + 2] ^ Xi[i + 3], i + 3);
	}
	for (; i < width; ++i) {
		SET_RLC_1(Xi[i - 1] ^ Xi[i], i);
	}
	*b1 = b;
	*er1 = er;
}

// Relative segment labeling: step#1
// Algorithm 12: LSL segment detection STD
// Xi: a binary line of width w (allowed values: 0x01, 0xff, 0x00)
// ERi, an associative table of size w holding the relative labels er associated to Xi
// RLCi, a table holding the run length coding of segments of the line Xi
// ner, the number of segments of ERi � black + white
template<typename T>
static void step1_algo13_segment_RLE(const CompVMatPtr& X, CompVMatPtr ER, CompVMatPtr RLC, CompVMatPtr ner, compv_ccl_indice_t* ner_max1, compv_ccl_indice_t* ner_sum1, const compv_ccl_indice_t w, const compv_ccl_indice_t start, const compv_ccl_indice_t end)
{
	const T* Xi = X->ptr<T>(start);
	compv_ccl_indice_t* ERi = ER->ptr<compv_ccl_indice_t>(start);
	compv_ccl_indice_t* RLCi = RLC->ptr<compv_ccl_indice_t>(start);
	compv_ccl_indice_t* ner0 = ner->ptr<compv_ccl_indice_t>(0);

	const size_t X_stride = X->stride();
	const size_t ER_stride = ER->stride();
	const size_t RLC_stride = RLC->stride();

	compv_ccl_indice_t ner_max = 0;
	compv_ccl_indice_t ner_sum = 0;

	/* Hook to processing function */
	typedef void(*FunPtr)(const T* Xi, compv_ccl_indice_t* RLCi, compv_ccl_indice_t* ERi, compv_ccl_indice_t* b1, compv_ccl_indice_t* er1, const compv_ccl_indice_t width);
	FunPtr funPtr = [](const T* Xi, compv_ccl_indice_t* RLCi, compv_ccl_indice_t* ERi, compv_ccl_indice_t* b1, compv_ccl_indice_t* er1, const compv_ccl_indice_t width) {
		CompVConnectedComponentLabelingLSL_Step1Algo13SegmentRLE_C<T >(Xi, RLCi, ERi, b1, er1, width);
	};

	if (std::is_same<T, uint8_t>::value && std::is_same<compv_ccl_indice_t, int32_t>::value) {
		void(*funPtr_8u_32s)(const uint8_t* Xi, int32_t* RLCi, int32_t* ERi, int32_t* b1, int32_t* er1, const int32_t width)
			= nullptr;
#if COMPV_ARCH_X86
		if (CompVCpu::isEnabled(kCpuFlagSSE2) && X->isAlignedSSE()) {
			COMPV_EXEC_IFDEF_INTRIN_X86(funPtr_8u_32s = CompVConnectedComponentLabelingLSL_Step1Algo13SegmentRLE_8u32s_Intrin_SSE2);
			//COMPV_EXEC_IFDEF_ASM_X64(funPtr_8u_32s = nullptr);
		}
#elif COMPV_ARCH_ARM
		if (CompVCpu::isEnabled(kCpuFlagARM_NEON) && X->isAlignedNEON()) {
			//COMPV_EXEC_IFDEF_INTRIN_ARM(FunPtr_8u_32s = nullptr);
			//COMPV_EXEC_IFDEF_ASM_ARM32(FunPtr_8u_32s = nullptr);
			//COMPV_EXEC_IFDEF_ASM_ARM64(FunPtr_8u_32s = nullptr);
		}
#endif
		if (funPtr_8u_32s) {
			funPtr = reinterpret_cast<FunPtr>(funPtr_8u_32s);
		}
	}

	/* Loop through the rows */
	for (compv_ccl_indice_t j = start; j < end; ++j) {
		/* For i = 0 */
		compv_ccl_indice_t b = Xi[0] & 1; // right border compensation
		compv_ccl_indice_t er = b; // a relative label
		RLCi[0] = 0;
		ERi[0] = er;
		/* i = 1....w */
#if 1
		//FIXME(dmi): for testing
		CompVConnectedComponentLabelingLSL_Step1Algo13SegmentRLE_8u32s_Asm_X64_CMOV(Xi, RLCi, ERi, &b, &er, w);
#else
		funPtr(Xi, RLCi, ERi, &b, &er, w);
#endif
		/* update las RLCi and ner */
		RLCi[er] = (w - b);
		const compv_ccl_indice_t ner0j = er + (Xi[w - 1] & 1);
		ner0[j] = ner0j;
		ner_sum += ner0j;
		if (ner_max < ner0j) {
			ner_max = ner0j;
		}
		/* next */
		Xi += X_stride;
		ERi += ER_stride;
		RLCi += RLC_stride;
	}

	*ner_max1 = ner_max;
	*ner_sum1 = ner_sum;
}

// 2.2 Equivalence construction: step#2.0 (MT friendly)
// Algorithm 14: LSL equivalence construction
// ERi, an associative table of size w holding the relative labels er associated to Xi
// RLCi, a table holding the run length coding of segments of the line Xi
// ERAi, an associative table holding the association between er and ea: ea = ERAi[er]
static void step20_algo14_equivalence_build(const CompVMatPtr& ER, const CompVMatPtr& RLC, const CompVMatPtr& ner, CompVMatPtr ERA, const compv_ccl_indice_t w, const compv_ccl_indice_t start, const compv_ccl_indice_t end)
{
	const compv_ccl_indice_t jstart = !start ? 1 : start;
	if (!start) {
		const compv_ccl_indice_t ner0 = *ner->ptr<const compv_ccl_indice_t>(0);
		compv_ccl_indice_t* ERA0 = ERA->ptr<compv_ccl_indice_t>(0);
		for (compv_ccl_indice_t er = 1; er < ner0; er += 2) {
			ERA0[er] = 0;
		}
	}

	const compv_ccl_indice_t* ERi = ER->ptr<const compv_ccl_indice_t>(jstart);
	const compv_ccl_indice_t* ERiminus1 = ER->ptr<const compv_ccl_indice_t>(jstart - 1);
	const compv_ccl_indice_t* RLCi = RLC->ptr<const compv_ccl_indice_t>(jstart);
	const compv_ccl_indice_t* ner0 = ner->ptr<const compv_ccl_indice_t>(0);
	compv_ccl_indice_t* ERAi = ERA->ptr<compv_ccl_indice_t>(jstart);
	const compv_ccl_indice_t wminus1 = (w - 1);
	const size_t ER_stride = ER->stride();
	const size_t RLC_stride = RLC->stride();
	const size_t ERA_stride = ERA->stride();
	
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No ASM implementation found: cmov");
	for (compv_ccl_indice_t j = jstart; j < end; ++j) {
		const compv_ccl_indice_t ner0j = ner0[j];
		for (compv_ccl_indice_t er = 1; er < ner0j; er += 2) {
			compv_ccl_indice_t j0 = RLCi[er - 1];
			compv_ccl_indice_t j1 = RLCi[er];
			// [check extension in case of 8-connect algorithm]
			j0 -= (j0 > 0);
			j1 += (j1 < wminus1);
			compv_ccl_indice_t er0 = ERiminus1[j0];
			compv_ccl_indice_t er1 = ERiminus1[j1];
			// [check label parity: segments are odd]
			er0 += ((er0 & 1) ^ 1);
			er1 -= ((er1 & 1) ^ 1);
			ERAi[er] = (er1 >= er0)
				? (er0 | er1 << 16)
				: 0;
		}
		ERi += ER_stride;
		ERiminus1 += ER_stride;
		RLCi += RLC_stride;
		ERAi += ERA_stride;
	}
}

// 2.2 Equivalence construction: step#2.1 (not MT friendly)
// Algorithm 14: LSL equivalence construction
// EQ,  the table holding the equivalence classes, before transitive closure
// ERAi, an associative table holding the association between er and ea: ea = ERAi[er]
// ner, the number of segments of ERi - black + white
// nea the current number of absolute labels, update of EQ and ERAi
static void step21_algo14_equivalence_build(const CompVMatPtr ner, CompVMatPtr ERA, CompVMatPtr EQ, compv_ccl_indice_t* nea1)
{
	const compv_ccl_indice_t ner00 = *ner->ptr<compv_ccl_indice_t>(0, 0);
	compv_ccl_indice_t* ERA0 = ERA->ptr<compv_ccl_indice_t>(0);
	compv_ccl_indice_t nea = 0;
	for (compv_ccl_indice_t er = 1; er < ner00; er += 2) {
		ERA0[er] = ++nea; // [new label]
	}
	
	const size_t rows = ERA->rows();
	compv_ccl_indice_t* ERAi = ERA->ptr<compv_ccl_indice_t>(1);
	const compv_ccl_indice_t* ERAiminus1 = ERA->ptr<compv_ccl_indice_t>(0);
	compv_ccl_indice_t* EQ0 = EQ->ptr<compv_ccl_indice_t>(0, 0);
	compv_ccl_indice_t* ner0 = ner->ptr<compv_ccl_indice_t>(0, 0);
	const size_t ERA_stride = ERA->stride();

	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No ASM implementation found");
	for (size_t j = 1; j < rows; ++j) {
		const compv_ccl_indice_t ner0j = ner0[j];
		for (compv_ccl_indice_t er = 1; er < ner0j; er += 2) {
			if (ERAi[er]) {
				const compv_ccl_indice_t er0 = ERAi[er] & 0xffff;
				const compv_ccl_indice_t er1 = (ERAi[er] >> 16) & 0xffff;
				compv_ccl_indice_t ea = ERAiminus1[er0];
				compv_ccl_indice_t a = EQ0[ea];
				for (compv_ccl_indice_t erk = er0 + 2; erk <= er1; erk += 2) {
					const compv_ccl_indice_t eak = ERAiminus1[erk];
					const compv_ccl_indice_t ak = EQ0[eak];
					// [min extraction and propagation]
					if (a < ak) {
						EQ0[eak] = a;
					}
					else {
						a = ak;
						EQ0[ea] = a;
						ea = eak;
					}
				}
				ERAi[er] = a; // [global min]
			}
			else {
				ERAi[er] = ++nea; // [new label]
			}
		}
		ERAiminus1 = ERAi;
		ERAi += ERA_stride;
	}

	*nea1 = nea;
}

// 2.4 Equivalence resolution: step#4 (MT friendly)
// EQ, the table holding the equivalence classes, before transitive closure
// A, the associative table of ancestors
// nea, the current number of absolute labels
// na, final number of absolute labels (background not counted)
static void step4_algo6_eq_resolv(const CompVMatPtr& EQ, const compv_ccl_indice_t nea, CompVMatPtr A, compv_ccl_indice_t& na)
{
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No ASM implementation found (cmov)");
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No unroll implementation found");
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No MT implementation found");
	const compv_ccl_indice_t* EQPtr = EQ->ptr<const compv_ccl_indice_t>();
	compv_ccl_indice_t* APtr = A->ptr<compv_ccl_indice_t>();
	na = 0;

	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("Could be nea only instead of size");
	for (compv_ccl_indice_t e = 1; e <= nea; ++e) {
		const compv_ccl_indice_t eq = EQPtr[e];
		COMPV_ASSERT(eq <= nea); // FIXME(dmi): remove
		APtr[e] = (eq != e)
			? APtr[eq]
			: ++na;
	}
}

static COMPV_ERROR_CODE build_EQ(const size_t ner_sum, const size_t width, const size_t height, CompVMatPtrPtr EQ)
{
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No MT implementation found");
	/* Create EQ and init with 0...n (itoa) */
	const size_t EQ_count = COMPV_MATH_MIN(static_cast<size_t>(ner_sum), (height * (width >> 2)));
	COMPV_CHECK_CODE_RETURN(CompVMat::newObjStrideless<compv_ccl_indice_t>(EQ, 1, EQ_count));
	compv_ccl_indice_t* EQPtr = (*EQ)->ptr<compv_ccl_indice_t>();
	const compv_ccl_indice_t n = static_cast<compv_ccl_indice_t>(EQ_count);
	for (compv_ccl_indice_t i = 0; i < n; ++i) {
		EQPtr[i] = i;
	}
	return COMPV_ERROR_CODE_S_OK;
}

static COMPV_ERROR_CODE build_all_labels(const CompVMatPtr& A, const CompVMatPtr& ERA, const CompVMatPtr& ER, CompVMatPtrPtr EA)
{
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No MT implementation found");
	const size_t ER_width = ER->cols();
	const size_t ER_height = ER->rows();
	const size_t ER_stride = ER->stride();
	const size_t ERA_stride = ERA->stride();

	// Create EA using same size and stride as ER
	COMPV_CHECK_CODE_RETURN(CompVMat::newObjAligned<compv_ccl_indice_t>(EA, ER_height, ER_width, ER_stride));

	const compv_ccl_indice_t* APtr = A->ptr<const compv_ccl_indice_t>();
	const compv_ccl_indice_t* ERAPtr = ERA->ptr<const compv_ccl_indice_t>();
	const compv_ccl_indice_t* ERPtr = ER->ptr<compv_ccl_indice_t>();
	compv_ccl_indice_t* EAPtr = (*EA)->ptr<compv_ccl_indice_t>();

	/* #3 and #5 merged */
	// step #3: First absolute labeling
	// step #5: Second absolute labeling
	for (size_t j = 0; j < ER_height; ++j) {
		for (size_t i = 0; i < ER_width; ++i) {
			COMPV_ASSERT(static_cast<compv_ccl_indice_t>(A->cols()) > ERAPtr[ERPtr[i]]); // FIXME(dmi): remove
			EAPtr[i] = APtr[ERAPtr[ERPtr[i]]];
		}
		EAPtr += ER_stride;
		ERPtr += ER_stride;
		ERAPtr += ERA_stride;
	}

	return COMPV_ERROR_CODE_S_OK;
}

// VERY important: binar must be binary image (allowed values: 0x01, 0xff, 0x00)
COMPV_ERROR_CODE CompVConnectedComponentLabelingLSL::process(const CompVMatPtr& binar, CompVConnectedComponentLabelingResult& result) /*Overrides(CompVConnectedComponentLabeling)*/
{
	COMPV_CHECK_EXP_RETURN(!binar || binar->isEmpty() || binar->planeCount() != 1 || binar->elmtInBytes() != sizeof(uint8_t)
		, COMPV_ERROR_CODE_E_INVALID_PARAMETER);

	/* Reset result */
	result.reset();
	result.label_background = kCompVConnectedComponentLabelingLSLBachgroundLabel;

	CompVMatPtr ner; // the number of segments of ERi - black + white -
	CompVMatPtr ER; //  an associative table of size w holding the relative labels er associated to Xi
	CompVMatPtr RLC; // a table holding the run length coding of segments of the line Xi, RLCi-1 is the similar memorization of the previous line.
	CompVMatPtr ERA; // an associative table holding the association between er and ea: ea = ERAi[er]
	CompVMatPtr EQ; // the table holding the equivalence classes, before transitive closure
	CompVMatPtr EA; // an image of size h � w of absolute labels ea before equivalence resolution
	CompVMatPtr A; // the associative table of ancestors

	const size_t width = binar->cols();
	const size_t height = binar->rows();
	const size_t stride = binar->stride();
	const size_t __ner_max = ((width + 1) >> 1); // full dashed row

	COMPV_CHECK_CODE_RETURN(CompVMat::newObjStrideless<compv_ccl_indice_t>(&ER, height, width));
	COMPV_CHECK_CODE_RETURN(CompVMat::newObjStrideless<compv_ccl_indice_t>(&RLC, height, __ner_max));
	COMPV_CHECK_CODE_RETURN(CompVMat::newObjStrideless<compv_ccl_indice_t>(&ner, 1, height));

	/* Multi-threading dispatcher */
	CompVThreadDispatcherPtr threadDisp = CompVParallel::threadDispatcher();
	const size_t maxThreads = threadDisp ? static_cast<size_t>(threadDisp->threadsCount()) : 1;

	/* Relative segment labeling: step#1 */
	compv_ccl_indice_t ner_max, ner_sum;
	const size_t threadsCountStep1 = (threadDisp && !threadDisp->isMotherOfTheCurrentThread())
		? CompVThreadDispatcher::guessNumThreadsDividingAcrossY(width, height, maxThreads, COMPV_CCL_LSL_STEP1_MIN_SAMPLES_PER_THREAD)
		: 1;
	auto funcPtrStep1 = [&](const compv_ccl_indice_t mt_start, const compv_ccl_indice_t mt_end, compv_ccl_indice_t* mt_ner_max, compv_ccl_indice_t* mt_ner_sum) -> COMPV_ERROR_CODE {
		step1_algo13_segment_RLE<uint8_t>(
			binar,
			ER,
			RLC,
			ner,
			mt_ner_max,
			mt_ner_sum,
			static_cast<compv_ccl_indice_t>(width),
			mt_start,
			mt_end
			);
		return COMPV_ERROR_CODE_S_OK;
	};
	if (threadsCountStep1 > 1) {
		CompVAsyncTaskIds taskIds;
		taskIds.reserve(threadsCountStep1);
		std::vector<compv_ccl_indice_t> mt_ner_max(threadsCountStep1), mt_ner_sum(threadsCountStep1);
		const size_t heights = (height / threadsCountStep1);
		size_t YStart = 0, YEnd;
		for (size_t threadIdx = 0; threadIdx < threadsCountStep1; ++threadIdx) {
			YEnd = (threadIdx == (threadsCountStep1 - 1)) ? height : (YStart + heights);
			COMPV_CHECK_CODE_RETURN(threadDisp->invoke(std::bind(funcPtrStep1, static_cast<compv_ccl_indice_t>(YStart), static_cast<compv_ccl_indice_t>(YEnd), &mt_ner_max[threadIdx], &mt_ner_sum[threadIdx]),
				taskIds), "Dispatching task failed");
			YStart += heights;
		}
		COMPV_CHECK_CODE_RETURN(threadDisp->waitOne(taskIds[0]));
		ner_max = mt_ner_max[0];
		ner_sum = mt_ner_sum[0];
		for (size_t threadIdx = 1; threadIdx < threadsCountStep1; ++threadIdx) {
			COMPV_CHECK_CODE_RETURN(threadDisp->waitOne(taskIds[threadIdx]));
			if (ner_max < mt_ner_max[threadIdx]) {
				ner_max = mt_ner_max[threadIdx];
			}
			ner_sum += mt_ner_sum[threadIdx];
		}
	}
	else {
		COMPV_CHECK_CODE_RETURN(funcPtrStep1(0, static_cast<compv_ccl_indice_t>(height), &ner_max, &ner_sum));
	}
	
	/* Create ERA and init with zeros (FIXME(use calloc) */
	{
		COMPV_CHECK_CODE_RETURN(CompVMat::newObjStrideless<compv_ccl_indice_t>(&ERA, height, ner_max));
		COMPV_CHECK_CODE_RETURN(ERA->zero_all());
	}

	/* Equivalence construction: step#2.0 (MT-friendly) */
	const size_t minSamplePerThreadStep20 = std::max(
		static_cast<size_t>(COMPV_CCL_LSL_STEP20_MIN_SAMPLES_PER_THREAD),
		(width << 1) // At least #2 rows (because of ERiminus1)
	);
	auto funcPtrStep20 = [&](const size_t mt_start, const size_t mt_end) -> COMPV_ERROR_CODE {
		step20_algo14_equivalence_build(
			ER,
			RLC,
			ner,
			ERA,
			static_cast<compv_ccl_indice_t>(width),
			static_cast<compv_ccl_indice_t>(mt_start),
			static_cast<compv_ccl_indice_t>(mt_end)
		);
		if (!mt_start) {
			/* Build EQ */
			COMPV_CHECK_CODE_RETURN(build_EQ(ner_sum, width, height, &EQ));
		}
		return COMPV_ERROR_CODE_S_OK;
	};
	COMPV_CHECK_CODE_RETURN(CompVThreadDispatcher::dispatchDividingAcrossY(
		funcPtrStep20,
		width,
		height,
		minSamplePerThreadStep20
	));
	

	/* Equivalence construction: step#2.1 (not MT-friendly) */
	compv_ccl_indice_t nea; // the current number of absolute labels, update of EQ and ERAi
	step21_algo14_equivalence_build(
		ner,
		ERA,
		EQ,
		&nea
	);
	COMPV_ASSERT(nea < ner_sum); // FIXME(dmi): remove

	/* Create A and init first element with zero (because bacground label is equal to zero) */
	COMPV_CHECK_CODE_RETURN(CompVMat::newObjStrideless<compv_ccl_indice_t>(&A, 1, (nea + 1)));
	*A->ptr<compv_ccl_indice_t>(0, 0) = 0; // other values will be initialzed in step4_algo6_eq_resolv

	/* Equivalence resolution: step#4 */
	compv_ccl_indice_t na = 0; // final number of absolute labels
	step4_algo6_eq_resolv(
		EQ,
		nea,
		A,
		na
	);

	/* For testing */
	build_all_labels(A, ERA, ER, &EA);

	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("Directly write to result.labels");
	result.labels = EA;
	result.labels_count = (na + 1); // +1 for the background

	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVConnectedComponentLabelingLSL::newObj(CompVConnectedComponentLabelingPtrPtr ccl)
{
	COMPV_CHECK_CODE_RETURN(CompVCore::init());
	COMPV_CHECK_EXP_RETURN(!ccl, COMPV_ERROR_CODE_E_INVALID_PARAMETER);
	CompVConnectedComponentLabelingPtr _ccl = new CompVConnectedComponentLabelingLSL();
	COMPV_CHECK_EXP_RETURN(!_ccl, COMPV_ERROR_CODE_E_OUT_OF_MEMORY);
	*ccl = *_ccl;
	return COMPV_ERROR_CODE_S_OK;
}

COMPV_NAMESPACE_END()