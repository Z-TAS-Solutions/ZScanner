#include <ZScanCore.h>




#define rtsp "rtsp://admin::@192.168.1.168:80/ch0_0.264"



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


	GUI = new ZScanGUI(*this);
	GUI->SetupImGui(hwnd, D3D11Device, D3D11Context, Events);

	MainFrame = cv::imread(R"(D:\Workspace\Repos\Z-TAS\ZScanner-Tests\cpp\Images\044.jpeg)", cv::IMREAD_GRAYSCALE);

	if (MainFrame.empty()) {
		MainFrame = cv::imread(R"(D:\Workspace\Repos\Z-TAS\ZScanner-Tests\cpp\Images\026_3.jpg)", cv::IMREAD_UNCHANGED);
		if (MainFrame.depth() != CV_8U)
			MainFrame.convertTo(MainFrame, CV_8U);

		if (MainFrame.empty()) {
			return;
		}
	}

	cv::cvtColor(MainFrame, RFrame, cv::COLOR_GRAY2RGBA);



	/*if (!CaptureEngine.open("rtsp://admin::@192.168.1.168:80/ch0_0.264", cv::CAP_FFMPEG)) {
		std::cerr << "Error: Could not open RTSP stream." << std::endl;
		return;
	}

	CaptureEngine.read(MainFrame);

	cv::cvtColor(MainFrame, RFrame, cv::COLOR_BGR2RGBA);*/

	SetMainFeedSize(RFrame);

	BSParams.claheClipLimit = 0;

	CLengine = cv::createCLAHE();

	CLengine->setClipLimit(BSParams.claheClipLimit);
	CLengine->setTilesGridSize(BSParams.GridLimit);


	RFrame = MainFrame.clone();

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
			if (!redraw) {
				//CaptureROI(RFrame, MainFrame);
				cv::GaussianBlur(MainFrame, MainFrame, cv::Size(13	, 13), 0);

				cv::Canny(MainFrame, MainFrame, 40, 150, 3);

				cv::cvtColor(MainFrame, MainFrame, cv::COLOR_BGR2RGBA);

				D3D11Context->UpdateSubresource(MainFeedTex, 0, nullptr, MainFrame.data, MainFrame.step, 0);
			}
			
			//CaptureLiveFeed();


			GUI->FrameBegin(MainFeedSRV, MainFrame, BSParams);

			D3D11Context->ClearRenderTargetView(renderTargetView, clearColor);
			D3D11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);
			//D3D11Context->Draw(4, 0);

			//##########################################################

			




			GUI->Render();

			swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
			LastFrameTime = std::chrono::steady_clock::now();

			MainFrame = RFrame.clone();

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

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


LRESULT CALLBACK ZScan::WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {


	ZScan* zscan = reinterpret_cast<ZScan*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
		return true;

	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
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

