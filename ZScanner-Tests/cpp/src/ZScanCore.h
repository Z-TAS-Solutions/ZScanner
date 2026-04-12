#ifndef UNICODE
#define UNICODE
#endif

#ifndef ZSCANCORE_H
#define ZSCANCORE_H

#pragma once

#include "SSHHandler.h"
#include "RendererCore.h"
#include "WinForge.h"
#include "ZLogger.h"


#include <opencv2/opencv.hpp>
#include "ZCore.h"

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
#include <filesystem>
#include <memory>
#include <cstdio>

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

enum StreamMode {
	RTSP,
	TCP
};

enum CaptureMode {
	Default,
	GStream
};

struct StreamConfig {
	int width = 1280;
	int height = 720;
	int bitDepth = 0;
	int captureMode = CaptureMode::Default;
	int fpsLimit = 60;
	int renderFpsLimit = 75;
};

struct CameraConfig {
	int shutterSpeed = 0;
	int iso = 100;
	int awbMode = 0;
	int rotation = 0;
	bool hFlip = false;
	bool vFlip = false;
	int timeout = 5000;
};

enum MenuIndex {
	Dashboard,
	LiveFeed,
	ImageTest,
	Database,
	Settings
};

enum LiveFeedState {
	CLOSED,
	READY,
	LIVE
};


struct FSizeWxH {
	float width;
	float height;

	FSizeWxH(float Width, float Height) : width(Width), height(Height) {}
};

struct ISizeWxH {
	int width;
	int height;

	ISizeWxH(int Width, int Height) : width(Width), height(Height) {}
};


enum class ThresholdType {
	Global,
	Otsu,
	AdaptiveMean,
	AdaptiveGaussian
};

enum SharpenTypes {
	SharpenKernel,
	SharpenUnsharp,
	SharpenLaplacian,
	Frangi
};

enum FilterTypes {
	CLAHE,
	MedianBlur,
	BilateralBlur,
	GaussianBlur,
	Threshold,
	Morphology,
	Skeletonize,
	Sharpen,
	Canny,
	Sobel,
	BrightnessContrast,
	Invert,
	Gamma,
	HistEqGlobal,
	BoxBlur,
};


struct FilterCLAHE {
	float claheClipLimit = 5;
	cv::Size GridLimit = { 16, 16 };
};

struct FilterMedianBlur {
	int medianK = 5;
};

struct FilterBilateralBlur {
	int bilateralD = 9;
	float sigmaColor = 75.0f;
	float sigmaSpace = 75.0f;
};

struct FilterGaussianBlur {
	int gaussK = 5;
	float sigmaX = 1.5f;
	float sigmaY = 0.0f;
};

struct FilterThreshold {
	ThresholdType ThresholdType = ThresholdType::AdaptiveGaussian;
	float GlobalThreshold = 127.0f;
	float MaxBinaryValue = 255.0f;
	int AdaptiveBlockSize = 11;
	float AdaptiveC = 2.0f;
};

struct FilterMorphology {
	cv::MorphShapes MorphShape = cv::MorphShapes::MORPH_RECT;
	int MorphKernelSize = 3;
	int MorphIterations = 1;
	cv::MorphTypes MorphType = cv::MORPH_OPEN;
};

struct FilterSkeletonize {
	int PruningIterations = 0;
};

struct FilterSharpen {
	SharpenTypes SharpenType = SharpenTypes::SharpenKernel;
	float KernelStrength = 1.0f;
	float UnsharpSigma = 1.0f;
	float UnsharpAmount = 1.5f;
	int UnsharpThreshold = 0;
	int LaplacianKSize = 3;
	float LaplacianScale = 1.0f;
	float LaplacianSubAlpha = 0.5f;
	int RidgeKSize = 3;
	float RidgeScale = 1.0f;
	float RidgeAlpha = 0.5f;
	float RidgeBeta = 0.5f;
};

struct FilterCanny {
	float threshold1 = 100.0f;
	float threshold2 = 200.0f;
	int apertureSize = 3;
	bool L2gradient = false;
};

struct FilterSobel {
	int dx = 1;
	int dy = 0;
	int ksize = 3;
	float scale = 1.0f;
	float delta = 0.0f;
};

struct FilterBrightnessContrast {
	float alpha = 1.0f; // Scale
	float beta = 0.0f;  // Shift
};

struct FilterInvert {
	// Zero parameters
};

struct FilterGamma {
	float gamma = 1.0f;
};

struct FilterHistEq {
	// Zero parameters
};

struct FilterBoxBlur {
	int ksize = 3;
};

struct FilterDistanceTransform {
	int maskSize = 3;
};

using FilterConfigVariant = std::variant<
	std::monostate,
	FilterCLAHE,
	FilterMedianBlur,
	FilterBilateralBlur,
	FilterGaussianBlur,
	FilterThreshold,
	FilterMorphology,
	FilterSkeletonize,
	FilterSharpen,
	FilterCanny,
	FilterSobel,
	FilterBrightnessContrast,
	FilterInvert,
	FilterGamma,
	FilterHistEq,
	FilterBoxBlur,
	FilterDistanceTransform
>;

struct FilterNode {
	FilterTypes Type;
	FilterConfigVariant Config;
};

struct CVParams {
	std::vector<FilterNode> Filters;

	int adaptiveThreshold = 11;
	float minDefectDepthRatio = 0.05f;

};



class ZScanCore {

public:

	bool verification = false;
	std::vector<std::string> Directories;
	MenuIndex ActiveMenu = MenuIndex::Dashboard;
	LiveFeedState LiveFeedStatus = LiveFeedState::CLOSED;

	bool ScannerSignIn(const std::string& IP, int Port, const std::string& Username, const std::string& PubKeyPath, const std::string& PrvKeyPath, const std::string& Passphrase);

	void UpdateStreamConfig(const StreamConfig& Config);

	void UpdateCameraConfig(const CameraConfig& Config);

	bool CheckScannerStatus();

	bool SetupGStreamerPipeline8Bit(const std::string& host, int port, StreamMode mode, bool GPUAccel, bool monochrome, cv::VideoCapture& cap);

	bool SetupGStreamerPipeline10Bit(const std::string& host, int port, StreamMode mode, bool GPUAccel, bool monochrome, cv::VideoCapture& cap);

	std::string GenerateStreamURL(StreamMode mode, std::string_view ip, int port);

	bool OpenStream(const std::string& url); 

	bool OpenStream(const std::string_view ip, int port, StreamMode mode);

	bool OpenGStream8Bit(const std::string& ip, int port, StreamMode mode);

	bool OpenGStream10Bit(std::string ip, int port, StreamMode mode);

	void CloseStream();

	bool Capture2ImageAnalysis();

	bool ExportLiveFeedImage(const std::string& filename = "");

	inline void CaptureLiveFeed() {
		CaptureEngine.read(MainFrame);
		if (!CaptureEngine.read(MainFrame) || MainFrame.empty()) {
			return;
		}
		


	}

	

	

	inline void UpdateMainFeed(cv::Mat& srcFrame) 
	{

		D3D11Context->UpdateSubresource(MainFeedTex, 0, nullptr, srcFrame.data, srcFrame.step, 0);

	}

	inline void UpdateImageFeed(cv::Mat& srcFrame)
	{
		D3D11Context->UpdateSubresource(MainFeedTex, 0, nullptr, srcFrame.data, srcFrame.step, 0);

	}

	inline void UpdateSubFeed(ID3D11Texture2D* Texture2d, cv::Mat& srcFrame) {
		D3D11Context->CopyResource(Texture2d, SubFeedTex);
		D3D11Context->UpdateSubresource(SubFeedTex, 0, nullptr, srcFrame.data, srcFrame.step, 0);
	}



	inline void ApplyClahe() {
		CLengine->apply(MainFrame, MainFrame);
	}

	/*inline void UpdateClahe() {
		CLengine->setClipLimit(CV2Params.claheClipLimit);
		CLengine->setTilesGridSize(CV2Params.GridLimit);
	}*/


	

	inline void CheckTypeData(cv::Mat& Frame) {
		std::cout << Frame.type() << std::endl;
		std::cout << Frame.channels() << std::endl;
		std::cout << Frame.depth() << std::endl;
	}

	 inline void SetReconfig() {
		reconfig = true;
		redraw = true;
	}

	 inline void SetRedraw() {
		 redraw = true;
	}

	inline void SkeletonizeIMM(const cv::Mat& input, cv::Mat& output) {
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

		redraw = true;

	}

	inline void onRedrawDirMode() {

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
		//Template = MainFrame.clone();
		cv::Mat enhancedLive;
		cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(3.0, cv::Size(8, 8));
		clahe->apply(MainFrame, enhancedLive);
		ExtractCompCode(enhancedLive, GarborVec, Template);

		std::cout << "Enrolled" << "\n";

	}

	inline void UpdateImageFeed(std::string FilePath) {

		MainImageFrame = cv::imread(FilePath, cv::IMREAD_GRAYSCALE);

		if (MainImageFrame.empty()) {
			MainImageFrame = cv::imread(FilePath, cv::IMREAD_UNCHANGED);
			if (MainImageFrame.depth() != CV_8U)
				MainImageFrame.convertTo(MainImageFrame, CV_8U);

			if (MainImageFrame.empty()) {
				return;
			}
		}

		

		if (CheckMainFeedSizeMismatch(MainImageFrame))
		{
			ResizeMonoExpansionPipeline(MainImageFrame);
			SetSubFeedSize(MainImageFrame);
		}

		OriginalFrame = MainImageFrame.clone();

		redraw = true;
		
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

	inline void ClearVerification() {
		verification = false;
	}


	void ScanDirectory(const std::string path);

protected:
	
	HANDLE* Events = nullptr;
	DWORD EventDW = NULL;

	ZSSHHandler SSHEngine{};
	
	std::mutex Mutex;

	ID3D11Device* D3D11Device = nullptr;
	ID3D11DeviceContext* D3D11Context = nullptr;
	IDXGISwapChain3* swapchain = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;

	ComPtr<ID3D11PixelShader> PixelShader; 
	ID3D11PixelShader* PixelShaderPtr = nullptr;
	ComPtr<ID3D11VertexShader> VertexShader;
	ID3D11SamplerState* Sampler;

	ID3D11Texture2D* MainFeedTex = nullptr;
	ID3D11ShaderResourceView* MainFeedSRV = nullptr;

	ID3D11Texture2D* MainOutputFeedTex = nullptr;
	ID3D11ShaderResourceView* MainOutputFeedSRV = nullptr;
	ID3D11RenderTargetView* MainOutputFeedRTV = nullptr;
	D3D11_VIEWPORT MainOutViewPort = {};

	ID3D11Texture2D* SubFeedTex = nullptr;
	ID3D11ShaderResourceView* SubFeedSRV = nullptr;


	bool reconfig = false;
	bool redraw = false;
	bool matching = false;

	CVParams CV2Params;
	
	cv::Mat MainFrame;

	cv::Mat MainImageFrame;

	cv::Mat OriginalFrame;

	cv::Mat ClaheFrame;

	cv::Mat ThresholdFrame;

	cv::Mat MorphKernelFrame;

	std::vector<cv::Mat> GarborVec;


	cv::Mat MaskFrame;

	cv::Mat RFrame;


	cv::Mat Template;

	cv::VideoCapture CaptureEngine;

	cv::Ptr<cv::CLAHE> CLengine;


	void InitializeMonoExpansion(ZRenderer& Renderer);

	void SetupMonoExpansionInput(cv::Mat& Frame);

	void SetupMonoExpansionOutput(const ISizeWxH& Size);

	void SetupMonoExpansion(cv::Mat& Frame);

	void ResizeMonoExpansionPipeline(cv::Mat& Frame);

	void ReleaseMonoExpansion();

	bool CheckMonoExpansionStatus();

	inline bool CheckMainFeedSizeMismatch(cv::Mat& Frame)
	{
		return (Frame.cols != (int) MainOutViewPort.Width || Frame.rows != (int) MainOutViewPort.Height);
	}

	void SetSubFeedSize(cv::Mat& Frame);


};






class ZScan : public ZScanCore {
public:
	
	void ZScanMain(HINSTANCE hInstance, int nCmdShow);

	inline void ModeSwitch(MenuIndex Index) {
		ActiveMenu = Index;
		switch (Index)
		{
		case MenuIndex::LiveFeed:
		{
			switch (LiveFeedStatus)
			{
			case LiveFeedState::CLOSED:
			{
				ReleaseMonoExpansion();
				break;
			}
			case LiveFeedState::READY || LiveFeedState::LIVE:
			{
				if (!CheckMonoExpansionStatus() || CheckMainFeedSizeMismatch(MainFrame))
				{
					SetupMonoExpansion(MainFrame);
				}
				break;
			}
			}
			break;
		}

		case MenuIndex::ImageTest:
		{
			if (MainImageFrame.empty())
			{
				ReleaseMonoExpansion();
			}
			else
			{
				if (!CheckMonoExpansionStatus() || CheckMainFeedSizeMismatch(MainImageFrame))
				{
					SetupMonoExpansion(MainImageFrame);

					SetRedraw();
				}
				else SetRedraw();
			}

			break;
		}
		}
	}

	void SetRenderFPS(const uint32_t& FPS)
	{
		FrameTimeLimit = std::chrono::nanoseconds(((1 / FPS) * 1000000000));
	}

private:
	//GUI
	ZScanGUI* GUI = nullptr;

	HWND hwnd;

	NOTIFYICONDATAW TrayIconData = {};

	std::chrono::steady_clock::duration FrameTimeLimit = std::chrono::nanoseconds(12 * 1000000);

	std::chrono::time_point<std::chrono::steady_clock> LastFrameTime = std::chrono::steady_clock::now();

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };


	static LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	MSG msg = { };

	void ZScanMainLoop();

	void InitTrayIcon(HWND hwnd);

};

#endif