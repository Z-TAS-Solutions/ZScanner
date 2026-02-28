#ifndef UNICODE
#define UNICODE
#endif

#ifndef ZSCANCORE_H
#define ZSCANCORE_H

#pragma once

#include "RendererCore.h"
#include "WinForge.h"
#include "ZLogger.h"

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


class ZScanGUI;

enum Blur {
	None,
	MedianBlur,
	Bilateral,
	GaussianBlur
};

struct CVParams {
	Blur ActiveBlur = Blur::None;

	bool useMedian = false;
	bool useBilateral = false;
	bool useGaussian = false;

	// Config for Median, and.. keep it odd
	int  medianK = 3;

	// Config for Bilateral
	int    bilateralD = 9;
	float  sigmaColor = 75.0f;
	float  sigmaSpace = 75.0f;

	// Config for Gaussian, again.. keep it kernel odd
	int   gaussK = 5;
	float sigmaX = 1.5f;
	float sigmaY = 0.0f;


	int adaptiveThreshold = 11;
	int morphKernel = 7;

	float minDefectDepthRatio = 0.05f;

	int claheClipLimit = 1;
	cv::Size GridLimit = { 8, 8 };



};


static cv::Mat removeSmallComponents(const cv::Mat& bin255, int minArea)
{
	CV_Assert(bin255.type() == CV_8UC1);

	cv::Mat labels, stats, centroids;
	int n = cv::connectedComponentsWithStats(bin255, labels, stats, centroids, 8, CV_32S);

	cv::Mat out = cv::Mat::zeros(bin255.size(), CV_8UC1);
	for (int i = 1; i < n; i++) {
		int area = stats.at<int>(i, cv::CC_STAT_AREA);
		if (area >= minArea) out.setTo(255, labels == i);
	}
	return out;
}

static cv::Mat makeDistanceField(const cv::Mat& skel255)
{
	cv::Mat inv;
	cv::bitwise_not(skel255, inv);
	cv::Mat inv01 = inv > 0;

	cv::Mat dist32f;
	cv::distanceTransform(inv01, dist32f, cv::DIST_L2, 3);
	return dist32f;
}

static float chamferScoreShift(const cv::Mat& query255, const cv::Mat& templDist32f, int dx, int dy)
{
	cv::Mat shifted = cv::Mat::zeros(query255.size(), CV_8UC1);

	int x0s = std::max(0, dx), y0s = std::max(0, dy);
	int x0q = std::max(0, -dx), y0q = std::max(0, -dy);
	int w = query255.cols - std::abs(dx);
	int h = query255.rows - std::abs(dy);
	if (w <= 0 || h <= 0) return std::numeric_limits<float>::infinity();

	query255(cv::Rect(x0q, y0q, w, h)).copyTo(shifted(cv::Rect(x0s, y0s, w, h)));

	std::vector<cv::Point> pts;
	cv::findNonZero(shifted, pts);
	if (pts.empty()) return std::numeric_limits<float>::infinity();

	double sum = 0.0;
	for (const auto& p : pts) sum += templDist32f.at<float>(p.y, p.x);
	return (float)(sum / pts.size());
}

struct MatchResult { float score; int dx; int dy; };

static MatchResult matchChamferShiftSearch(const cv::Mat& query255, const cv::Mat& templ255, int maxShift)
{
	cv::Mat templDist = makeDistanceField(templ255);

	MatchResult best{ std::numeric_limits<float>::infinity(), 0, 0 };
	for (int dy = -maxShift; dy <= maxShift; dy++) {
		for (int dx = -maxShift; dx <= maxShift; dx++) {
			float s = chamferScoreShift(query255, templDist, dx, dy);
			if (s < best.score) best = { s, dx, dy };
		}
	}
	return best;
}



class ZScanCore {

public:

	bool verification = false;

	inline void SetMainFeedSize(cv::Mat& Frame) {
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = Frame.cols - 20;
		desc.Height = Frame.rows - 20;
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

	inline void UpdateMainFeed(cv::Mat src) {
		cv::cvtColor(src, RFrame, cv::COLOR_BGR2RGBA);
		D3D11Context->UpdateSubresource(MainFeedTex, 0, nullptr, RFrame.data, RFrame.step, 0);
	}


	inline void ApplyClahe() {
		CLengine->apply(MainFrame, MainFrame);
	}

	inline void UpdateClahe() {
		CLengine->setClipLimit(CV2Params.claheClipLimit);
		CLengine->setTilesGridSize(CV2Params.GridLimit);
	}


	inline void CaptureLiveFeed() {
		CaptureEngine.read(MainFrame);
		if (!CaptureEngine.read(MainFrame) || MainFrame.empty()) {
			return;
		}

		cv::Rect roi(280, 0, 720, 720);
		cv::Mat square = MainFrame(roi).clone();

		//CheckTypeData(MainFrame);
		cv::cvtColor(square, MainFrame, cv::COLOR_BGR2GRAY);

		if (redraw) {
			CLengine->setClipLimit(CV2Params.claheClipLimit);
		}
	}

	inline void CheckTypeData(cv::Mat& Frame) {
		std::cout << Frame.type() << std::endl;
		std::cout << Frame.channels() << std::endl;
	}

	inline inline void SetRedraw() {
		redraw = true;

		if (CV2Params.useBilateral) {
			CV2Params.ActiveBlur = Blur::Bilateral;
		}

		else if (CV2Params.useGaussian) {
			CV2Params.ActiveBlur = Blur::GaussianBlur;
		}

		else if (CV2Params.useMedian) {
			CV2Params.ActiveBlur = Blur::MedianBlur;
		}
		else {
			CV2Params.ActiveBlur = Blur::None;
		}
	}

	inline void skeletonize(const cv::Mat& input, cv::Mat& output) {
		cv::Mat img = input.clone();
		cv::threshold(img, img, 127, 255, cv::THRESH_BINARY);
		output = cv::Mat::zeros(img.size(), CV_8UC1);
		cv::Mat temp, eroded;
		cv::Mat element = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));

		bool done;
		do {
			erode(img, eroded, element);
			dilate(eroded, temp, element);
			subtract(img, temp, temp);
			bitwise_or(output, temp, output);
			eroded.copyTo(img);

			done = (countNonZero(img) == 0);
		} while (!done);
	}


	inline void ToggleMatching() {
		matching = !matching;
		if (matching) std::cout << "Matching Enabled" << "\n";
		else std::cout << "Matching Disabled" << "\n";

	}

	inline cv::Mat cutBorderOffset(const cv::Mat& input, int offsetX, int offsetY)
	{
		CV_Assert(!input.empty());

		cv::Rect roi(
			offsetX,                     
			offsetY,                        
			input.cols - 2 * offsetX,
			input.rows - 2 * offsetY
		);

		return input(roi).clone();
	}

	inline void Enroll() {
		Template = MainFrame.clone();
		std::cout << "Enrolled" << "\n";

	}

	inline double matchSkeletons(const cv::Mat& liveSkeleton, const cv::Mat& templateSkeleton) {
		cv::Mat invertedTemplate;
		cv::bitwise_not(templateSkeleton, invertedTemplate);

		cv::Mat distMap;
		cv::distanceTransform(invertedTemplate, distMap, cv::DIST_L2, 3);

		if (liveSkeleton.size() != distMap.size()) {
			std::cerr << "Size mismatch between live image and template!" << std::endl;
			return -1.0;
		}


		double totalDistance = 0;
		int pointCount = 0;

		for (int r = 0; r < liveSkeleton.rows; r++) {
			for (int c = 0; c < liveSkeleton.cols; c++) {
				if (liveSkeleton.at<uchar>(r, c) > 0) {
					totalDistance += distMap.at<float>(r, c);
					pointCount++;
				}
			}
		}

		if (pointCount == 0) return 1e10;
		return totalDistance / pointCount;
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
	bool matching = false;


	CVParams CV2Params;
	
	cv::Mat MaskFrame;

	cv::Mat MainFrame;

	cv::Mat RFrame;

	cv::Mat OFrame;

	cv::Mat kernel;

	cv::Mat Template;

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