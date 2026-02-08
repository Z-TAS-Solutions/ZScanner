#ifndef OMNIGUI_H
#define OMNIGUI_H

#pragma once


#include <Windows.h>
#include <wrl/client.h>

#include <array>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"

#include <opencv2/opencv.hpp>

class ZScan;
struct CVParams {
	int threshold = 70;
	int morphKernel = 7;
	float minDefectDepthRatio = 0.05f;
	int claheClipLimit = 0;
	cv::Size GridLimit = {16, 16};
};

class ZScanGUI {
public:
	ZScanGUI(ZScan& OmniLinkInstance);

	void SetupImGui(HWND hwnd, ID3D11Device* D3D11Device, ID3D11DeviceContext* D3D11Context, HANDLE* Events);

	inline void FrameBegin(ID3D11ShaderResourceView* SRV, cv::Mat FrameMat, CVParams& MaskParams) {
		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(1920, 1080));
		ImGui::SetNextWindowPos(ImVec2(0, 0));

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 15.0f;

		if (ImGui::Begin("ZScanner", &ImGuiState, 0)) {

			if (ImGui::SliderInt("ClaheClipLimit", &MaskParams.claheClipLimit, 0, 10)) {
				//App.UpdateClahe();
			
			}

			ImGui::SliderInt("Threshold", &MaskParams.threshold, 0, 255);
			ImGui::SliderInt("Morph kernel", &MaskParams.morphKernel, 1, 21);
			//ImGui::SliderFloat("Defect depth ratio", &MaskParams.minDefectDepthRatio, 0.01f, 0.2f);

			ImGui::Image((void*)SRV, ImVec2(FrameMat.cols, FrameMat.rows));
		}


		ImGui::End();

	}

	inline void Render() {
		// Rendering
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


	}



private:
	HANDLE* EventHandler = nullptr;
	ZScan& App;


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