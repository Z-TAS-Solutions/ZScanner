#ifndef OMNIGUI_H
#define OMNIGUI_H

#pragma once

#include "ZScanCore.h"

#include <Windows.h>
#include <wrl/client.h>

#include <array>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"

#include <opencv2/opencv.hpp>

class ZScan;

enum class MenuIndex {
	Dashboard,
	LiveFeed,
	ImageTest,
	Database,
	Settings
};

#define ICON_HOME reinterpret_cast<const char*>(u8"\uE900")
#define ICON_VIDEO reinterpret_cast<const char*>(u8"\uE903")
#define ICON_IMAGE reinterpret_cast<const char*>(u8"\uE902")
#define ICON_COG reinterpret_cast<const char*>(u8"\uE901")
#define ICON_DATABASE reinterpret_cast<const char*>(u8"\uE904")


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
			if (ImGui::Button("Connect", ImVec2(-1, 0))) {
				App->OpenStream(StreamProtocolTCP, StreamTCPPort, ActiveStreamMode);
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
			if (ImGui::Button("Connect", ImVec2(-1, 0))) {
				App->OpenStream(StreamProtocolRTSP, StreamRTSPPort, ActiveStreamMode);
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
		ImGui::InputText("Username", SSH_Username, 64);
		ImGui::InputText("SSH Key Path", SSH_KeyPath, 256);
		ImGui::InputText("Passphrase", SSH_Passphrase, 128, ImGuiInputTextFlags_Password);

		if (ImGui::Button(ScannerState ? "Shutdown Scanner" : "Start Scanner", ImVec2(-1, 0))) {
			ScannerState = !ScannerState;
		}

		ImGui::EndChild();
	}

	inline void DrawImageTestPanel(ID3D11ShaderResourceView* SRV, cv::Mat FrameMat, ID3D11ShaderResourceView* OutSRV, cv::Mat OutFrameMat, CVParams& MaskParams) {


		ImGui::Checkbox("Live Mode", &(App->LiveCapture));

		ImGui::Text("General Settings");

		ImGui::Separator();
		
		ImGui::SliderInt("Clahe Clip Limit", &MaskParams.claheClipLimit, 0, 10);
		if (ImGui::SliderInt("Adaptive Threshold", &MaskParams.adaptiveThreshold, 0, 255))
			MaskParams.adaptiveThreshold = ClampKernel(MaskParams.adaptiveThreshold);
		ImGui::SliderInt("Morph Kernel", &MaskParams.morphKernel, 1, 21);

		ImGui::Separator();

		ImGui::Text("Blur Settings");
		ImGui::Checkbox("Median", &MaskParams.useMedian); ImGui::SameLine();
		ImGui::Checkbox("Bilateral", &MaskParams.useBilateral); ImGui::SameLine();
		ImGui::Checkbox("Gaussian", &MaskParams.useGaussian);

		ImGui::Separator();

		if (MaskParams.useMedian) {
			ImGui::BeginChild("MedianPanel", ImVec2(0, 80), true);
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Median Settings");
			ImGui::SliderInt("Kernel (odd)", &MaskParams.medianK, 3, 15);
			MaskParams.medianK = ClampKernel(MaskParams.medianK);
			ImGui::EndChild();
		}

		if (MaskParams.useBilateral) {
			ImGui::BeginChild("BilateralPanel", ImVec2(0, 120), true);
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Bilateral Settings");
			ImGui::SliderInt("Diameter d", &MaskParams.bilateralD, 1, 25);
			ImGui::SliderFloat("Sigma Color", &MaskParams.sigmaColor, 1.0f, 200.0f);
			ImGui::SliderFloat("Sigma Space", &MaskParams.sigmaSpace, 1.0f, 200.0f);
			ClampNonNegative(MaskParams.sigmaColor);
			ClampNonNegative(MaskParams.sigmaSpace);
			ImGui::EndChild();
		}

		if (MaskParams.useGaussian) {
			ImGui::BeginChild("GaussianPanel", ImVec2(0, 140), true);
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Gaussian Settings");
			ImGui::SliderInt("Kernel (odd)", &MaskParams.gaussK, 3, 31);
			MaskParams.gaussK = ClampKernel(MaskParams.gaussK);
			ImGui::SliderFloat("SigmaX", &MaskParams.sigmaX, 0.0f, 10.0f);
			ImGui::SliderFloat("SigmaY", &MaskParams.sigmaY, 0.0f, 10.0f);
			ClampNonNegative(MaskParams.sigmaX);
			ClampNonNegative(MaskParams.sigmaY);
			ImGui::EndChild();
		}

		ImGui::Separator();

		if (ImGui::Button("Apply Config", ImVec2(150, 30))) App->SetRedraw();
		ImGui::SameLine();
		if (ImGui::Button("Toggle Matching", ImVec2(150, 30))) App->ToggleMatching();
		ImGui::SameLine();
		if (ImGui::Button("Enroll", ImVec2(150, 30))) App->Enroll();

		ImGui::Spacing();
		ImGui::Text("Input Feed");
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.5f, 1.0f, 0.8f));
		
		if (ImGui::Combo(
			"Directory",
			&selected_item,
			ReadStringVector,
			&App->Directories,
			App->Directories.size()))
		{
			const std::string& current_item = App->Directories[selected_item];
			App->UpdateInput(current_item);
		}

		ImGui::Image((void*)SRV, ImVec2(FrameMat.cols, FrameMat.rows));
		
		ImGui::PopStyleColor();

		ImGui::SameLine();

		ImVec4 verColor = App->verification ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		ImGui::TextColored(verColor, App->verification ? "Verified" : "Not Verified");

		ImGui::SameLine();

		ImGui::Image((void*)SRV, ImVec2(FrameMat.cols, FrameMat.rows));
		
		ImGui::SameLine();

		if (App->verification)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
			ImGui::Text("||| Verified User - >>");
			ImGui::SameLine();

			ImGui::Text("Name : zischl  |||  ");
			ImGui::SameLine();

			ImGui::Text("User ID : 20232645  |||");
			ImGui::SameLine();

			ImGui::PopStyleColor();
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			ImGui::Text("Not Verified");
			ImGui::PopStyleColor();
		}

		ImGui::SameLine();

		ImGui::Image((void*)OutSRV, ImVec2(OutFrameMat.cols, OutFrameMat.rows));

		if (ImGui::Button("Reset Verification")) {
			App->ClearVerification();
		}

	}

	inline void DrawDBPanel() {
		static bool useLocalDB = true;
		ImGui::Checkbox("Use Local DB", &useLocalDB);
		if (ImGui::Button("Connect")) {

		}
	}

	inline void DrawSettingsPanel() {
		static bool darkMode = true;
		ImGui::Checkbox("Dark Mode", &darkMode);
	}

	inline void FrameBegin(ID3D11ShaderResourceView* SRV, cv::Mat FrameMat, ID3D11ShaderResourceView* OutSRV, cv::Mat OutFrameMat, CVParams& MaskParams) {
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
				ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4(0.0f, 0.7f, 1.0f, 1.0f) : ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.5f, 1.0f, 0.6f));
				ImGui::PushFont(IconFont);
				if (ImGui::Button(label, ImVec2(80, 50))) CurrentMenu = menu;
				ImGui::PopFont();
				ImGui::PopStyleColor(2);
				};
			
			ImGui::BeginGroup();

			ImGui::Dummy(ImVec2(0, 20));
			ImGui::Text("ZZZZ");
			ImGui::Dummy(ImVec2(0, 20));


			DrawMenuItem(MenuIndex::Dashboard, ICON_HOME);
			DrawMenuItem(MenuIndex::LiveFeed, ICON_VIDEO);
			DrawMenuItem(MenuIndex::ImageTest, ICON_IMAGE);
			DrawMenuItem(MenuIndex::Database, ICON_DATABASE);
			DrawMenuItem(MenuIndex::Settings, ICON_COG);
			ImGui::EndGroup();

			ImGui::SameLine();

			ImGui::BeginGroup();
			switch (CurrentMenu) {
			case MenuIndex::Dashboard:
				Dashboard();
				break;
			case MenuIndex::LiveFeed:
				//DrawLiveFeedPanel();
				break;
			case MenuIndex::ImageTest:
				DrawImageTestPanel(SRV, FrameMat, OutSRV, OutFrameMat, MaskParams);
				break;
			case MenuIndex::Database:
				//DrawDBPanel();
				break;
			case MenuIndex::Settings:
				//DrawSettingsPanel();
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

	int selected_item = 0;

	ImDrawList* DrawList = nullptr;

	int ActiveMenu = 0;
	//bool PopUp1 = false;

	bool IconizedButton(const char* label, ImVec2& ButtonSize);
	bool VerticalMenuItem(const char* label);


	void CenterItemX(const float ItemWidth);


	//Fonts
	ImFont* JetBrainsReg20 = nullptr;
	ImFont* JetBrainsReg18 = nullptr;


	bool ScannerState = false;
	StreamMode ActiveStreamMode = StreamMode::TCP;
	char StreamProtocolTCP[64] = "tcp://";
	char StreamProtocolRTSP[64] = "rtsp://";
	int StreamTCPPort = 8888;
	int StreamRTSPPort = 8554;

	char SSH_IP[64] = "";
	char SSH_Username[64] = "";
	char SSH_KeyPath[256] = "";
	char SSH_Passphrase[128] = "";

	ImVec4 ActiveColor = ImVec4(0.215f, 0.207f, 0.243f, 1.000f);
	ImVec4 InactiveColor = ImVec4(0.266f, 0.266f, 0.305f, 1.000f);


};

#endif