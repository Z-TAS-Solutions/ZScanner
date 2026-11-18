#ifndef ZCORE_H
#define ZCORE_H

#pragma once
#define NOMINMAX
#include <opencv2/opencv.hpp>
#include <opencv2/ximgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

inline cv::Mat FrangiFilter(
    const cv::Mat& roi, float sigma_start = 1.0f, float sigma_end = 3.0f, float sigma_step = 1.0f
)
{
    cv::Mat src;
    roi.convertTo(src, CV_32FC1, 1.0 / 255.0);

    const float beta = 0.8f;
    const float c    = 0.08f;

    cv::Mat max_vesselness = cv::Mat::zeros(src.size(), CV_32FC1);

    for (float sigma = sigma_start; sigma <= sigma_end; sigma += sigma_step) {
        cv::Mat blurred;
        int     ksize = (int)(4 * sigma + 1);
        if (ksize % 2 == 0)
            ksize++;
        cv::GaussianBlur(src, blurred, cv::Size(ksize, ksize), sigma);

        cv::Mat Dxx, Dyy, Dxy;
        cv::Sobel(blurred, Dxx, CV_32F, 2, 0, 3);
        cv::Sobel(blurred, Dyy, CV_32F, 0, 2, 3);
        cv::Sobel(blurred, Dxy, CV_32F, 1, 1, 3);

        Dxx *= (sigma * sigma);
        Dyy *= (sigma * sigma);
        Dxy *= (sigma * sigma);

        cv::Mat vesselness = cv::Mat::zeros(src.size(), CV_32FC1);
        for (int y = 0; y < src.rows; y++) {
            for (int x = 0; x < src.cols; x++) {
                float fxx = Dxx.at<float>(y, x);
                float fyy = Dyy.at<float>(y, x);
                float fxy = Dxy.at<float>(y, x);

                float tmp = std::sqrt((fxx - fyy) * (fxx - fyy) + 4 * fxy * fxy);
                float l1  = (fxx + fyy - tmp) / 2.0f;
                float l2  = (fxx + fyy + tmp) / 2.0f;

                if (l2 <= 0)
                    continue;

                float Rb = std::abs(l1) / std::abs(l2);
                float S  = std::sqrt(l1 * l1 + l2 * l2);

                float v = std::exp(-(Rb * Rb) / (2 * beta * beta)) *
                          (1.0f - std::exp(-(S * S) / (2 * c * c)));

                vesselness.at<float>(y, x) = v;
            }
        }
        cv::max(max_vesselness, vesselness, max_vesselness);
    }

    cv::normalize(max_vesselness, max_vesselness, 0, 255, cv::NORM_MINMAX, CV_8U);
    return max_vesselness;
}

class ZCore
{
  public:
    static cv::Mat PreProcess(const cv::Mat& roi, int targetSize = 512)
    {
        cv::Mat GrayFrame, Ouput;

        GrayFrame = roi.clone();

        const int Padding = 10;
        cv::Mat   Padded;
        cv::copyMakeBorder(
            GrayFrame, Padded, Padding, Padding, Padding, Padding, cv::BORDER_REFLECT
        );

        auto clahe = cv::createCLAHE(1.1, cv::Size(16, 16));
        clahe->apply(Padded, Padded);

        cv::medianBlur(Padded, Padded, 7);

        Ouput = FrangiFilter(Padded, 3.5, 3.5 + (7 * 0.5), 0.5);

        Ouput = CropBorder(Ouput, Padding);

        cv::normalize(Ouput, Ouput, 0, 255, cv::NORM_MINMAX, CV_8U);

        return Ouput;
    }

    static std::vector<cv::Point2f>
    ValleyExRadial(const cv::Mat& frame, float smoothSigma = 5.0f, int minNeighbor = 15);

    static std::vector<cv::Point2f>
    ROIGen(cv::Point2f p1, cv::Point2f p2, int handedness = 1, float scaleFactor = 2.8f);

    inline static void PointVisualizerEx(const std::vector<cv::Point2f>& Points, cv::Mat& DestFrame)
    {
        for (size_t i = 0; i < Points.size(); i++) {
            cv::circle(DestFrame, Points[i], 5, cv::Scalar(0, 255, 0), -1);

            cv::putText(
                DestFrame,
                std::to_string(i),
                Points[i] + cv::Point2f(5, -5),
                cv::FONT_HERSHEY_SIMPLEX,
                0.5,
                cv::Scalar(255, 255, 255),
                1
            );
        }
    }

    inline static void DrawROI(const std::vector<cv::Point2f>& Points, cv::Mat& DestFrame)
    {
        std::vector<std::vector<cv::Point>> intPoints(1);
        for (const auto& p : Points) {
            intPoints[0].push_back(cv::Point(p.x, p.y));
        }

        cv::polylines(DestFrame, intPoints, true, cv::Scalar(255), 2);
    }

    static inline cv::Mat ExtractPalmROI(
        const cv::Mat& src, const std::vector<cv::Point2f>& roiPoints, int targetSize = 512
    )
    {
        if (roiPoints.size() != 4)
            return cv::Mat();

        std::vector<cv::Point2f> dstPoints = {
            cv::Point2f(0, 0),
            cv::Point2f(targetSize, 0),
            cv::Point2f(targetSize, targetSize),
            cv::Point2f(0, targetSize)
        };

        cv::Mat transMat = cv::getPerspectiveTransform(roiPoints, dstPoints);

        cv::Mat roi;
        cv::warpPerspective(src, roi, transMat, cv::Size(targetSize, targetSize), cv::INTER_CUBIC);

        return roi;
    }

    static cv::Mat CropBorder(const cv::Mat& src, int borderSize)
    {
        if (src.empty())
            return src;

        int safeBorder = std::min({borderSize, src.cols / 2 - 1, src.rows / 2 - 1});
        if (safeBorder <= 0)
            return src;

        cv::Rect innerROI(
            safeBorder, safeBorder, src.cols - (2 * safeBorder), src.rows - (2 * safeBorder)
        );

        return src(innerROI).clone();
    }

    static void
    CalculateHessian(const cv::Mat& src, cv::Mat& Lxx, cv::Mat& Lxy, cv::Mat& Lyy, float sigma);

    static cv::Mat PreProcessTest(const cv::Mat& roi, cv::Mat& Lxx, cv::Mat& Lxy, cv::Mat& Lyy);

    static void GenerateTemplate(
        const cv::Mat& vesselness,
        const cv::Mat& Lxx,
        const cv::Mat& Lxy,
        const cv::Mat& Lyy,
        cv::Mat&       outTemplate,
        cv::Mat&       outMask
    );

    static float Match(
        const cv::Mat& T1,
        const cv::Mat& M1,
        const cv::Mat& T2,
        const cv::Mat& M2,
        int            searchRange = 5
    );

    static std::vector<uint8_t> PackTemplate(const cv::Mat& orient, const cv::Mat& mask);

    static void UnpackTemplate(
        const std::vector<uint8_t>& buffer,
        cv::Mat&                    orient,
        cv::Mat&                    mask,
        int                         width  = 128,
        int                         height = 128
    );
};

#endif
