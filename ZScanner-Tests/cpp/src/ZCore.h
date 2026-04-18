#ifndef ZCORE_H
#define ZCORE_H

#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/ximgproc.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>

class ZCore {
public:
    void AwaitAlignment();

    inline static void PointVisualizer(const std::vector<cv::Point2f>& Points, cv::Mat& DestFrame)
    {
        for (const auto& Point : Points) {
            cv::circle(DestFrame, Point, 5, cv::Scalar(0, 255, 0), -1);
        }
    }

    inline static void PointVisualizerEx(const std::vector<cv::Point2f>& Points, cv::Mat& DestFrame)
    {
        for (size_t i = 0; i < Points.size(); i++) {
            cv::circle(DestFrame, Points[i], 5, cv::Scalar(0, 255, 0), -1);

            cv::putText(DestFrame,
                std::to_string(i),
                Points[i] + cv::Point2f(5, -5),
                cv::FONT_HERSHEY_SIMPLEX,
                0.5,
                cv::Scalar(255, 255, 255),
                1);
        }
    }

    inline static void DrawROI(std::vector<cv::Point2f>& Points, cv::Mat& DestFrame)
    {
        std::vector<std::vector<cv::Point>> intPoints(1);
        for (const auto& p : Points) {
            intPoints[0].push_back(cv::Point(p.x, p.y));
        }

        cv::polylines(DestFrame, intPoints, true, cv::Scalar(255), 2);
    }
};


void ApplyLbp(const cv::Mat& src, cv::Mat& dst);

std::vector<cv::Mat> InitializeGaborBank();

void ExtractCompCode(const cv::Mat& Src, const std::vector<cv::Mat>& GaborBank, cv::Mat& Dest);

void VisualizeCompCode(const cv::Mat& Src, const cv::Mat& Dest);

void ExtractVeinSkeleton(const cv::Mat& Src, cv::Mat& Dest);

void ExtractFrangiVeins(const cv::Mat& Src, cv::Mat& Dest, float Sigma = 2.0f);

void XimgprocSkeletonize(const cv::Mat& Src, cv::Mat& Dest);

cv::Mat ExtractDistanceTransformRoi(const cv::Mat& frame, cv::Point& centerPoint, int& dynamicRoiSize)  ;

cv::Mat AnnotateMomentsRoi(const cv::Mat& frame, int roiSize, cv::Point& centerPoint);

cv::Mat AnnotateConvexityDefectRoi(const cv::Mat& Src, cv::Mat& Dest);

cv::Mat DrawPcaDistanceRoi(const cv::Mat& frame);

cv::Mat DrawDistanceMomentsRoi(const cv::Mat& frame);

cv::Mat DrawStickyDistanceRoi(const cv::Mat& frame);



inline std::pair<double, cv::Point2f> DirectionExPCA(const cv::Mat& mask) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
    if (contours.empty()) return { 0.0, cv::Point2f(0,0) };

    auto handContour = *std::max_element(contours.begin(), contours.end(),
        [](const auto& a, const auto& b) { return cv::contourArea(a) < cv::contourArea(b); });

    int sz = static_cast<int>(handContour.size());
    cv::Mat data_pts = cv::Mat(sz, 2, CV_64F);
    for (int i = 0; i < data_pts.rows; i++) {
        data_pts.at<double>(i, 0) = handContour[i].x;
        data_pts.at<double>(i, 1) = handContour[i].y;
    }

    cv::PCA pca_analysis(data_pts, cv::Mat(), cv::PCA::DATA_AS_ROW);

    cv::Point2f center = cv::Point2f(pca_analysis.mean.at<double>(0, 0),
        pca_analysis.mean.at<double>(0, 1));

    cv::Point2f eigen_vec = cv::Point2f(pca_analysis.eigenvectors.at<double>(0, 0),
        pca_analysis.eigenvectors.at<double>(0, 1));

    double angle = std::atan2(eigen_vec.y, eigen_vec.x);

    return { angle, center };
}




inline double DirectionExMoments(const cv::Mat& mask) {
    cv::Moments m = cv::moments(mask, true);

    double mu20 = m.mu20;
    double mu02 = m.mu02;
    double mu11 = m.mu11;

    double angle = 0.5 * std::atan2(2 * mu11, mu20 - mu02);

    cv::Point2f centroid(m.m10 / m.m00, m.m01 / m.m00);
    cv::Point2f testPt(centroid.x + cos(angle) * 30, centroid.y + sin(angle) * 30);

    if (testPt.x < 0 || testPt.x >= mask.cols || testPt.y < 0 || testPt.y >= mask.rows ||
        mask.at<uchar>(testPt) == 0) {
        angle += CV_PI;
    }

    return angle;
}


std::vector<cv::Point2f> ValleyExConvexityDefects(const cv::Mat& frame,
    float minDepth = 10.0f,
    int kernelSize = 5,
    float yOffset = 0.0f,
    int manualThreshold = -1
);


std::vector<cv::Point2f> ValleyExRadial(const cv::Mat& frame, float smoothSigma = 5.0f, int minNeighbor = 15);

struct HandData {
    bool found = false;
    cv::Point centroid{ -1, -1 };
    double size = 0.0;
};

class HandProcessor {
private:
    cv::Point last_centroid_{ -1, -1 };
    int stable_frames_ = 0;

    int base_box_size_;
    int base_roi_size_;
    int output_size_;
    double movement_tolerance_;
    int required_stable_frames_;
    int y_offset_;

    double expected_hand_size_;
    int min_box_size_;
    int max_box_size_;
    double min_hand_size_;


    HandData analyzeFrame(const cv::Mat& gray) {
        HandData data;
        cv::Mat blurred, thresh;
        cv::GaussianBlur(gray, blurred, cv::Size(7, 7), 0);
        cv::threshold(blurred, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        if (!contours.empty()) {
            double max_area = 0;
            int max_idx = -1;
            for (size_t i = 0; i < contours.size(); i++) {
                double area = cv::contourArea(contours[i]);
                if (area > max_area) {
                    max_area = area;
                    max_idx = static_cast<int>(i);
                }
            }

            if (max_idx != -1) {
                cv::Moments M = cv::moments(contours[max_idx]);
                if (M.m00 != 0) {
                    data.centroid.x = static_cast<int>(M.m10 / M.m00);
                    data.centroid.y = static_cast<int>(M.m01 / M.m00);
                    data.size = std::sqrt(max_area);
                    data.found = true;
                }
            }
        }
        return data;
    }

public:
    HandProcessor(int base_box_size = 150, int base_roi_size = 300, int output_size = 224,
        double movement_tolerance = 5.0, int required_stable_frames = 10, int y_offset = 60)
        : base_box_size_(base_box_size), base_roi_size_(base_roi_size), output_size_(output_size),
        movement_tolerance_(movement_tolerance), required_stable_frames_(required_stable_frames),
        y_offset_(y_offset) {

        expected_hand_size_ = 440.0;
        min_box_size_ = 40;
        max_box_size_ = 120;
        min_hand_size_ = 430.0;
    }


    bool waitForAlignment(const cv::Mat& frame, cv::Mat& display_frame) {
        if (frame.empty()) return false;

        cv::Mat gray;
        if (frame.channels() == 3) cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else gray = frame;

        HandData hand = analyzeFrame(gray);

        int center_x = gray.cols / 2;
        int center_y = (gray.rows / 2) - y_offset_;
        int current_box_size = base_box_size_;
        bool valid_distance = true;

        if (hand.found) {
            if (hand.size < min_hand_size_) valid_distance = false;

            double trigger_scale = expected_hand_size_ / hand.size;
            current_box_size = static_cast<int>(base_box_size_ * trigger_scale);
            current_box_size = std::max(min_box_size_, std::min(max_box_size_, current_box_size));
        }

        cv::Rect target_box(center_x - current_box_size / 2, center_y - current_box_size / 2,
            current_box_size, current_box_size);
        cv::Scalar box_color(0, 0, 255);

        if (hand.found) {
            cv::drawMarker(display_frame, hand.centroid, cv::Scalar(255, 0, 0), cv::MARKER_CROSS, 15, 2);

            if (target_box.contains(hand.centroid) && valid_distance) {
                box_color = cv::Scalar(0, 255, 255);

                double movement = (last_centroid_.x != -1) ? cv::norm(hand.centroid - last_centroid_) : 0.0;
                if (movement < movement_tolerance_) stable_frames_++;
                else stable_frames_ = 0;

                last_centroid_ = hand.centroid;

                if (stable_frames_ >= required_stable_frames_) {
                    stable_frames_ = 0;
                    return true;
                }
            }
            else {
                stable_frames_ = 0;
                last_centroid_ = cv::Point(-1, -1);
                if (target_box.contains(hand.centroid) && !valid_distance) {
                    box_color = cv::Scalar(0, 165, 255); 
                    cv::putText(display_frame, "MOVE CLOSER", cv::Point(target_box.x, target_box.y - 15),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, box_color, 2);
                }
            }
        }
        else {
            stable_frames_ = 0;
            last_centroid_ = cv::Point(-1, -1);
        }

        cv::rectangle(display_frame, target_box, box_color, 2);

        if (stable_frames_ > 0) {
            int bar_width = (stable_frames_ * current_box_size) / required_stable_frames_;
            cv::rectangle(display_frame, cv::Point(target_box.x, target_box.y - 10),
                cv::Point(target_box.x + bar_width, target_box.y - 5), cv::Scalar(0, 255, 255), -1);
        }

        return false;
    }

    cv::Mat extractDynamicROI(const cv::Mat& frame, int targetSize = 256) {
        if (frame.empty()) return cv::Mat();

        cv::Mat gray, blurred, thresh, distMap;

        if (frame.channels() == 3) cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else gray = frame;

        cv::GaussianBlur(gray, blurred, cv::Size(7, 7), 0);
        cv::threshold(blurred, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        if (contours.empty()) return cv::Mat();

        auto it = std::max_element(contours.begin(), contours.end(),
            [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
                return cv::contourArea(a) < cv::contourArea(b);
            });

        cv::Mat handMask = cv::Mat::zeros(thresh.size(), CV_8UC1);
        cv::drawContours(handMask, contours, std::distance(contours.begin(), it), cv::Scalar(255), cv::FILLED);

        cv::distanceTransform(handMask, distMap, cv::DIST_L2, 5);

        double maxVal;
        cv::Point centerPoint;
        cv::minMaxLoc(distMap, nullptr, &maxVal, nullptr, &centerPoint);

        int roiSize = static_cast<int>(maxVal * 2.2);
        int halfSize = roiSize / 2;

        int x = std::max(0, centerPoint.x - halfSize);
        int y = std::max(0, centerPoint.y - halfSize);
        int w = std::min(frame.cols - x, roiSize);
        int h = std::min(frame.rows - y, roiSize);

        cv::Rect roiRect(x, y, w, h);
        cv::Mat croppedRoi = frame(roiRect).clone();

        cv::Mat finalRoi;
        if (!croppedRoi.empty()) {
            cv::resize(croppedRoi, finalRoi, cv::Size(targetSize, targetSize));
        }

        return finalRoi;
    }

    cv::Mat ExtractGaborFeatures(const cv::Mat& roi) {
        cv::Mat gray;
        if (roi.channels() == 3) cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
        else gray = roi;

        cv::Mat enhanced;
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        clahe->apply(gray, enhanced);

        int ksize = 31;
        double sigma = 3.0;
        double lambd = 10.0;
        double gamma = 0.5; 
        double psi = 0;

        std::vector<double> angles = { 0, CV_PI / 4, CV_PI / 2, 3 * CV_PI / 4 };
        cv::Mat combinedFeatures = cv::Mat::zeros(enhanced.size(), CV_32F);

        for (double theta : angles) {
            cv::Mat kernel = cv::getGaborKernel(cv::Size(ksize, ksize), sigma, theta, lambd, gamma, psi, CV_32F);
            cv::Mat filtered;
            cv::filter2D(enhanced, filtered, CV_32F, kernel);

            cv::max(combinedFeatures, filtered, combinedFeatures);
        }

        cv::Mat out;
        cv::normalize(combinedFeatures, out, 0, 255, cv::NORM_MINMAX, CV_8U);
        return out;
    }

    std::vector<uchar> GeneratePalmCode(const cv::Mat& gaborResult) {
        cv::Mat binaryCode;
        cv::threshold(gaborResult, binaryCode, 0, 255, cv::THRESH_BINARY);
        binaryCode.convertTo(binaryCode, CV_8U);

        std::vector<uchar> bitstream;
        bitstream.assign(binaryCode.data, binaryCode.data + binaryCode.total());
        return bitstream;
    }

    void SaveBitstreamBinary(const std::vector<uchar>& bitstream, const std::string& filename) {
        std::ofstream outFile(filename, std::ios::out | std::ios::binary);
        if (outFile.is_open()) {
            outFile.write(reinterpret_cast<const char*>(bitstream.data()), bitstream.size());
            outFile.close();
            std::cout << "Successfully saved to " << filename << std::endl;
        }
        else {
            std::cerr << "Error: Could not open file for writing!" << std::endl;
        }
    }
};


inline cv::Mat FrangiFilter(const cv::Mat& roi, float sigma_start = 1.0f, float sigma_end = 3.0f, float sigma_step = 1.0f) {
    cv::Mat src;
    roi.convertTo(src, CV_32FC1, 1.0 / 255.0);

    const float beta = 0.8f;
    const float c = 0.08f;

    cv::Mat max_vesselness = cv::Mat::zeros(src.size(), CV_32FC1);

    for (float sigma = sigma_start; sigma <= sigma_end; sigma += sigma_step) {
        cv::Mat blurred;
        int ksize = (int)(4 * sigma + 1);
        if (ksize % 2 == 0) ksize++;
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
                float l1 = (fxx + fyy - tmp) / 2.0f;
                float l2 = (fxx + fyy + tmp) / 2.0f;

                if (l2 <= 0) continue;

                float Rb = std::abs(l1) / std::abs(l2);
                float S = std::sqrt(l1 * l1 + l2 * l2);

                float v = std::exp(-(Rb * Rb) / (2 * beta * beta)) * (1.0f - std::exp(-(S * S) / (2 * c * c)));

                vesselness.at<float>(y, x) = v;
            }
        }
        cv::max(max_vesselness, vesselness, max_vesselness);
    }

    cv::normalize(max_vesselness, max_vesselness, 0, 255, cv::NORM_MINMAX, CV_8U);
    return max_vesselness;
}


#endif 
