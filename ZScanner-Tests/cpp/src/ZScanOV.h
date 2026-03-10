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

	inline void DrawImageTestPanel(ID3D11ShaderResourceView* SRV, cv::Mat FrameMat, CVParams& MaskParams) {

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
		ImGui::Text("Live Frame");
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.5f, 1.0f, 0.8f));
		ImGui::Image((void*)SRV, ImVec2(FrameMat.cols, FrameMat.rows));
		ImGui::PopStyleColor();

		ImVec4 verColor = App->verification ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		ImGui::TextColored(verColor, App->verification ? "Verified" : "Not Verified");

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

	inline void FrameBegin(ID3D11ShaderResourceView* SRV, cv::Mat FrameMat, CVParams& MaskParams) {
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
			case MenuIndex::LiveFeed:
				//DrawLiveFeedPanel();
				break;
			case MenuIndex::ImageTest:
				DrawImageTestPanel(SRV, FrameMat, MaskParams);
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
	bool DeviceHoverState = false;
	ImVec2 SelectedDevicePos;

	int user_resx = GetSystemMetrics(SM_CXSCREEN);
	int user_resy = GetSystemMetrics(SM_CYSCREEN);

	ImDrawList* DrawList = nullptr;

	int ActiveMenu = 0;
	//bool PopUp1 = false;

	bool IconizedButton(const char* label, ImVec2& ButtonSize);
	bool VerticalMenuItem(const char* label);


	void CenterItemX(const float ItemWidth);


	//Fonts
	ImFont* JetBrainsReg20 = nullptr;
	ImFont* JetBrainsReg18 = nullptr;


};

#endif