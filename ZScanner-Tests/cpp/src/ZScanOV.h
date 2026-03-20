#ifndef OMNIGUI_H
#define OMNIGUI_H

#pragma once

#include "ZScanCore.h"

#include <Windows.h>
#include <wrl/client.h>

#include <array>
#include <charconv>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"

#include <opencv2/opencv.hpp>

#include <DDSTextureLoader.h>

#pragma comment(lib, "d3d11.lib") 
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

class ZScan;


#define ICON_HOME reinterpret_cast<const char*>(u8"\uE900")
#define ICON_VIDEO reinterpret_cast<const char*>(u8"\uE903")
#define ICON_IMAGE reinterpret_cast<const char*>(u8"\uE902")
#define ICON_COG reinterpret_cast<const char*>(u8"\uE901")
#define ICON_DATABASE reinterpret_cast<const char*>(u8"\uE904")

#define LIVE_PANEL_HEIGHT 720
#define LIVE_PANEL_WIDTH 720



class ZScanGUI {
public:
	ZScanGUI(ZScan& OmniLinkInstance);

	void SetupImGui(HWND hwnd, ID3D11Device* D3D11Device, ID3D11DeviceContext* D3D11Context, HANDLE* Events);

	inline void DrawLiveFeedPanel() {
		ImGui::Text("Live Camera Feed");
		//ImGui::Image((void*)SRV, ImVec2(1280, 720));
	}

	inline void Dashboard() {

		ImGui::SetCursorPosY(100);

		ImGui::BeginChild("LeftPanel", ImVec2(500, 0));

		if (ActiveStreamMode == StreamMode::TCP) {
			if (ImGui::Button("TCP", ImVec2(80, 30))) ActiveStreamMode = StreamMode::TCP;
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, InactiveColor);
			if (ImGui::Button("RTSP", ImVec2(80, 30))) ActiveStreamMode = StreamMode::RTSP;
			ImGui::PopStyleColor();

			ImGui::Spacing();

			ImGui::InputText("Prefix", StreamProtocolTCP, IM_ARRAYSIZE(StreamProtocolTCP));
			ImGui::InputInt("Port", &StreamTCPPort);

			ImGui::Spacing();
			if(LiveFeedState) 
			{
				if (ImGui::Button("Disconnect", ImVec2(-1, 0))) {
					App->CloseStream();
					UpdateConsole("Disconnected !");
					LiveFeedState = false;
				}
			}
			else {
				if (ImGui::Button("Connect", ImVec2(-1, 0))) {
					if (!App->OpenStream(StreamProtocolTCP, StreamTCPPort, ActiveStreamMode)) {
						UpdateConsole("Could not connect !");

					}
					else
					{
						UpdateConsole("Connected : Live Feed Ready !");
						ScannerState = App->CheckScannerStatus();
						LiveFeedState = true;
					}
				}
			}
			
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Button, InactiveColor);
			if (ImGui::Button("TCP", ImVec2(80, 30))) ActiveStreamMode = StreamMode::TCP;
			ImGui::PopStyleColor();
			ImGui::SameLine();
			if (ImGui::Button("RTSP", ImVec2(80, 30))) ActiveStreamMode = StreamMode::RTSP;

			ImGui::Spacing();

			ImGui::InputText("Prefix", StreamProtocolRTSP, IM_ARRAYSIZE(StreamProtocolRTSP));
			ImGui::InputInt("Port", &StreamRTSPPort);

			ImGui::Spacing();
			if (LiveFeedState)
			{
				if (ImGui::Button("Disconnect", ImVec2(-1, 0))) {
					App->CloseStream();
					LiveFeedState = false;
					UpdateConsole("Disconnected !");
				}
			}
			else {
				if (ImGui::Button("Connect", ImVec2(-1, 0))) {
					if (!App->OpenGStream8Bit(StreamProtocolRTSP + 7, StreamRTSPPort, ActiveStreamMode)) {
						UpdateConsole("Could not connect !");
					}
					else
					{
						UpdateConsole("Connected : Live Feed Ready !");
						ScannerState = App->CheckScannerStatus();
						LiveFeedState = true;
					}
				}
			}
		}

		

		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		ImGui::SetCursorPosY(100);

		ImGui::BeginChild("RightPanel", ImVec2(0, 0));

		ImGui::Text("ZScanner v1.0 | Status: % s ", ScannerState ? "Online" : "Offline");

		// ImGui::Image(laters..., ImVec2(256, 256));

		ImGui::Separator();

		ImGui::InputText("IP", SSH_IP, 64);
		ImGui::InputText("PORT", SSH_PORT, 64);	
		ImGui::InputText("Username", SSH_Username, 64);
		ImGui::InputText("SSH Key Path", SSH_KeyPath, 256);
		ImGui::InputText("Passphrase", SSH_Passphrase, 128, ImGuiInputTextFlags_Password);

		if (ImGui::Button(ScannerState ? "Scanner Log In" : "Scan Network", ImVec2(-1, 0))) {



			if (ScannerState)
			{
				int port = 0;
				std::from_chars(SSH_PORT, SSH_PORT + strlen(SSH_PORT), port);

				std::string PrvKey = SSH_KeyPath;
				PrvKey += ".pub";

				if (App->ScannerSignIn(SSH_IP, port, SSH_Username, SSH_KeyPath, PrvKey, SSH_Passphrase))
				{
					UpdateConsole("Succesfully Logged In : Admin Access Received !");

					
				} else UpdateConsole("Failed To Log In, Check Again !");

			}

			ScannerState = App->CheckScannerStatus();

		}

		if (AdminState) {
			if (ImGui::Button("Shutdown Scanner"))
			{

			}
		}

		if (ScannerState)
		{
			if (ImGui::Button("Start RTSP Stream"))
			{
			}

			ImGui::SameLine();

			if (ImGui::Button("Start TCP Stream"))
			{

			}
		}


		ImGui::EndChild();

		ImGui::SetCursorPos(ImVec2(ImVec2(10, ImGui::GetIO().DisplaySize.y - 35)));
		ImGui::PushItemWidth(-1.0f);
		ImGui::InputText("##Console", Console, sizeof(Console), ImGuiInputTextFlags_ReadOnly);
		ImGui::PopItemWidth();
		

	}

	inline void LiveFeedPanel(ID3D11ShaderResourceView* SRV, const ImVec2& FrameSize) 
	{
		ImGui::TextColored(ScannerState ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), "Status: %s", ScannerState ? "Online" : "Offline");
		ImGui::SameLine();
		ImGui::Text(" | Mode: %s | FPS: %.1f", ActiveStreamMode ? "TCP" : "RTSP", ImGui::GetIO().Framerate);

		ImGui::BeginChild("LiveFeedPanel", ImVec2(LIVE_PANEL_WIDTH, LIVE_PANEL_HEIGHT), true, ImGuiWindowFlags_NoScrollbar);
		

		if (SRV) {
			ImGui::Image(SRV, FrameSize);
		}
		else {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "No feed available, set the damned feed in the dashboard !");
		}

		
		ImGui::EndChild();

		ImGui::Spacing();

		if (ImGui::Button("Capture", ImVec2(100, 30))) {
			App->Capture2ImageAnalysis();
			CurrentMenu = MenuIndex::ImageTest;
			App->ModeSwitch(CurrentMenu);
		}

		ImGui::SameLine();

		if (ImGui::Button("Export", ImVec2(100, 30))) {
			App->ExportLiveFeedImage();
		}


		
	}

	inline void DrawImageTestPanel(ID3D11ShaderResourceView* MainSRV, const ImVec2& MainFrameSize, ID3D11ShaderResourceView* SubSRV, const ImVec2& SubFrameSize, CVParams& MaskParams) {


		//ImGui::Checkbox("Live Mode", &(App->LiveFeedStatus));

		ModuleMenu(MaskParams);

		ImGui::Text("General Settings");

		ImGui::Separator();


		ImGui::Separator();

		if (ImGui::Button("Apply Config", ImVec2(150, 30)))
		{
			App->SetReconfig();
			App->SetRedraw();
		}
		ImGui::SameLine();
		

		ImGui::Spacing();
		ImGui::Text("Input Feed");
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.5f, 1.0f, 0.8f));
		

		if (DirScanCombo("Directory", App->Directories , ImagePathBuffer, 256, SelectedImage))
		{
			const std::string& current_item = "\\" + App->Directories[SelectedImage];
			App->UpdateImageFeed(ImagePathBuffer + current_item);
		}


		ImGui::PopStyleColor();


		ImGui::BeginChild("MainFeedPanel", ImVec2(LIVE_PANEL_WIDTH, LIVE_PANEL_HEIGHT), true, ImGuiWindowFlags_NoScrollbar);


		if (MainSRV) {
			ImGui::Image(MainSRV, MainFrameSize);

			if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
			{
				ImVec2 delta = ImGui::GetIO().MouseDelta;
				ImGui::SetScrollX(ImGui::GetScrollX() - delta.x);
				ImGui::SetScrollY(ImGui::GetScrollY() - delta.y);
			}
		}
		else {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "No feed available, select an image or use live mode !");
		}


		ImGui::EndChild();
		
		ImGui::SameLine();

		ImGui::BeginChild("SubFeedPanel", ImVec2(LIVE_PANEL_WIDTH, LIVE_PANEL_HEIGHT), true, ImGuiWindowFlags_NoScrollbar);


		if (SubSRV) {
			ImGui::Image(SubSRV, SubFrameSize);

			if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
			{
				ImVec2 delta = ImGui::GetIO().MouseDelta;
				ImGui::SetScrollX(ImGui::GetScrollX() - delta.x);
				ImGui::SetScrollY(ImGui::GetScrollY() - delta.y);
			}
		}
		else {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "No Enrollments Available !");
		}


		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("Control Panel");
		
		if (ImGui::Button("Enroll", ImVec2(150, 30))) App->Enroll();

		if (ImGui::Button("Toggle Matching", ImVec2(150, 30))) App->ToggleMatching();

		if (ImGui::Button("Reset")) {
			App->ClearVerification();
		}

		if (App->verification)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
			ImGui::Text("<< Verified User >>");

			ImGui::Text("Name : zischl ");

			ImGui::Text("User ID : 20232645");

			ImGui::PopStyleColor();
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			ImGui::Text("<< Not Verified >>");
			ImGui::PopStyleColor();
		}

		ImGui::EndChild();


	}

	inline void DrawDBPanel() {
		static bool useLocalDB = true;
		ImGui::Checkbox("Use Local DB", &useLocalDB);
		if (ImGui::Button("Connect")) {

		}
	}

	inline void DrawSettingsPanel()
	{
		ImGui::SetCursorPosY(60);

		ImGui::BeginChild("TopPanel", ImVec2(0, 150), false);

		float Width = ImGui::GetWindowContentRegionMax().x;

		ImGui::SetCursorPosX((Width - 400.0f) / 2.0f);
		ImGui::Image(LogoSRV, ImVec2(100, 128));
		
		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();

		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		
		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::Text("ZScanner v1.0");
		ImGui::TextColored(
			ScannerState ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
			"Status: %s",
			ScannerState ? "Online" : "Offline"
		);
		ImGui::Text("Stream: %s", LiveFeedState ? "Active" : "Inactive");
		ImGui::EndGroup();


		ImGui::EndChild();

		ImGui::Spacing();

		ImGui::Spacing();
		ImGui::SameLine();


		ImGui::BeginChild("SSHPanel", ImVec2(Width/2, 0), false);
		ImGui::Text("SSH Connection");
		ImGui::Separator();

		ImGui::InputText("IP", SSH_IP, 64);
		ImGui::InputText("PORT", SSH_PORT, 64);
		ImGui::InputText("Username", SSH_Username, 64);
		ImGui::InputText("SSH Key Path", SSH_KeyPath, 256);
		ImGui::InputText("Passphrase", SSH_Passphrase, 128, ImGuiInputTextFlags_Password);

		if (ImGui::Button(ScannerState ? "Scanner Log In" : "Scan Network", ImVec2(-1, 0))) {
			int port = 0;
			std::from_chars(SSH_PORT, SSH_PORT + strlen(SSH_PORT), port);
			ScannerState = App->CheckScannerStatus();
		}

		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();

		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();


		ImGui::BeginChild("AdvancedSettingsPanel", ImVec2(0, 0), false);

		ImGui::Text("Streaming Settings");
		ImGui::Separator();
		ImGui::InputInt("Width", &StreamParams.width);
		ImGui::InputInt("Height", &StreamParams.height);

		const char* bits[] = { "8-bit", "10-bit" };
		ImGui::Combo("Bit Depth", &StreamParams.bitDepth, bits, IM_ARRAYSIZE(bits));

		const char* modes[] = { "Default Capture", "GStreamer" };
		ImGui::Combo("Capture Mode", &StreamParams.captureMode, modes, IM_ARRAYSIZE(modes));

		ImGui::InputInt("FPS Limit", &StreamParams.fpsLimit);
		ImGui::InputInt("Render FPS Limit", &StreamParams.renderFpsLimit);


		ImGui::Spacing();
		ImGui::Separator();

		ImGui::Text("Camera Controls");
		ImGui::Separator();
		ImGui::InputInt("Shutter Speed (us)", &CameraParams.shutterSpeed);
		ImGui::InputInt("ISO", &CameraParams.iso);

		const char* awbOptions[] = { "Auto", "Sun", "Cloud", "Shade", "Tungsten", "Fluorescent", "Incandescent" };
		ImGui::Combo("AWB Mode", &CameraParams.awbMode, awbOptions, IM_ARRAYSIZE(awbOptions));

		const char* rotationOptions[] = { "0", "90", "180", "270" };
		ImGui::Combo("Rotation", &CameraParams.rotation, rotationOptions, IM_ARRAYSIZE(rotationOptions));

		ImGui::Checkbox("Horizontal Flip", &CameraParams.hFlip);
		ImGui::Checkbox("Vertical Flip", &CameraParams.vFlip);

		ImGui::InputInt("Timeout (ms)", &CameraParams.timeout);

		ImGui::Spacing();
		ImGui::Separator();

		ImGui::Text("Image Paths");
		ImGui::InputText("Export Path", ImageExportPathBuffer, 256);
		ImGui::InputText("Import Path", ImagePathBuffer, 256);

		ImGui::EndChild();
	}

	inline void FrameBegin(ID3D11ShaderResourceView* MainFeedSRV, const ImVec2& MainFeedSize, ID3D11ShaderResourceView* SubSRV, const ImVec2& SubFeedSize, CVParams& MaskParams) {
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(1920, 1080));
		ImGui::SetNextWindowPos(ImVec2(0, 0));

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 15.0f;

		if (ImGui::Begin("ZScanner", &ImGuiState, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize)) {

			ImGui::SetWindowPos(ImVec2(0, 0));
			ImGui::SetWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y));


			auto DrawMenuItem = [&](MenuIndex menu, const char* label) {
				bool selected = (CurrentMenu == menu);

				ImGui::PushFont(IconFont);
				if (ImGui::Button(label, ImVec2(80, 80)))
				{
					CurrentMenu = menu;
					App->ModeSwitch(menu);
					
				};
				ImGui::PopFont();
				};
			

			ImGui::SetCursorPosX(2.0f);

			ImGui::BeginGroup();

			ImGui::SetCursorPosX(20.8f);
			ImGui::Image((void*)LogoSRV, ImVec2(41.6f, 53.2f));
			ImGui::Spacing();

			ImGui::SetCursorPosY(250.0f);
			DrawMenuItem(MenuIndex::Dashboard, ICON_HOME);
			DrawMenuItem(MenuIndex::LiveFeed, ICON_VIDEO);
			DrawMenuItem(MenuIndex::ImageTest, ICON_IMAGE);
			DrawMenuItem(MenuIndex::Database, ICON_DATABASE);
			DrawMenuItem(MenuIndex::Settings, ICON_COG);
			ImGui::EndGroup();

			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();

			ImGui::BeginGroup();
			switch (CurrentMenu) {
			case MenuIndex::Dashboard:
				Dashboard();
				break;
			case MenuIndex::LiveFeed:
				LiveFeedPanel(MainFeedSRV, MainFeedSize);
				break;
			case MenuIndex::ImageTest:
				DrawImageTestPanel(MainFeedSRV, MainFeedSize, SubSRV, SubFeedSize, MaskParams);
				break;
			case MenuIndex::Database:
				//DrawDBPanel();
				break;
			case MenuIndex::Settings:
				DrawSettingsPanel();
				break;
			}


			ImGui::EndGroup();

		}


		ImGui::End();

	}

	static inline int ClampKernel(int k) {
		k = std::max(k, 3);
		if ((k % 2) == 0) k += 1;
		return k;
	}

	static inline void ClampNonNegative(float& v) {
		if (v < 0.0f) v = 0.0f;
	}

	static bool ReadStringVector(void* data, int idx, const char** out_text)
	{
		auto* vec = static_cast<std::vector<std::string>*>(data);

		if (idx < 0 || idx >= vec->size())
			return false;

		*out_text = (*vec)[idx].c_str();
		return true;
	}

	inline void Render() {
		// Rendering
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


	}



private:
	HANDLE* EventHandler = nullptr;
	ZScan* App;

	MenuIndex CurrentMenu = MenuIndex::Dashboard;
	ImFont* IconFont;

	bool ImGuiState = true;

	ImDrawList* DrawList = nullptr;

	int ActiveMenu = 0;
	//bool PopUp1 = false;

	bool IconizedButton(const char* label, ImVec2& ButtonSize);

	bool VerticalMenuItem(const char* label);

	void CenterItemX(const float ItemWidth);

	bool DirScanCombo(const char* ID, std::vector<std::string>& Buffer, char ActiveBuffer[256], int ActiveBufferLen, int& SelectedImage);

	inline void UpdateConsole(const std::string& msg) {
		snprintf(Console, sizeof(Console), "%s", msg.c_str());
	}

	bool ModuleMenu(CVParams& Parameters);


	//Fonts
	ImFont* JetBrainsReg20 = nullptr;
	ImFont* JetBrainsReg18 = nullptr;

	ID3D11Texture2D* LogoTex = nullptr;
	ID3D11ShaderResourceView* LogoSRV = nullptr;

	char Console[256] = "";

	StreamConfig StreamParams{};
	CameraConfig CameraParams{};

	bool ScannerState = false;
	bool LiveFeedState = false;
	bool AdminState = false;
	StreamMode ActiveStreamMode = StreamMode::TCP;
	char StreamProtocolTCP[64] = "tcp://192.168.1.228";
	char StreamProtocolRTSP[64] = "rtsp://";
	int StreamTCPPort = 8888;
	int StreamRTSPPort = 8554;

	char SSH_IP[64] = "192.168.43.191";
	char SSH_PORT[8] = "22";
	char SSH_Username[64] = "zischl";
	char SSH_KeyPath[256] = "D:\\ZPi";
	char SSH_Passphrase[128] = "zischl@96";

	char ImageExportPathBuffer[256] = "";
	char ImagePathBuffer[256] = R"(D:\Workspace\Repositories\ZScanner\ZScanner-Tests\cpp\Images)";
	int SelectedImage = 0;

	ImVec4 ActiveColor = ImVec4(0.215f, 0.207f, 0.243f, 1.000f);
	ImVec4 InactiveColor = ImVec4(0.266f, 0.266f, 0.305f, 1.000f);


};

#endif