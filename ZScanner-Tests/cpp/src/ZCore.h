#ifndef ZCORE_H
#define ZCORE_H

#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <iostream>


void applyLBP(const cv::Mat& src, cv::Mat& dst);

std::vector<cv::Mat> InitializeGaborBank();

void ExtractCompCode(const cv::Mat& Src, const std::vector<cv::Mat>& GaborBank, cv::Mat& Dest);

void VisualizeCompCode(const cv::Mat& Src, const cv::Mat& Dest);

#endif 
