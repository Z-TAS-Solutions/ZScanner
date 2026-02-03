#ifndef UNICODE
#define UNICODE
#endif

#ifndef ZSCANCORE_H
#define ZSCANCORE_H

#pragma once

#include "RendererCore.h"
#include "WinForge.h"
#include "ZLogger.h"

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

};






class ZScan : public ZScanCore {
public:

	void ZScanMain(HINSTANCE hInstance, int nCmdShow);



private:

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