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


	int threshold = 70;
	int morphKernel = 7;
	float minDefectDepthRatio = 0.05f;
	int claheClipLimit = 0;
	cv::Size GridLimit = { 16, 16 };



};

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
		CLengine->setClipLimit(CV2Params.claheClipLimit);
		CLengine->setTilesGridSize(CV2Params.GridLimit);
	}


	inline void CaptureLiveFeed() {
		CaptureEngine.read(MainFrame);
		UpdateMainFeed();
	}

	inline void CheckTypeData(cv::Mat& Frame) {
		std::cout << Frame.type() << std::endl;
		std::cout << Frame.channels() << std::endl;
	}

	inline void SetRedraw() {
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

	CVParams CV2Params;
	
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