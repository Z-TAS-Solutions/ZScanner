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



class StableHandExtractor {
private:
    cv::Point last_centroid_{ -1, -1 };
    int stable_frames_ = 0;

    int base_box_size_;
    int output_size_;
    double movement_tolerance_;
    int required_stable_frames_;
    int y_offset_;

    double expected_hand_size_;
    int min_box_size_;
    int max_box_size_;

    double min_hand_size_;

public:
    StableHandExtractor(int base_box_size = 150, int output_size = 224,
        double movement_tolerance = 5.0, int required_stable_frames = 10,
        int y_offset = 65)
        : base_box_size_(base_box_size), output_size_(output_size),
        movement_tolerance_(movement_tolerance), required_stable_frames_(required_stable_frames),
        y_offset_(y_offset) {

        expected_hand_size_ = 440.0;
        min_box_size_ = 40;
        max_box_size_ = 120;

        min_hand_size_ = 430.0;
    }

    cv::Mat process(const cv::Mat& frame, cv::Mat& display_frame) {
        if (frame.empty()) return cv::Mat();

        cv::Mat gray;
        if (frame.channels() == 3) cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else gray = frame;

        int h = gray.rows;
        int w = gray.cols;

        int center_x = w / 2;
        int center_y = (h / 2) - y_offset_;

        int current_box_size = base_box_size_;

        cv::Mat blurred, thresh;
        cv::GaussianBlur(gray, blurred, cv::Size(7, 7), 0);
        cv::threshold(blurred, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        cv::Point current_centroid(-1, -1);
        bool hand_found = false;
        double max_area = 0;

        if (!contours.empty()) {
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
                    current_centroid.x = static_cast<int>(M.m10 / M.m00);
                    current_centroid.y = static_cast<int>(M.m01 / M.m00);
                    hand_found = true;
                }
            }
        }

        bool valid_distance = true;

        if (hand_found && max_area > 0) {
            double hand_size = std::sqrt(max_area);

            if (hand_size < min_hand_size_) {
                valid_distance = false;
            }

            double scale_factor = expected_hand_size_ / hand_size;

            current_box_size = static_cast<int>(base_box_size_ * scale_factor);
            current_box_size = std::max(min_box_size_, std::min(max_box_size_, current_box_size));
        }

        cv::Rect center_box(center_x - current_box_size / 2,
            center_y - current_box_size / 2,
            current_box_size, current_box_size);

        cv::Scalar box_color(0, 0, 255);
        cv::Mat final_roi;

        if (hand_found) {
            cv::drawMarker(display_frame, current_centroid, cv::Scalar(255, 0, 0), cv::MARKER_CROSS, 15, 2);

            if (center_box.contains(current_centroid) && valid_distance) {
                box_color = cv::Scalar(0, 255, 255);

                double movement = 0.0;
                if (last_centroid_.x != -1) {
                    movement = cv::norm(current_centroid - last_centroid_);
                }

                if (movement < movement_tolerance_) stable_frames_++;
                else stable_frames_ = 0;

                last_centroid_ = current_centroid;

                if (stable_frames_ >= required_stable_frames_) {
                    box_color = cv::Scalar(0, 255, 0);

                    cv::Rect crop_rect(current_centroid.x - output_size_ / 2,
                        current_centroid.y - output_size_ / 2,
                        output_size_, output_size_);

                    cv::Rect img_bounds(0, 0, w, h);
                    cv::Rect valid_rect = crop_rect & img_bounds;

                    final_roi = cv::Mat::zeros(output_size_, output_size_, gray.type());

                    if (valid_rect.area() > 0) {
                        int offset_x = valid_rect.x - crop_rect.x;
                        int offset_y = valid_rect.y - crop_rect.y;
                        cv::Rect paste_rect(offset_x, offset_y, valid_rect.width, valid_rect.height);
                        gray(valid_rect).copyTo(final_roi(paste_rect));
                    }

                    stable_frames_ = 0;
                }
            }
            else {
                stable_frames_ = 0;
                last_centroid_ = cv::Point(-1, -1);

                if (center_box.contains(current_centroid) && !valid_distance) {
                    box_color = cv::Scalar(0, 165, 255);
                    cv::putText(display_frame, "MOVE CLOSER",
                        cv::Point(center_box.x - 10, center_box.y - 15),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, box_color, 2);
                }
            }
        }
        else {
            stable_frames_ = 0;
            last_centroid_ = cv::Point(-1, -1);
        }

        cv::rectangle(display_frame, center_box, box_color, 2);

        if (stable_frames_ > 0) {
            int bar_width = (stable_frames_ * current_box_size) / required_stable_frames_;
            cv::rectangle(display_frame, cv::Point(center_box.x, center_box.y - 10),
                cv::Point(center_box.x + bar_width, center_box.y - 5), cv::Scalar(0, 255, 255), -1);
        }

        return final_roi;
    }
};
#endif 
