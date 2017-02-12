#include "../tests_common.h"

#define TAG_TEST			"TestInverse"
#define LOOP_COUNT			1
#define TYP					compv_float64_t

COMPV_ERROR_CODE pseudoinv()
{
	CompVMatPtr A, Ai;
	static const size_t rows = 3;
	static const size_t cols = 3;

	static const struct compv_unittest_psi {
		size_t rows;
		size_t cols;
		const char* md5;
		const char* md5_fma;
	}
	COMPV_UNITTEST_PSI_FLOAT64[] = {
#if COMPV_ARCH_X64
		{ 11, 7, "495d6a52a87dd8eeab968cc8ff11d7bc" }, // non-square
		{ 9, 9, "da1389e9ab32e15b8372f46588d7fb2d" },
		{ 3, 3, "42254759b1104b0ab77822b727a4f91e" },
#elif COMPV_ARCH_X86
		{ 11, 7, "d709a8fd1eff33716fe54ea3088b72a6" }, // non-square
		{ 9, 9, "ccbf7501f070bc02ec869477ea344e2d" },
		{ 3, 3, "42254759b1104b0ab77822b727a4f91e" },
#elif COMPV_ARCH_ARM
		{ 11, 7, "53c0f26c55b4a72772ededcb86980eae" }, // non-square
		{ 9, 9, "31dcd8f422f47e90d90ed859fa2e9a98" },
		{ 3, 3, "42254759b1104b0ab77822b727a4f91e" },
#else
		{ 11, 7, "" }, // non-square
		{ 9, 9, "" },
		{ 3, 3, "" },
#endif
	},
	COMPV_UNITTEST_PSI_FLOAT32[] = {
#if COMPV_ARCH_X64
		{ 11, 7, "590701fe56119e3b451fb74a1e37cfe3" }, // non-square
		{ 9, 9, "06a03cec995c15d983aa16eb60733cdd" },
		{ 3, 3, "adcd58633cdb45f6f84d5a24e7488d3a" },
#elif COMPV_ARCH_X86
		{ 11, 7, "590701fe56119e3b451fb74a1e37cfe3" }, // non-square
		{ 9, 9, "06a03cec995c15d983aa16eb60733cdd" },
		{ 3, 3, "adcd58633cdb45f6f84d5a24e7488d3a" },
#elif COMPV_ARCH_ARM
		{ 11, 7, "590701fe56119e3b451fb74a1e37cfe3" }, // non-square
		{ 9, 9, "06a03cec995c15d983aa16eb60733cdd" },
		{ 3, 3, "adcd58633cdb45f6f84d5a24e7488d3a" },
#else
		{ 11, 7, "" }, // non-square
		{ 9, 9, "" },
		{ 3, 3, "" },
#endif
	};

	const compv_unittest_psi* test = NULL;
	const compv_unittest_psi* tests = std::is_same<TYP, compv_float32_t>::value
		? COMPV_UNITTEST_PSI_FLOAT32
		: COMPV_UNITTEST_PSI_FLOAT64;

	for (size_t i = 0; i < sizeof(COMPV_UNITTEST_PSI_FLOAT64) / sizeof(COMPV_UNITTEST_PSI_FLOAT64[i]); ++i) {
		if (tests[i].rows == rows && tests[i].cols == cols) {
			test = &tests[i];
			break;
		}
	}

	COMPV_CHECK_EXP_RETURN(!test, COMPV_ERROR_CODE_E_UNITTEST_FAILED, "Failed to find test");
	
	COMPV_CHECK_CODE_RETURN(CompVMat::newObjAligned<TYP>(&A, test->rows, test->cols));

	// Build random data (must be deterministic to have same MD5)
	TYP* row;
	for (signed j = 0; j < static_cast<signed>(A->rows()); ++j) {
		row = A->ptr<TYP>(j);
		for (signed i = 0; i < static_cast<signed>(A->cols()); ++i) {
			row[i] = static_cast<TYP>(((i & 1) ? i : -i) + (j * 0.5) + j + 0.7);
		}
	}

	uint64_t timeStart = CompVTime::nowMillis();
	for (size_t i = 0; i < LOOP_COUNT; ++i) {
		COMPV_CHECK_CODE_RETURN(CompVMatrix::pseudoinv(A, &Ai));
	}
	uint64_t timeEnd = CompVTime::nowMillis();

	//COMPV_DEBUG_INFO("MD5: %s", compv_tests_md5(Ai).c_str());

	COMPV_DEBUG_INFO_EX(TAG_TEST, "Elapsed time(pseudoinv) = [[[ %" PRIu64 " millis ]]]", (timeEnd - timeStart));

	COMPV_CHECK_EXP_RETURN(std::string(test->md5).compare(compv_tests_md5(Ai)) != 0, COMPV_ERROR_CODE_E_UNITTEST_FAILED, "pseudoinv: MD5 mismatch");

	return COMPV_ERROR_CODE_S_OK;
}