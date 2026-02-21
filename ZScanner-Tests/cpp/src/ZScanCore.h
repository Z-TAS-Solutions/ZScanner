#ifndef UNICODE
#define UNICODE
#endif

#ifndef ZSCANCORE_H
#define ZSCANCORE_H

#pragma once

#include "RendererCore.h"
#include "WinForge.h"
#include "ZLogger.h"
#include "ZScanOV.h"

#include <opencv2/opencv.hpp>

#include <Windows.h>
#include <dwmapi.h>
#pragma comment (lib, "dwmapi.lib")
#include <shellapi.h>
#include <string>
#include <vector>
#include <deque>
#include <variant>
#include <array>
#include <wrl/client.h>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <d3d11.h>
#include <dxgi1_5.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <dcomp.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dcomp.lib")
#include <wincodec.h>
#include <comdef.h>



#ifndef ZScan_BUILD_RELEASE
//#pragma comment(lib, "")
#endif

#ifndef ZScan_BUILD_DEBUG
//#pragma comment(lib, "")
#endif

#include <thread>


#define WM_TRAYICON (WM_USER + 1)




class ZScanCore {

public:
	inline void SetMainFeedSize(cv::Mat& Frame) {
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = Frame.cols;
		desc.Height = Frame.rows;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = Frame.data;
		data.SysMemPitch = Frame.step;

		D3D11Device->CreateTexture2D(&desc, &data, &MainFeedTex);
		D3D11Device->CreateShaderResourceView(MainFeedTex, nullptr, &MainFeedSRV);
	}

	inline void UpdateMainFeed() {
		cv::cvtColor(MainFrame, RFrame, cv::COLOR_BGR2RGBA);
		D3D11Context->UpdateSubresource(MainFeedTex, 0, nullptr, RFrame.data, RFrame.step, 0);
	}


	inline void ApplyClahe() {
		CLengine->apply(MainFrame, MainFrame);
	}

	inline void UpdateClahe() {
		CLengine->setClipLimit(BSParams.claheClipLimit);
		CLengine->setTilesGridSize(BSParams.GridLimit);
	}




    static inline void KeepLargestComponent(cv::Mat& binMask)
    {
        CV_Assert(binMask.type() == CV_8U);

        cv::Mat labels, stats, centroids;
        int n = cv::connectedComponentsWithStats(binMask, labels, stats, centroids, 8, CV_32S);

        if (n <= 1) {
            binMask.setTo(0);
            return;
        }

        int bestLabel = 1;
        int bestArea = stats.at<int>(1, cv::CC_STAT_AREA);

        for (int i = 2; i < n; ++i)
        {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area > bestArea) { bestArea = area; bestLabel = i; }
        }

        binMask = (labels == bestLabel);
        binMask.convertTo(binMask, CV_8U, 255);
    }


    static inline bool CaptureDTROI(
        const cv::Mat& srcFrame,
        cv::Mat& dstFrame,
        cv::Rect& outRoi,
        int thresholdVal,
        int morphK = 11,
        float roiScale = 0.85f,
        bool invertThreshold = false,
        int minSide = 64
    )
    {
        if (srcFrame.empty()) return false;

        cv::Mat gray;
        if (srcFrame.channels() == 3)
            cv::cvtColor(srcFrame, gray, cv::COLOR_BGR2GRAY);
        else
            gray = srcFrame;

        cv::Mat mask;
        int threshType = invertThreshold ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY;
        cv::threshold(gray, mask, thresholdVal, 255, threshType);

        morphK = std::max(3, morphK);
        if ((morphK % 2) == 0) morphK += 1;
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(morphK, morphK));

        cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

        KeepLargestComponent(mask);

        if (cv::countNonZero(mask) == 0) return false;

        cv::Mat dist;
        cv::distanceTransform(mask, dist, cv::DIST_L2, 5);

        double maxVal = 0.0;
        cv::Point maxLoc;
        cv::minMaxLoc(dist, nullptr, &maxVal, nullptr, &maxLoc);

        if (maxVal <= 0.0) return false;

        int r = (int)std::floor(maxVal * std::clamp(roiScale, 0.05f, 1.0f));
        int side = 2 * r;

        if (side < minSide) return false;

        int x = maxLoc.x - r;
        int y = maxLoc.y - r;

        x = std::max(0, x);
        y = std::max(0, y);

        side = std::min(side, srcFrame.cols - x);
        side = std::min(side, srcFrame.rows - y);

        if (side <= 0) return false;

        outRoi = cv::Rect(x, y, side, side);

        if (srcFrame.channels() == 3) dstFrame = srcFrame.clone();
        else cv::cvtColor(srcFrame, dstFrame, cv::COLOR_GRAY2BGR);

        cv::rectangle(dstFrame, outRoi, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
        cv::circle(dstFrame, maxLoc, 3, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);

        return true;
    }


	inline void CaptureValleyROI(cv::Mat& srcFrame, cv::Mat& dstFrame) {

		MaskFrame = srcFrame.clone();

		//CLengine->apply(MaskFrame, MaskFrame);

		//cv::GaussianBlur(MaskFrame, MaskFrame, cv::Size(15, 15), 0);

		cv::threshold(MaskFrame, MaskFrame, BSParams.threshold, 255, cv::THRESH_BINARY);

		cv::Mat kernel = cv::getStructuringElement(
			cv::MORPH_ELLIPSE, cv::Size(BSParams.morphKernel, BSParams.morphKernel)
		);
		cv::morphologyEx(MaskFrame, MaskFrame, cv::MORPH_CLOSE, kernel);


		std::vector<std::vector<cv::Point>> contours;
		cv::findContours(MaskFrame, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

		int largestIdx = -1;
		double largestArea = 0;

		for (size_t i = 0; i < contours.size(); ++i) {	
			double area = cv::contourArea(contours[i]);
			if (area > largestArea) {
				largestArea = area;
				largestIdx = i;
			}
		}

		std::vector<cv::Point> mainContour = contours[largestIdx];
			


		std::vector<int> hullIndices;
		cv::convexHull(mainContour, hullIndices);

		std::vector<cv::Vec4i> defects;
		cv::convexityDefects(mainContour, hullIndices, defects);


		std::vector<cv::Point> hullPoints;
		for (int idx : hullIndices) {
			hullPoints.push_back(mainContour[idx]);
		}

		std::vector<std::vector<cv::Point>> hullVec = { hullPoints };
		//cv::drawContours(dstFrame, hullVec, 0, cv::Scalar(0, 0, 255), 2);



		//for (const auto& d : defects) {
		//	cv::Point start = mainContour[d[0]];
		//	cv::Point end = mainContour[d[1]];
		//	cv::Point pfar = mainContour[d[2]];

		//	//cv::line(dstFrame, start, pfar, cv::Scalar(255, 0, 0), 1);
		//	//cv::line(dstFrame, end, pfar, cv::Scalar(255, 0, 0), 1);

		//	cv::circle(dstFrame, pfar, 5, cv::Scalar(0, 128, 20), -1);
		//}

		//cv::drawContours(dstFrame, contours, largestIdx, cv::Scalar(244, 50, 44), 2);

        DrawPalmRoiFromDefects(mainContour, hullIndices, defects, dstFrame);

	}


    static inline cv::Point2f ClampPt(const cv::Point2f& p, int W, int H) {
        return cv::Point2f(
            std::max(0.f, std::min(p.x, (float)(W - 1))),
            std::max(0.f, std::min(p.y, (float)(H - 1)))
        );
    }

    static inline float AngleDeg(const cv::Point& start, const cv::Point& end, const cv::Point& pfar) {
        cv::Point2f v1 = cv::Point2f((float)(start.x - pfar.x), (float)(start.y - pfar.y));
        cv::Point2f v2 = cv::Point2f((float)(end.x - pfar.x), (float)(end.y - pfar.y));
        float n1 = std::sqrt(v1.dot(v1));
        float n2 = std::sqrt(v2.dot(v2));
        if (n1 < 1e-6f || n2 < 1e-6f) return 180.f;
        float c = (v1.dot(v2)) / (n1 * n2);
        c = std::max(-1.f, std::min(1.f, c));
        return std::acos(c) * 180.0f / 3.14159265f;
    }

    struct Valley {
        cv::Point p;
        float depth;
    };

    bool DrawPalmRoiFromDefects(
        const std::vector<cv::Point>& mainContour,
        const std::vector<int>& hullIndices,
        const std::vector<cv::Vec4i>& defects,
        cv::Mat& dstFrame
    ) {
        if (mainContour.empty() || hullIndices.size() < 3 || defects.empty()) return false;

        cv::Moments mu = cv::moments(mainContour);
        if (std::abs(mu.m00) < 1e-6) return false;
        cv::Point2f center((float)(mu.m10 / mu.m00), (float)(mu.m01 / mu.m00));

        cv::Rect bb = cv::boundingRect(mainContour);

        std::vector<Valley> cand;
        cand.reserve(defects.size());

        const float minDepth = 12.f;
        const float maxAngle = 120.f;

        for (const auto& d : defects) {
            cv::Point start = mainContour[d[0]];
            cv::Point end = mainContour[d[1]];
            cv::Point pfar = mainContour[d[2]];

            float depth = d[3] / 256.0f;

            if (depth < minDepth) continue;

            if (pfar.y > bb.y + (int)(0.75f * bb.height)) continue;

            float ang = AngleDeg(start, end, pfar);
            if (ang > maxAngle) continue;

            cand.push_back({ pfar, depth });
        }

        if (cand.size() < 2) return false;

        std::sort(cand.begin(), cand.end(), [](const Valley& a, const Valley& b) {
            return a.depth > b.depth;
            });

        const int topK = std::min<int>((int)cand.size(), 8);
        const float minSep = 60.f;
        const float maxSep = 220.f;

        int bestI = -1, bestJ = -1;
        float bestScore = -1.f;

        for (int i = 0; i < topK; ++i) {
            for (int j = i + 1; j < topK; ++j) {
                cv::Point2f a((float)cand[i].p.x, (float)cand[i].p.y);
                cv::Point2f b((float)cand[j].p.x, (float)cand[j].p.y);
                float w = cv::norm(b - a);
                if (w < minSep || w > maxSep) continue;

                float score = (cand[i].depth + cand[j].depth) + 0.25f * w;
                if (score > bestScore) {
                    bestScore = score;
                    bestI = i; bestJ = j;
                }
            }
        }

        if (bestI < 0) return false;

        cv::Point2f P1((float)cand[bestI].p.x, (float)cand[bestI].p.y);
        cv::Point2f P2((float)cand[bestJ].p.x, (float)cand[bestJ].p.y);

        cv::Point2f mid = (P1 + P2) * 0.5f;
        cv::Point2f v = (P2 - P1);
        float w = std::sqrt(v.dot(v));
        if (w < 1e-6f) return false;

        cv::Point2f xAxis = v * (1.0f / w);
        cv::Point2f yAxis(-xAxis.y, xAxis.x);

        if (((center - mid).dot(yAxis)) < 0.f) yAxis = -yAxis;

        float side = 3.7f * w;
        float offset = 2.3f * w;

        cv::Point2f roiCenter = mid + yAxis * offset;
		float lateral = 0.34f * w; 
		roiCenter -= xAxis * lateral;
        float half = side * 0.5f;

        cv::Point2f C0 = roiCenter - xAxis * half - yAxis * half;
        cv::Point2f C1 = roiCenter + xAxis * half - yAxis * half;
        cv::Point2f C2 = roiCenter + xAxis * half + yAxis * half;
        cv::Point2f C3 = roiCenter - xAxis * half + yAxis * half;

        int W = dstFrame.cols, H = dstFrame.rows;
        C0 = ClampPt(C0, W, H);
        C1 = ClampPt(C1, W, H);
        C2 = ClampPt(C2, W, H);
        C3 = ClampPt(C3, W, H);

        std::vector<cv::Point> poly = {
            cv::Point((int)C0.x, (int)C0.y),
            cv::Point((int)C1.x, (int)C1.y),
            cv::Point((int)C2.x, (int)C2.y),
            cv::Point((int)C3.x, (int)C3.y),
        };

        cv::circle(dstFrame, cv::Point((int)P1.x, (int)P1.y), 6, cv::Scalar(0, 255, 255), -1);
        cv::circle(dstFrame, cv::Point((int)P2.x, (int)P2.y), 6, cv::Scalar(0, 255, 255), -1);

        cv::polylines(dstFrame, poly, true, cv::Scalar(0, 255, 0), 2);

        return true;
    }




	inline void CaptureLiveFeed() {
		CaptureEngine.read(MainFrame);
		//CaptureROI();
		UpdateMainFeed();
	}




protected:

	HANDLE* Events = nullptr;
	DWORD EventDW = NULL;
	
	std::mutex Mutex;

	ID3D11Device* D3D11Device = nullptr;
	ID3D11DeviceContext* D3D11Context = nullptr;
	IDXGISwapChain3* swapchain = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;

	ID3D11Texture2D* MainFeedTex = nullptr;
	ID3D11ShaderResourceView* MainFeedSRV = nullptr;
	bool redraw = false;

	CVParams BSParams;
	
	cv::Mat MaskFrame;

	cv::Mat MainFrame;

	cv::Mat RFrame;

	cv::VideoCapture CaptureEngine;

	cv::Ptr<cv::CLAHE> CLengine;
};






class ZScan : public ZScanCore {
public:

	void ZScanMain(HINSTANCE hInstance, int nCmdShow);



private:
	//GUI
	ZScanGUI* GUI = nullptr;

	HWND hwnd;

	NOTIFYICONDATAW TrayIconData = {};

	std::chrono::steady_clock::duration FrameTimeLimit = std::chrono::nanoseconds(15 * 1000000);

	std::chrono::time_point<std::chrono::steady_clock> LastFrameTime = std::chrono::steady_clock::now();

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };


	static LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	MSG msg = { };

	void ZScanMainLoop();

	void InitTrayIcon(HWND hwnd);

};

#endif