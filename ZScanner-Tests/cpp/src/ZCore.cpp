
#include "ZCore.h"

void applyLBP(const cv::Mat& src, cv::Mat& dst) {
    dst = cv::Mat::zeros(src.rows - 2, src.cols - 2, CV_8UC1);
    for (int i = 1; i < src.rows - 1; i++) {
        for (int j = 1; j < src.cols - 1; j++) {
            uchar center = src.at<uchar>(i, j);
            uchar code = 0;
            code |= (src.at<uchar>(i - 1, j - 1) >= center) << 7;
            code |= (src.at<uchar>(i - 1, j) >= center) << 6;
            code |= (src.at<uchar>(i - 1, j + 1) >= center) << 5;
            code |= (src.at<uchar>(i, j + 1) >= center) << 4;
            code |= (src.at<uchar>(i + 1, j + 1) >= center) << 3;
            code |= (src.at<uchar>(i + 1, j) >= center) << 2;
            code |= (src.at<uchar>(i + 1, j - 1) >= center) << 1;
            code |= (src.at<uchar>(i, j - 1) >= center) << 0;
            dst.at<uchar>(i - 1, j - 1) = code;
        }
    }
}