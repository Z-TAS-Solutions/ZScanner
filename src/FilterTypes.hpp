#ifndef FILTER_TYPES_H
#define FILTER_TYPES_H

#include <opencv2/opencv.hpp>
#include <variant>

enum class ThresholdType { Global, Otsu, AdaptiveMean, AdaptiveGaussian };

enum SharpenTypes { SharpenKernel, SharpenUnsharp, SharpenLaplacian, Frangi };

enum FilterTypes {
    CLAHE,
    MedianBlur,
    BilateralBlur,
    GaussianBlur,
    Threshold,
    Morphology,
    Skeletonize,
    Sharpen,
    Canny,
    Sobel,
    BrightnessContrast,
    Invert,
    Gamma,
    HistEqGlobal,
    BoxBlur,
};

struct FilterCLAHE
{
    float    claheClipLimit = 5;
    cv::Size GridLimit      = {16, 16};
};

struct FilterMedianBlur
{
    int medianK = 5;
};

struct FilterBilateralBlur
{
    int   bilateralD = 9;
    float sigmaColor = 75.0f;
    float sigmaSpace = 75.0f;
};

struct FilterGaussianBlur
{
    int   gaussK = 5;
    float sigmaX = 1.5f;
    float sigmaY = 0.0f;
};

struct FilterThreshold
{
    ThresholdType ThresholdType     = ThresholdType::AdaptiveGaussian;
    float         GlobalThreshold   = 127.0f;
    float         MaxBinaryValue    = 255.0f;
    int           AdaptiveBlockSize = 11;
    float         AdaptiveC         = 2.0f;
};

struct FilterMorphology
{
    cv::MorphShapes MorphShape      = cv::MorphShapes::MORPH_RECT;
    int             MorphKernelSize = 3;
    int             MorphIterations = 1;
    cv::MorphTypes  MorphType       = cv::MORPH_OPEN;
};

struct FilterSkeletonize
{
    int PruningIterations = 0;
};

struct FilterSharpen
{
    SharpenTypes SharpenType       = SharpenTypes::SharpenKernel;
    float        KernelStrength    = 1.0f;
    float        UnsharpSigma      = 1.0f;
    float        UnsharpAmount     = 1.5f;
    int          UnsharpThreshold  = 0;
    int          LaplacianKSize    = 3;
    float        LaplacianScale    = 1.0f;
    float        LaplacianSubAlpha = 0.5f;
    int          RidgeKSize        = 3;
    float        RidgeScale        = 1.0f;
    float        RidgeAlpha        = 0.5f;
    float        RidgeBeta         = 0.5f;
};

struct FilterCanny
{
    float threshold1   = 100.0f;
    float threshold2   = 200.0f;
    int   apertureSize = 3;
    bool  L2gradient   = false;
};

struct FilterSobel
{
    int   dx    = 1;
    int   dy    = 0;
    int   ksize = 3;
    float scale = 1.0f;
    float delta = 0.0f;
};

struct FilterBrightnessContrast
{
    float alpha = 1.0f; // Scale
    float beta  = 0.0f; // Shift
};

struct FilterInvert
{
    // Zero parameters
};

struct FilterGamma
{
    float gamma = 1.0f;
};

struct FilterHistEq
{
    // Zero parameters
};

struct FilterBoxBlur
{
    int ksize = 3;
};

struct FilterDistanceTransform
{
    int maskSize = 3;
};

using FilterConfigVariant = std::variant<
    std::monostate,
    FilterCLAHE,
    FilterMedianBlur,
    FilterBilateralBlur,
    FilterGaussianBlur,
    FilterThreshold,
    FilterMorphology,
    FilterSkeletonize,
    FilterSharpen,
    FilterCanny,
    FilterSobel,
    FilterBrightnessContrast,
    FilterInvert,
    FilterGamma,
    FilterHistEq,
    FilterBoxBlur,
    FilterDistanceTransform>;

struct FilterNode
{
    FilterTypes         Type;
    FilterConfigVariant Config;
};

#endif // !FILTER_TYPES_H
