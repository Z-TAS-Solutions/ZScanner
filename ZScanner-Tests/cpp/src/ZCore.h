#ifndef ZCORE_H
#define ZCORE_H

#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/ximgproc.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>


void ApplyLbp(const cv::Mat& src, cv::Mat& dst);

std::vector<cv::Mat> InitializeGaborBank();

void ExtractCompCode(const cv::Mat& Src, const std::vector<cv::Mat>& GaborBank, cv::Mat& Dest);

void VisualizeCompCode(const cv::Mat& Src, const cv::Mat& Dest);

void ExtractVeinSkeleton(const cv::Mat& Src, cv::Mat& Dest);

void ExtractFrangiVeins(const cv::Mat& Src, cv::Mat& Dest, float Sigma = 2.0f);

void XimgprocSkeletonize(const cv::Mat& Src, cv::Mat& Dest);

cv::Mat ExtractDistanceTransformRoi(const cv::Mat& frame, cv::Point& centerPoint, int& dynamicRoiSize);

cv::Mat AnnotateMomentsRoi(const cv::Mat& frame, int roiSize, cv::Point& centerPoint);

cv::Mat AnnotateConvexityDefectRoi(const cv::Mat& frame, cv::Point& centerPoint, int& dynamicRoiSize);

cv::Mat DrawPcaDistanceRoi(const cv::Mat& frame);

cv::Mat DrawDistanceMomentsRoi(const cv::Mat& frame);

cv::Mat DrawStickyDistanceRoi(const cv::Mat& frame);

#include <opencv2/opencv.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

// A simple struct to hold the hand data so we don't recalculate it
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

    // --- SHARED HELPER FUNCTION ---
    // Does the heavy OpenCV lifting once per frame
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
        double movement_tolerance = 5.0, int required_stable_frames = 10, int y_offset = 65)
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

    cv::Mat extractDynamicROI(const cv::Mat& frame, cv::Mat& display_frame) {
        if (frame.empty()) return cv::Mat();

        cv::Mat gray;
        if (frame.channels() == 3) cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else gray = frame;

        HandData hand = analyzeFrame(gray);
        cv::Mat final_roi;

        if (hand.found) {
            double roi_scale = hand.size / expected_hand_size_;
            int current_roi_size = static_cast<int>(base_roi_size_ * roi_scale);
            current_roi_size = std::max(100, std::min(std::min(gray.cols, gray.rows), current_roi_size));

            cv::Rect drawn_roi_box(hand.centroid.x - current_roi_size / 2,
                hand.centroid.y - current_roi_size / 2,
                current_roi_size, current_roi_size);

            cv::rectangle(display_frame, drawn_roi_box, cv::Scalar(0, 255, 0), 3);
            cv::putText(display_frame, "CAPTURED!", cv::Point(drawn_roi_box.x, drawn_roi_box.y - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);

            cv::Rect img_bounds(0, 0, gray.cols, gray.rows);
            cv::Rect valid_rect = drawn_roi_box & img_bounds;
            cv::Mat dynamic_crop = cv::Mat::zeros(current_roi_size, current_roi_size, gray.type());

            if (valid_rect.area() > 0) {
                int offset_x = valid_rect.x - drawn_roi_box.x;
                int offset_y = valid_rect.y - drawn_roi_box.y;
                cv::Rect paste_rect(offset_x, offset_y, valid_rect.width, valid_rect.height);
                gray(valid_rect).copyTo(dynamic_crop(paste_rect));
            }

            cv::resize(dynamic_crop, final_roi, cv::Size(output_size_, output_size_), 0, 0, cv::INTER_AREA);
        }

        return final_roi;
    }
};
#endif 
