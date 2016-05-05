#include <compv/compv_api.h>

#include "../common.h"

using namespace compv;

#define FAST_THRESHOLD				10
#define FAST_NONMAXIMA				true
#define ORB_MAX_FEATURES			-1
#define ORB_PYRAMID_LEVELS			8
#define ORB_PYRAMID_SCALEFACTOR		0.83f
#define ORB_PYRAMID_SCALE_TYPE		COMPV_SCALE_TYPE_BILINEAR
#define ORB_LOOOP_COUNT				1
#define ORB_DESC_MD5_FLOAT			"a9aa1a90cd404639274cd03727cca28c"
#define ORB_DESC_MD5_FLOAT_MT		"b546372e48fba28c9265f6eadbafefea" // multithreaded (convolution create temporary memory)
#define ORB_DESC_MD5_FXP			"a9aa1a90cd404639274cd03727cca28c" // "f1ba2ade7e357f14c5357a3587465b48"
#define JPEG_IMG					"C:/Projects/GitHub/pan360/tests/sphere_mapping/7019363969_a80a5d6acc_o.jpg" // voiture (2000*1000 = 2times more bytes than 720p)

bool TestORB()
{
    CompVPtr<CompVFeatureDete* > dete; // feature detector
    CompVPtr<CompVFeatureDesc* > desc; // feature descriptor
    CompVPtr<CompVImage *> image;
    CompVPtr<CompVBoxInterestPoint* > interestPoints;
    CompVPtr<CompVFeatureDescriptions*> descriptions;
    int32_t val32;
    bool valBool;
    float valFloat;
    uint64_t timeStart, timeEnd;

    // Decode the jpeg image
    COMPV_CHECK_CODE_ASSERT(CompVImageDecoder::decodeFile(JPEG_IMG, &image));
    // Convert the image to grayscal (required by feture detectors)
    COMPV_CHECK_CODE_ASSERT(image->convert(COMPV_PIXEL_FORMAT_GRAYSCALE, &image));

    // Create the ORB feature detector
    COMPV_CHECK_CODE_ASSERT(CompVFeatureDete::newObj(COMPV_ORB_ID, &dete));
    // Create the ORB feature descriptor
    COMPV_CHECK_CODE_ASSERT(CompVFeatureDesc::newObj(COMPV_ORB_ID, &desc));
    COMPV_CHECK_CODE_ASSERT(desc->attachDete(dete)); // attach detector to make sure we'll share context

    // Set the default values for the detector
    val32 = FAST_THRESHOLD;
    COMPV_CHECK_CODE_ASSERT(dete->set(COMPV_ORB_SET_INT32_FAST_THRESHOLD, &val32, sizeof(val32)));
    valBool = FAST_NONMAXIMA;
    COMPV_CHECK_CODE_ASSERT(dete->set(COMPV_ORB_SET_BOOL_FAST_NON_MAXIMA_SUPP, &valBool, sizeof(valBool)));
    val32 = ORB_PYRAMID_LEVELS;
    COMPV_CHECK_CODE_ASSERT(dete->set(COMPV_ORB_SET_INT32_PYRAMID_LEVELS, &val32, sizeof(val32)));
    val32 = ORB_PYRAMID_SCALE_TYPE;
    COMPV_CHECK_CODE_ASSERT(dete->set(COMPV_ORB_SET_INT32_PYRAMID_SCALE_TYPE, &val32, sizeof(val32)));
    valFloat = ORB_PYRAMID_SCALEFACTOR;
    COMPV_CHECK_CODE_ASSERT(dete->set(COMPV_ORB_SET_FLOAT_PYRAMID_SCALE_FACTOR, &valFloat, sizeof(valFloat)));
	val32 = ORB_MAX_FEATURES;
	COMPV_CHECK_CODE_ASSERT(dete->set(COMPV_ORB_SET_INT32_MAX_FEATURES, &val32, sizeof(val32)));

    timeStart = CompVTime::getNowMills();
    for (int i = 0; i < ORB_LOOOP_COUNT; ++i) {
        // Detect keypoints
        COMPV_CHECK_CODE_ASSERT(dete->process(image, interestPoints));

        // Describe keypoints
        COMPV_CHECK_CODE_ASSERT(desc->process(image, interestPoints, &descriptions));
    }
    timeEnd = CompVTime::getNowMills();
    COMPV_DEBUG_INFO("Elapsed time = [[[ %llu millis ]]]", (timeEnd - timeStart));

    // Compute Descriptions MD5
    const std::string md5 = descriptions ? CompVMd5::compute2(descriptions->getDataPtr(), descriptions->getDataSize()) : "";
	if (md5 != (CompVEngine::isMultiThreadingEnabled() ? ORB_DESC_MD5_FLOAT_MT : ORB_DESC_MD5_FLOAT)) {
        COMPV_DEBUG_ERROR("MD5 mismatch");
        COMPV_ASSERT(false);
        return false;
    }

    return true;
}