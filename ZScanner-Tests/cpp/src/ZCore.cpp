
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


void ExtractCompCode(const cv::Mat& Src, const std::vector<cv::Mat>& GaborBank, cv::Mat& Dest) {
    cv::Mat SrcFloat, InvertedSrc;

    cv::bitwise_not(Src, InvertedSrc);
    InvertedSrc.convertTo(SrcFloat, CV_32F);

    cv::Mat MaxResponse = cv::Mat::zeros(SrcFloat.size(), CV_32F);
    Dest = cv::Mat::zeros(SrcFloat.size(), CV_8UC1);

    for (int I = 0; I < GaborBank.size(); I++) {
        cv::Mat Response;
        cv::filter2D(SrcFloat, Response, CV_32F, GaborBank[I]);

        for (int Y = 0; Y < SrcFloat.rows; Y++) {
            const float* RespRow = Response.ptr<float>(Y);
            float* MaxRow = MaxResponse.ptr<float>(Y);
            uchar* DestRow = Dest.ptr<uchar>(Y);

            for (int X = 0; X < SrcFloat.cols; X++) {
                if (RespRow[X] > MaxRow[X]) {
                    MaxRow[X] = RespRow[X];
                    DestRow[X] = I;
                }
            }
        }
    }
}


void VisualizeCompCode(const cv::Mat& Src, const cv::Mat& Dest) {
    Src.convertTo(Dest, CV_8UC1, 50);
}


void ExtractVeinSkeleton(const cv::Mat& Src, cv::Mat& Dest) {
    cv::Mat Enhanced;
    cv::Ptr<cv::CLAHE> Clahe = cv::createCLAHE(3.0, cv::Size(8, 8));
    Clahe->apply(Src, Enhanced);

    cv::Mat Kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(15, 15));
    cv::morphologyEx(Enhanced, Dest, cv::MORPH_BLACKHAT, Kernel);

    cv::threshold(Dest, Dest, 10, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
}


void ExtractFrangiVeins(const cv::Mat& Src, cv::Mat& Dest, float Sigma) {
    cv::Mat Inverted;
    cv::bitwise_not(Src, Inverted);

    cv::Mat Smoothed;
    cv::GaussianBlur(Inverted, Smoothed, cv::Size(0, 0), Sigma);

    cv::Mat Dxx, Dyy, Dxy;
    cv::Sobel(Smoothed, Dxx, CV_32F, 2, 0, 3);
    cv::Sobel(Smoothed, Dyy, CV_32F, 0, 2, 3);
    cv::Sobel(Smoothed, Dxy, CV_32F, 1, 1, 3);

    cv::Mat Vesselness = cv::Mat::zeros(Src.size(), CV_32F);

    float Beta = 0.5f;
    float C = 10.0f;

    for (int Y = 0; Y < Src.rows; Y++) {
        const float* DxxRow = Dxx.ptr<float>(Y);
        const float* DyyRow = Dyy.ptr<float>(Y);
        const float* DxyRow = Dxy.ptr<float>(Y);
        float* VRow = Vesselness.ptr<float>(Y);

        for (int X = 0; X < Src.cols; X++) {
            float DxxVal = DxxRow[X];
            float DyyVal = DyyRow[X];
            float DxyVal = DxyRow[X];

            float Tmp1 = (DxxVal + DyyVal) * 0.5f;
            float Tmp2 = std::sqrt((DxxVal - DyyVal) * (DxxVal - DyyVal) * 0.25f + DxyVal * DxyVal);

            float Lambda1 = Tmp1 + Tmp2;
            float Lambda2 = Tmp1 - Tmp2;

            if (std::abs(Lambda1) > std::abs(Lambda2)) {
                std::swap(Lambda1, Lambda2);
            }

            if (Lambda2 >= 0) {
                VRow[X] = 0.0f;
                continue;
            }

            float Rb = Lambda1 / Lambda2;
            float S2 = Lambda1 * Lambda1 + Lambda2 * Lambda2;

            float Blobness = std::exp(-(Rb * Rb) / (2.0f * Beta * Beta));
            float Structure = 1.0f - std::exp(-S2 / (2.0f * C * C));

            VRow[X] = Blobness * Structure;
        }
    }

    cv::normalize(Vesselness, Dest, 0, 255, cv::NORM_MINMAX, CV_8UC1);
}

void XimgprocSkeletonize(const cv::Mat& Src, cv::Mat& Dest)
{
    cv::ximgproc::thinning(Src, Dest, cv::ximgproc::THINNING_ZHANGSUEN);
}
