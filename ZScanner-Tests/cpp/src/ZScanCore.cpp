#include <ZScanCore.h>
#include "ZScanOV.h"

struct UserTemplate {
	std::string name = "zischl";
	int id = 20232645;
};

#define rtsp "rtsp://admin::@192.168.1.168:80/ch0_0.264"

void ZScanCore::InitializeMonoExpansion(ZRenderer& Renderer)
{
	VertexShader = Renderer.CreateVertexShader(D3D11Device, L"VertexShader.cso").Get();
	D3D11Context->VSSetShader(VertexShader.Get(), nullptr, 0);

	PixelShader = Renderer.CreatePixelShader(D3D11Device, L"PixelShader.cso").Get();
	PixelShaderPtr = PixelShader.Get();

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.MaxAnisotropy = 1;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	D3D11Device->CreateSamplerState(&sampDesc, &Sampler);

	D3D11Context->PSSetSamplers(0, 1, &Sampler);

	D3D11Context->IASetInputLayout(nullptr);
	D3D11Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
}

void ZScanCore::ScanDirectory(const std::string path)
{
	Directories.clear();
	std::error_code ec;

	auto it = std::filesystem::directory_iterator(path, ec);

	if (ec) {
		return;
	}

	for (const auto& entry : it) {
		if (!entry.is_directory()) {
			Directories.push_back(entry.path().filename().string());
		}
	}
}

bool ZScanCore::CheckScannerStatus()
{
	//imma upgrade this shi later...
#ifdef _WIN32
	int result = system("ping -n 1 -w 200 ZTAS > nul");
#else
	int result = system("ping -c 1 -W 1 ZTAS > /dev/null 2>&1");
#endif

	return (result == 0);
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

bool ZScanCore::SetupGStreamerPipeline8Bit(const std::string& host, int port, StreamMode mode, bool GPUAccel, bool monochrome, cv::VideoCapture& cap)
{
	std::cout << cv::getBuildInformation() << std::endl;

	std::string PipelineCMD;

	if (mode == StreamMode::RTSP) {
		PipelineCMD = "rtspsrc location=" + host + " latency=0 ! rtph264depay ! h264parse ! ";
	}
	else if (mode == StreamMode::TCP) {
		PipelineCMD = "tcpclientsrc host=" + host + " port=" + std::to_string(port) + " ! h264parse ! ";
	}

	if (GPUAccel) {
		PipelineCMD += "nvh264dec ! nvvidconv ! ";
	}
	else {
		PipelineCMD += "avdec_h264 ! videoconvert ! ";
	}

	if (monochrome) {
		PipelineCMD += "video/x-raw,format=GRAY8 ! ";
	}
	else {
		PipelineCMD += "video/x-raw,format=BGR ! ";
	}

	PipelineCMD += "appsink sync=false drop=true";

	std::cout << "GStreamer: " << PipelineCMD << std::endl;

	cap.open(PipelineCMD, cv::CAP_GSTREAMER);
	if (!cap.isOpened()) {
		std::cerr << "Failed to open GStreamer!" << std::endl;
		return false;
	}

	cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
	return true;
}


bool ZScanCore::SetupGStreamerPipeline10Bit(const std::string& host, int port, StreamMode mode, bool GPUAccel, bool monochrome, cv::VideoCapture& cap)
{
	std::string PipelineCMD;

	switch (mode)
		case StreamMode::RTSP: {
		PipelineCMD = "rtspsrc location=" + host + " latency=50 ! ";
		PipelineCMD += "rtph264depay ! h264parse ! ";
		if (GPUAccel) {
			PipelineCMD += "nvh264dec ! ";
			PipelineCMD += "nvvidconv ! ";
		}
		else {
			PipelineCMD += "avdec_h264 ! videoconvert ! ";
		}

		case StreamMode::TCP: {
			PipelineCMD = "tcpclientsrc host=" + host + " port=" + std::to_string(port) + " !";
			PipelineCMD += "h264parse ! ";
			if (GPUAccel) {
				PipelineCMD += "nvh264dec ! nvvidconv ! ";
			}
			else {
				PipelineCMD += "avdec_h264 ! videoconvert ! ";
			}
		}

	}


	if (monochrome) {
		PipelineCMD += "video/x-raw,format=GRAY16_LE ! appsink";
	}
	else {
		PipelineCMD += "video/x-raw,format=BGR ! appsink";
	}

	std::cout << "GStreamer: " << PipelineCMD << std::endl;

	cap.open(PipelineCMD, cv::CAP_GSTREAMER);
	if (!cap.isOpened()) {
		std::cerr << "Failed to open GStreamer!" << std::endl;
		return false;
	}

	return true;
}


bool ZScanCore::OpenStream(const std::string& url) {


	int backend = cv::CAP_FFMPEG;

	CaptureEngine.open(url, backend);

	if (!CaptureEngine.isOpened()) {
		std::cerr << "Failed to open stream: " << url << std::endl;
		return false;
	}

	LiveFeedStatus = LiveFeedState::READY;

	CaptureEngine.set(cv::CAP_PROP_BUFFERSIZE, 1);

	CaptureEngine.read(MainFrame);

	SetMainFeedSize(MainFrame);

	std::cout << "Stream connected: " << url << std::endl;

	return true;

}

bool ZScanCore::OpenStream(const std::string_view ip, int port, StreamMode mode)
{
	std::cout << cv::getBuildInformation() << std::endl;
	if (OpenStream(GenerateStreamURL(mode, ip, port)))
	{
		return true;
		LiveFeedStatus = LiveFeedState::READY;
	}
	else return false;
}


bool ZScanCore::OpenGStream8Bit(const std::string& ip, int port, StreamMode mode)
{

	if (SetupGStreamerPipeline8Bit(ip, port, mode, false, true, CaptureEngine))
	{
		LiveFeedStatus = LiveFeedState::READY;
		return true;
	}
	else return false;

}

//can't use 10bit yet, have to make the pi output 10 bit manually
bool ZScanCore::OpenGStream10Bit(std::string ip, int port, StreamMode mode) 
{

	if (SetupGStreamerPipeline10Bit(ip, port, mode, false, true, CaptureEngine))
	{
		LiveFeedStatus = LiveFeedState::READY;
		return true;
	}
	else return false;
}


void ZScanCore::CloseStream()
{
	if (CaptureEngine.isOpened()) {
		CaptureEngine.release();
		std::cout << "Capture closed successfully." << std::endl;
	}
}

void ZScanCore::SetMainFeedSize(cv::Mat& Frame) {
	assert(Frame.isContinuous());

	if (MainFeedSRV) { MainFeedSRV->Release(); MainFeedSRV = nullptr; }
	if (MainFeedTex) { MainFeedTex->Release(); MainFeedTex = nullptr; }
	if (MainOutputFeedTex) { MainOutputFeedTex->Release(); MainOutputFeedTex = nullptr; }
	if (MainOutputFeedSRV) { MainOutputFeedSRV->Release(); MainOutputFeedSRV = nullptr; }
	if (MainOutputFeedRTV) { MainOutputFeedRTV->Release(); MainOutputFeedRTV = nullptr; }

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = Frame.cols;
	desc.Height = Frame.rows;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = Frame.data;
	data.SysMemPitch = static_cast<UINT>(Frame.step);

	HRESULT hr = D3D11Device->CreateTexture2D(&desc, &data, &MainFeedTex);
	if (FAILED(hr))
	{
		Logger::log("Texture Creation Failure For Main Feed ! : ", hr);
	}

	hr = D3D11Device->CreateShaderResourceView(MainFeedTex, nullptr, &MainFeedSRV);
	if (FAILED(hr))
	{
		Logger::log("SRV Creation Failure For Main Feed ! : ", hr);
	}

	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;


	hr = D3D11Device->CreateTexture2D(&desc, nullptr, &MainOutputFeedTex);
	if (FAILED(hr))
	{
		Logger::log("Texture Creation Failure For Main Feed ! : ", hr);
	}

	Renderer::CreateRTV(D3D11Device, MainOutputFeedTex, MainOutputFeedRTV);

	hr = D3D11Device->CreateShaderResourceView(MainOutputFeedTex, nullptr, &MainOutputFeedSRV);
	if (FAILED(hr))
	{
		Logger::log("SRV Creation Failure For Main Feed ! : ", hr);
	}

	MainOutViewPort.TopLeftX = 0;
	MainOutViewPort.TopLeftY = 0;
	MainOutViewPort.Width = static_cast<FLOAT>(Frame.cols);
	MainOutViewPort.Height = static_cast<FLOAT>(Frame.rows);
	MainOutViewPort.MinDepth = 0.0f;
	MainOutViewPort.MaxDepth = 1.0f;

}

bool ZScanCore::SaveCVImage(const cv::Mat& Frame, const std::string& FileName)
{
	if (Frame.empty()) {
		return false;
	}

	if (FileName == "") {
		std::string timestamp = std::format("Frame-{:%Y-%m-%d_%H-%M-%S}.png", std::chrono::system_clock::now());

		return cv::imwrite(timestamp, Frame);
	}
	
	return cv::imwrite(FileName, Frame);
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

	InitializeMonoExpansion(Renderer);

	GUI = new ZScanGUI(*this);
	GUI->SetupImGui(hwnd, D3D11Device, D3D11Context, Events);



	/*


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


			switch (ActiveMenu)
			{
			case MenuIndex::LiveFeed:
			{
				if (LiveFeedStatus == LiveFeedState::READY)
				{
					CaptureLiveFeed();
					UpdateMainFeed(MainFrame);
					//CheckTypeData(MainFrame);


					D3D11Context->OMSetRenderTargets(1, &MainOutputFeedRTV, nullptr);
					D3D11Context->RSSetViewports(1, &MainOutViewPort);

					D3D11Context->PSSetShader(PixelShader.Get(), nullptr, 0);
					D3D11Context->PSSetShaderResources(0, 1, &MainFeedSRV);

					D3D11Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
					D3D11Context->Draw(4, 0);


				}
				break;
			}
			case MenuIndex::ImageTest:
			{
				if (redraw) {

					/*CLengine->apply(MainImageFrame, MainImageFrame);


					switch (CV2Params.ActiveBlur) {
					case Blur::MedianBlur:
						cv::medianBlur(MainImageFrame, MainImageFrame, CV2Params.medianK);
						break;
					case Blur::Bilateral:
						cv::bilateralFilter(MainImageFrame, MaskFrame, CV2Params.bilateralD, CV2Params.sigmaColor, CV2Params.sigmaSpace);
						MainImageFrame = MaskFrame;
						break;
					case Blur::GaussianBlur:
						cv::GaussianBlur(MainImageFrame, MainImageFrame, cv::Size(CV2Params.gaussK, CV2Params.gaussK), CV2Params.sigmaX, CV2Params.sigmaY);
						break;
					}


					cv::Mat globalThresh;

					double otsuValue = cv::threshold(MainImageFrame, globalThresh, 0, 255,
						cv::THRESH_BINARY_INV | cv::THRESH_OTSU);


					cv::morphologyEx(globalThresh, globalThresh, cv::MORPH_OPEN, kernel);

					cv::Mat skeleton;
					skeletonize(globalThresh, skeleton);

					MainImageFrame = cutBorderOffset(skeleton, 10, 10);*/


					if (!MainFeedTex) SetMainFeedSize(MainImageFrame);

					UpdateMainFeed(MainImageFrame);

					D3D11Context->OMSetRenderTargets(1, &MainOutputFeedRTV, nullptr);
					D3D11Context->RSSetViewports(1, &MainOutViewPort);

					D3D11Context->PSSetShader(PixelShader.Get(), nullptr, 0);
					D3D11Context->PSSetShaderResources(0, 1, &MainFeedSRV);

					D3D11Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
					D3D11Context->Draw(4, 0);

					redraw = false;
				}


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
				break;
			}

			}

			if (reconfig) {
				CLengine->setClipLimit(CV2Params.claheClipLimit);
				CLengine->setTilesGridSize(CV2Params.GridLimit);

				kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(CV2Params.morphKernel, CV2Params.morphKernel));


				reconfig = false;
			}



			//##########################################################

			D3D11Context->ClearRenderTargetView(renderTargetView, clearColor);
			D3D11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);

			GUI->FrameBegin(MainOutputFeedSRV, MainFrame, OutputFeedSRV, Template, CV2Params);

			GUI->Render();

			swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
			LastFrameTime = std::chrono::steady_clock::now();



			/*if (LiveCapture) MainFrame = RFrame.clone();
			else*/
			//if (redraw) MainFrame = OFrame.clone();
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

