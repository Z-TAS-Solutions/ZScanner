#include <ZScanCore.h>


void ZScan::ZScanMain(HINSTANCE hInstance, int nCmdShow) {

	Events = new HANDLE[1];
	Events[0] = CreateEvent(NULL, FALSE, TRUE, L"PanelRender");


	//Control Panel Creation
	WinConfig config(L'Controller Window', 1920, 1080, L'Nexus', (LPVOID)this);
	hwnd = WindowInit(config, hInstance, nCmdShow, WProc);
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	InitTrayIcon(hwnd);

	ZRenderer Renderer;

	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	D3DDevice D3DDevStruct = Renderer.CreateD3d11Device(featureLevels, _countof(featureLevels), creationFlags);

	HWNDxD3D11 RendererPtrs;
	RendererPtrs.D3D11Device = D3DDevStruct.D3D11Device;
	RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
	Renderer.RendererInit(hwnd, 1920, 1080, RendererPtrs);
	D3D11Device = RendererPtrs.D3D11Device.Get();
	D3D11Context = RendererPtrs.D3D11Context.Get();
	swapchain = RendererPtrs.swapchain.Get();
	renderTargetView = RendererPtrs.renderTargetView.Get();



	ZScanMainLoop();
}




void ZScan::ZScanMainLoop() {

	while (true) {

		EventDW = MsgWaitForMultipleObjectsEx(1, Events, 10, QS_ALLINPUT, 0);
		switch (EventDW) {
		case WAIT_OBJECT_0 + 1:
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {

				if (msg.message == WM_QUIT)
					break;
				
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			/*if (std::chrono::steady_clock::now() - LastFrameTime >= FrameTimeLimit) {
				SetEvent(Events[0]);
			}*/



			break;

		case WAIT_OBJECT_0 + 0:
		{	
			
			D3D11Context->ClearRenderTargetView(renderTargetView, clearColor);
			D3D11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);
			//D3D11Context->Draw(4, 0);

			swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
			LastFrameTime = std::chrono::steady_clock::now();

		}
			break;


		case WAIT_TIMEOUT:
			SetEvent(Events[0]);

			break;

		}


	}
}

void ZScan::InitTrayIcon(HWND hwnd) {

	TrayIconData.cbSize = sizeof(NOTIFYICONDATAW);
	TrayIconData.hWnd = hwnd;
	TrayIconData.uID = 62485;
	TrayIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	TrayIconData.uCallbackMessage = WM_TRAYICON;
	//TrayIconData.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ZIcon));
	lstrcpy(TrayIconData.szTip, L"ZScan");

	Shell_NotifyIcon(NIM_ADD, &TrayIconData);
}

LRESULT CALLBACK ZScan::WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {


	ZScan* zscan = reinterpret_cast<ZScan*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));


	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_CLOSE:
		ShowWindow(hwnd, SW_HIDE);
		Shell_NotifyIcon(NIM_DELETE, &(zscan->TrayIconData));
		return 0;
	case WM_SETCURSOR:
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		return true;

	case WM_NCCREATE:
		zscan = static_cast<ZScan*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(zscan));
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

