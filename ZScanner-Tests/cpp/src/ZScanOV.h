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



class ZScanGUI {
public:
	ZScanGUI(ZScan& OmniLinkInstance);

	void SetupImGui(HWND hwnd, ID3D11Device* D3D11Device, ID3D11DeviceContext* D3D11Context, HANDLE* Events);

	inline void FrameBegin(ID3D11ShaderResourceView* SRV, cv::Mat FrameMat, ID3D11ShaderResourceView* OutSRV, cv::Mat OutFrameMat, CVParams& MaskParams) {
		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(1920, 1080));
		ImGui::SetNextWindowPos(ImVec2(0, 0));

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 15.0f;

		if (ImGui::Begin("ZScanner", &ImGuiState, 0)) {

			ImGui::Checkbox("Live Mode", &(App->LiveCapture));

			ImGui::SliderInt("ClaheClipLimit", &MaskParams.claheClipLimit, 0, 10);

			if (ImGui::SliderInt("Adaptive Threshold", &MaskParams.adaptiveThreshold, 0, 255)) {
				MaskParams.adaptiveThreshold = ClampKernel(MaskParams.adaptiveThreshold);
			}
			ImGui::SliderInt("Morph kernel", &MaskParams.morphKernel, 1, 21);
			//ImGui::SliderFloat("Defect depth ratio", &MaskParams.minDefectDepthRatio, 0.01f, 0.2f);


			ImGui::Checkbox("Median blur", &MaskParams.useMedian);
			ImGui::SameLine();
			ImGui::Checkbox("Bilateral", &MaskParams.useBilateral);
			ImGui::SameLine();
			ImGui::Checkbox("Gaussian blur", &MaskParams.useGaussian);

			ImGui::Separator();

			if (MaskParams.useMedian) {
				ImGui::TextUnformatted("Median settings");
				ImGui::Indent();
				ImGui::SliderInt("Kernel (odd)", &MaskParams.medianK, 3, 15);
				MaskParams.medianK = ClampKernel(MaskParams.medianK);
				ImGui::Unindent();
				ImGui::Separator();
			}

			if (MaskParams.useBilateral) {
				ImGui::TextUnformatted("Bilateral settings");
				ImGui::Indent();
				ImGui::SliderInt("Diameter d", &MaskParams.bilateralD, 1, 25);
				ImGui::SliderFloat("SigmaColor", &MaskParams.sigmaColor, 1.0f, 200.0f);
				ImGui::SliderFloat("SigmaSpace", &MaskParams.sigmaSpace, 1.0f, 200.0f);
				ClampNonNegative(MaskParams.sigmaColor);
				ClampNonNegative(MaskParams.sigmaSpace);
				ImGui::Unindent();
				ImGui::Separator();
			}

			if (MaskParams.useGaussian) {
				ImGui::TextUnformatted("Gaussian settings");
				ImGui::Indent();
				ImGui::SliderInt("Kernel (odd)", &MaskParams.gaussK, 3, 31);
				MaskParams.gaussK = ClampKernel(MaskParams.gaussK);

				ImGui::SliderFloat("SigmaX (0=auto)", &MaskParams.sigmaX, 0.0f, 10.0f);
				ImGui::SliderFloat("SigmaY (0=auto)", &MaskParams.sigmaY, 0.0f, 10.0f);
				ClampNonNegative(MaskParams.sigmaX);
				ClampNonNegative(MaskParams.sigmaY);
				ImGui::Unindent();
				ImGui::Separator();
			}


			if (ImGui::Button("Apply Config")) {
				App->SetReconfig();
			}

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

			

			if (ImGui::Button("Toggle Matching")) {
				App->ToggleMatching();
			}
			if (ImGui::Button("Enroll")) {
				App->Enroll();
			}
			if (ImGui::Button("Reset Verification")) {
				App->ClearVerification();
			}

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


};

#endif