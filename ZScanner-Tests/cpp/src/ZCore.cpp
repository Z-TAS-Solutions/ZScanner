
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


std::vector<cv::Mat> InitializeGaborBank() {
    std::vector<cv::Mat> Bank;
    int NumOrientations = 6;

    int KSize = 15;
    double Sigma = 3.0;
    double Lambda = 10.0;
    double Gamma = 0.5;

    for (int I = 0; I < NumOrientations; I++) {
        double Theta = I * CV_PI / NumOrientations;

        cv::Mat Kernel = cv::getGaborKernel(
            cv::Size(KSize, KSize),
            Sigma,
            Theta,
            Lambda,
            Gamma,
            0,
            CV_32F
        );
        Bank.push_back(Kernel);
    }
    return Bank;
}
