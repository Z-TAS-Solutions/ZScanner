#include "ZCore.h"

std::vector<cv::Point2f>
ZCore::ValleyExRadial(const cv::Mat& frame, float smoothSigma, int minNeighbor)
{
    cv::Mat gray, mask, dist;
    if (frame.channels() == 3)
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    else
        gray = frame;

    cv::GaussianBlur(gray, mask, cv::Size(7, 7), 0);
    cv::threshold(mask, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::distanceTransform(mask, dist, cv::DIST_L2, 5);
    double    maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(dist, nullptr, &maxVal, nullptr, &maxLoc);
    cv::Point2f rf = maxLoc;

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
    if (contours.empty())
        return {};
    auto handContour =
        *std::max_element(contours.begin(), contours.end(), [](const auto& a, const auto& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        });

    float dynamicSigma    = (float)maxVal * 0.12f;
    int   dynamicNeighbor = (int)(maxVal * 0.25f);

    std::vector<float> radialDistances;
    for (const auto& pt : handContour) {
        radialDistances.push_back(cv::norm(rf - (cv::Point2f)pt));
    }

    std::vector<float> smoothed;
    cv::Mat            distMat(radialDistances, false);
    cv::GaussianBlur(distMat, distMat, cv::Size(1, 0), dynamicSigma);
    distMat.copyTo(smoothed);

    std::vector<cv::Point2f> valleys;
    int                      n = smoothed.size();
    for (int i = 0; i < n; ++i) {
        bool isLocalMin = true;
        for (int j = -dynamicNeighbor; j <= dynamicNeighbor; ++j) {
            if (smoothed[(i + j + n) % n] < smoothed[i]) {
                isLocalMin = false;
                break;
            }
        }

        if (isLocalMin) {
            cv::Point2f pt = handContour[i];

            if (pt.y > rf.y + (maxVal * 0.6f))
                continue;

            if (pt.x <= 10 || pt.x >= frame.cols - 11 || pt.y <= 10)
                continue;

            if (smoothed[i] < (maxVal * 0.9f))
                continue;

            valleys.push_back(pt);
        }
    }

    std::sort(valleys.begin(), valleys.end(), [&](const cv::Point2f& a, const cv::Point2f& b) {
        float angA = atan2(a.y - rf.y, a.x - rf.x);
        float angB = atan2(b.y - rf.y, b.x - rf.x);

        return angA < angB;
    });

    return valleys;
}

std::vector<cv::Point2f>
ZCore::ROIGen(cv::Point2f p1, cv::Point2f p2, int handedness, float sizeFactor)
{
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float D  = std::sqrt(dx * dx + dy * dy);

    cv::Point2f mid = (p1 + p2) * 0.5f;
    cv::Point2f unitDown(-dy / D, dx / D);
    cv::Point2f unitRight(dx / D, dy / D);

    float       offsetDist = D * 2.2f;
    cv::Point2f roiCenter  = mid + (unitDown * offsetDist);

    float thumbShiftFactor = 0.3f;
    roiCenter              = roiCenter + (unitRight * (D * thumbShiftFactor * handedness));

    float h = (D * sizeFactor) / 2.0f;

    std::vector<cv::Point2f> corners(4);
    corners[0] = roiCenter - (unitRight * h) - (unitDown * h);
    corners[1] = roiCenter + (unitRight * h) - (unitDown * h);
    corners[2] = roiCenter + (unitRight * h) + (unitDown * h);
    corners[3] = roiCenter - (unitRight * h) + (unitDown * h);

    return corners;
}

float ZCore::Match(
    const cv::Mat& T1, const cv::Mat& M1, const cv::Mat& T2, const cv::Mat& M2, int searchRange
)
{
    if (T1.empty() || M1.empty() || T2.empty() || M2.empty()) {
        return 1.0f;
    }
    if (T1.size() != M1.size() || T2.size() != M2.size() || T1.size() != T2.size()) {
        return 1.0f;
    }

    int m1Count = cv::countNonZero(M1);
    int m2Count = cv::countNonZero(M2);
    // Real vein templates have several thousand mask pixels.
    // Require a minimum overlap of at least 25% of the smaller mask, with a floor of 500 pixels.
    int minRequiredOverlap = std::max(500, static_cast<int>(std::min(m1Count, m2Count) * 0.25f));

    float bestScore = 1.0f;

    for (int dy = -searchRange; dy <= searchRange; dy++) {
        for (int dx = -searchRange; dx <= searchRange; dx++) {

            float total = 0, diff = 0;

            for (int i = searchRange; i < T1.rows - searchRange; i++) {
                for (int j = searchRange; j < T1.cols - searchRange; j++) {

                    if (M1.at<uchar>(i, j) > 0 && M2.at<uchar>(i + dy, j + dx) > 0) {
                        total += 1.0f;

                        int v1 = T1.at<uchar>(i, j);
                        int v2 = T2.at<uchar>(i + dy, j + dx);

                        if (v1 != v2) {
                            int d = std::abs(v1 - v2);
                            if (d == 3)
                                d = 1;

                            if (d == 1) {
                                diff += 0.5f;
                            } else {
                                diff += 1.0f;
                            }
                        }
                    }
                }
            }

            if (total >= minRequiredOverlap) {
                float currentScore = diff / total;
                if (currentScore < bestScore)
                    bestScore = currentScore;
            }
        }
    }
    return bestScore;
}

void ZCore::CalculateHessian(
    const cv::Mat& src, cv::Mat& Lxx, cv::Mat& Lxy, cv::Mat& Lyy, float sigma
)
{
    int ksize = (int)(4 * sigma + 1);
    if (ksize % 2 == 0)
        ksize++;

    cv::Mat blurred;
    cv::GaussianBlur(src, blurred, cv::Size(ksize, ksize), sigma);

    cv::Sobel(blurred, Lxx, CV_32F, 2, 0, 3);
    cv::Sobel(blurred, Lyy, CV_32F, 0, 2, 3);
    cv::Sobel(blurred, Lxy, CV_32F, 1, 1, 3);
}

cv::Mat ZCore::PreProcessTest(const cv::Mat& roi, cv::Mat& Lxx, cv::Mat& Lxy, cv::Mat& Lyy)
{
    cv::Mat Resized, Output;

    cv::resize(roi, Resized, cv::Size(128, 128), 0, 0, cv::INTER_AREA);

    const int Padding = 10;
    cv::Mat   Padded;
    cv::copyMakeBorder(Resized, Padded, Padding, Padding, Padding, Padding, cv::BORDER_REFLECT);

    auto clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(Padded, Padded);

    cv::medianBlur(Padded, Padded, 3);

    float sigma = 1.5f;
    CalculateHessian(Padded, Lxx, Lxy, Lyy, sigma);

    Output = FrangiFilter(Padded, 1.0, 2.5, 0.5);

    Output = ZCore::CropBorder(Output, Padding);
    Lxx    = ZCore::CropBorder(Lxx, Padding);
    Lxy    = ZCore::CropBorder(Lxy, Padding);
    Lyy    = ZCore::CropBorder(Lyy, Padding);

    cv::normalize(Output, Output, 0, 255, cv::NORM_MINMAX, CV_8U);

    return Output;
}

void ZCore::GenerateTemplate(
    const cv::Mat& vesselness,
    const cv::Mat& Lxx,
    const cv::Mat& Lxy,
    const cv::Mat& Lyy,
    cv::Mat&       outTemplate,
    cv::Mat&       outMask
)
{
    outTemplate = cv::Mat::zeros(vesselness.size(), CV_8U);
    cv::threshold(vesselness, outMask, 50, 255, cv::THRESH_BINARY);

    for (int r = 0; r < vesselness.rows; r++) {
        for (int c = 0; c < vesselness.cols; c++) {
            if (outMask.at<uchar>(r, c) == 0)
                continue;

            float angle =
                0.5f *
                std::atan2(2.0f * Lxy.at<float>(r, c), Lxx.at<float>(r, c) - Lyy.at<float>(r, c));

            float deg = angle * 180.0f / CV_PI;
            if (deg < 0)
                deg += 180.0f;

            if (deg < 22.5 || deg >= 157.5)
                outTemplate.at<uchar>(r, c) = 0;
            else if (deg < 67.5)
                outTemplate.at<uchar>(r, c) = 1;
            else if (deg < 112.5)
                outTemplate.at<uchar>(r, c) = 2;
            else
                outTemplate.at<uchar>(r, c) = 3;
        }
    }
}

std::vector<uint8_t> ZCore::PackTemplate(const cv::Mat& orient, const cv::Mat& mask)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(4096 + 2048);

    uint8_t currentByte = 0;
    int     bitCount    = 0;
    for (int i = 0; i < orient.total(); ++i) {
        currentByte = (currentByte << 2) | (orient.data[i] & 0x03);
        bitCount += 2;
        if (bitCount == 8) {
            buffer.push_back(currentByte);
            currentByte = 0;
            bitCount    = 0;
        }
    }

    currentByte = 0;
    bitCount    = 0;
    for (int i = 0; i < mask.total(); ++i) {
        currentByte = (currentByte << 1) | (mask.data[i] > 0 ? 1 : 0);
        bitCount++;
        if (bitCount == 8) {
            buffer.push_back(currentByte);
            currentByte = 0;
            bitCount    = 0;
        }
    }
    return buffer;
}

void ZCore::UnpackTemplate(
    const std::vector<uint8_t>& buffer, cv::Mat& orient, cv::Mat& mask, int width, int height
)
{
    int totalPixels = width * height;
    orient          = cv::Mat::zeros(height, width, CV_8U);
    mask            = cv::Mat::zeros(height, width, CV_8U);

    int bufferIdx = 0;

    int     bitsProcessed = 0;
    uint8_t currentByte   = 0;
    for (int i = 0; i < totalPixels; ++i) {
        if (bitsProcessed == 0) {
            currentByte   = buffer[bufferIdx++];
            bitsProcessed = 8;
        }
        orient.data[i] = (currentByte >> (bitsProcessed - 2)) & 0x03;
        bitsProcessed -= 2;
    }

    bitsProcessed = 0;
    for (int i = 0; i < totalPixels; ++i) {
        if (bitsProcessed == 0) {
            currentByte   = buffer[bufferIdx++];
            bitsProcessed = 8;
        }
        mask.data[i] = ((currentByte >> (bitsProcessed - 1)) & 0x01) ? 255 : 0;
        bitsProcessed -= 1;
    }
}
