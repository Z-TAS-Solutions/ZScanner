#include <ZScanCore.h>
#include "ZScanOV.h"

struct UserTemplate {
	std::string name = "zischl";
	int id = 20232645;
};

#define rtsp "rtsp://admin::@192.168.1.168:80/ch0_0.264"

void ZScanCore::ScanDirectory(std::vector<std::string>& Dst, const std::string path)
	{

	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		if (!entry.is_directory())
		{
			Dst.push_back(entry.path().filename().string());
		}
	}

	return;
	}


std::string ZScanCore::GenerateStreamURL(StreamMode mode, std::string_view ip, int port)
{
	std::string url;
	url.reserve(48);

	switch (mode)
	{
	case StreamMode::TCP:
		url += ip;
		url += ":";
		url += std::to_string(port);
		break;

	case StreamMode::RTSP:
		url += ip;
		url += ":";
		url += std::to_string(port);
		url += "/cam";
		break;

	default:
		return "";
	}

	return url;
}


bool ZScanCore::OpenStream(const std::string& url, StreamMode mode) {

	switch (mode)
		case StreamMode::TCP:
			break;

	int backend = cv::CAP_FFMPEG;

	CaptureEngine.open(url, backend);

	if (!CaptureEngine.isOpened()) {
		std::cerr << "[Error] Failed to open stream: " << url << std::endl;
		return false;
	}

	CaptureEngine.set(cv::CAP_PROP_BUFFERSIZE, 1);

	std::cout << "[Success] Stream connected: " << url << std::endl;
	return true;
}

bool ZScanCore::OpenStream(std::string_view ip, int port, StreamMode mode) {
	if (OpenStream(GenerateStreamURL(mode, ip, port), mode)) return true;
	else return false;
}

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


	/*ScanDirectory(Directories, R"(D:\Workspace\Repos\Z-TAS\ZScanner-Tests\cpp\Images\)");


	MainFrame = cv::imread(R"(D:\Workspace\Repos\Z-TAS\ZScanner-Tests\cpp\Images\ROI12.png)", cv::IMREAD_GRAYSCALE);

	if (MainFrame.empty()) {
		MainFrame = cv::imread(R"(D:\Workspace\Repos\Z-TAS\ZScanner-Tests\cpp\Images\ROI12.png)", cv::IMREAD_UNCHANGED);
		if (MainFrame.depth() != CV_8U)
			MainFrame.convertTo(MainFrame, CV_8U);

		if (MainFrame.empty()) {
			return;
		}
	}

	cv::cvtColor(MainFrame, RFrame, cv::COLOR_GRAY2RGBA);
	OFrame = MainFrame.clone();*/


	/*if (!CaptureEngine.open("rtsp://admin::@192.168.1.168:80/ch0_0.264", cv::CAP_FFMPEG)) {
		std::cerr << "Error: Could not open RTSP stream." << std::endl;
		return;
	}

	CaptureEngine.set(cv::CAP_PROP_FRAME_WIDTH, 640);
	CaptureEngine.set(cv::CAP_PROP_FRAME_HEIGHT, 640);

	

	CaptureEngine.read(MainFrame);

	cv::cvtColor(MainFrame, RFrame, cv::COLOR_BGR2RGBA);*/



	SetMainFeedSize(RFrame);
	SetOutputFeedSize(RFrame);

	CV2Params.claheClipLimit = 1;

	CLengine = cv::createCLAHE();

	CLengine->setClipLimit(CV2Params.claheClipLimit);
	CLengine->setTilesGridSize(CV2Params.GridLimit);

	kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(CV2Params.morphKernel, CV2Params.morphKernel));

	RFrame = MainFrame.clone();

	ZScanMainLoop();
}


void ZScan::ZScanMainLoop() {

	while (true) {

		EventDW = MsgWaitForMultipleObjectsEx(1, Events, 0, QS_ALLINPUT, 0);
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
			//CaptureLiveFeed();


			if (reconfig) {
				CLengine->setClipLimit(CV2Params.claheClipLimit);
				CLengine->setTilesGridSize(CV2Params.GridLimit);

				kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(CV2Params.morphKernel, CV2Params.morphKernel));


				reconfig = false;
			}
			
			if (redraw) {				

			/*
				CLengine->apply(MainFrame, MainFrame);


				switch (CV2Params.ActiveBlur) {
				case Blur::MedianBlur:
					cv::medianBlur(MainFrame, MainFrame, CV2Params.medianK);
					break;
				case Blur::Bilateral:
					cv::bilateralFilter(MainFrame, MaskFrame, CV2Params.bilateralD, CV2Params.sigmaColor, CV2Params.sigmaSpace);
					MainFrame = MaskFrame;
					break;
				case Blur::GaussianBlur:
					cv::GaussianBlur(MainFrame, MainFrame, cv::Size(CV2Params.gaussK, CV2Params.gaussK), CV2Params.sigmaX, CV2Params.sigmaY);
					break;
				}


				cv::Mat globalThresh;

				double otsuValue = cv::threshold(MainFrame, globalThresh, 0, 255,
					cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

				
				cv::morphologyEx(globalThresh, globalThresh, cv::MORPH_OPEN, kernel);

				cv::Mat skeleton;
				skeletonize(globalThresh, skeleton);

				MainFrame = cutBorderOffset(skeleton, 10, 10);

				redraw = false;
			}
			

			UpdateMainFeed(MainFrame);

			if (matching) {

				cv::Mat q = removeSmallComponents(MainFrame, 10);
				cv::Mat t = removeSmallComponents(Template, 10);

				cv::dilate(q, q, cv::getStructuringElement(cv::MORPH_CROSS, { 3,3 }));
				cv::dilate(t, t, cv::getStructuringElement(cv::MORPH_CROSS, { 3,3 }));

				MatchResult r = matchChamferShiftSearch(q, t, 20);

				std::cout << "Score=" << r.score << " dx=" << r.dx << " dy=" << r.dy << "\n";

				verification = (r.score < 2.5f);


				/*if (matchSkeletons(MainFrame, Template) < 2.5f) {
					verification = true;
				}*/



				if (verification) {
					matching = false;
				}
			}
			


			GUI->FrameBegin(MainFeedSRV, MainFrame, OutputFeedSRV, Template, CV2Params);

			D3D11Context->ClearRenderTargetView(renderTargetView, clearColor);
			D3D11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);
			//D3D11Context->Draw(4, 0);

			//##########################################################


			GUI->Render();

			swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
			LastFrameTime = std::chrono::steady_clock::now();

			

			/*if (LiveCapture) MainFrame = RFrame.clone();
			else*/ 
			if (redraw) MainFrame = OFrame.clone();
			//MainFrame = RFrame.clone();
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

