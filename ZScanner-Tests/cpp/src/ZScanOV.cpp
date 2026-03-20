#include "ZScanCore.h"
#include "ZScanOV.h"
#include "fonts.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using Microsoft::WRL::ComPtr;


ZScanGUI::ZScanGUI(ZScan& OmniLinkInstance) : App(&OmniLinkInstance) {

}

void ZScanGUI::SetupImGui(HWND hwnd, ID3D11Device* D3D11Device, ID3D11DeviceContext* D3D11Context, HANDLE* Events)
{

	EventHandler = Events;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(D3D11Device, D3D11Context);

	ImFontConfig FontCFG;
	FontCFG.FontDataOwnedByAtlas = false;

	JetBrainsReg18 = io.Fonts->AddFontFromMemoryTTF(JetBrainsMonoRegular, JetBrainsMonoRegular_Size, 18.0f, &FontCFG);
	JetBrainsReg20 = io.Fonts->AddFontFromMemoryTTF(JetBrainsMonoRegular, JetBrainsMonoRegular_Size, 20.0f, &FontCFG);

	static const ImWchar icon_ranges[] = { 0xE900, 0xE900, 0 };

	IconFont = io.Fonts->AddFontFromMemoryTTF(
		IconFontHome, IconFontHome_len, 32.0f, nullptr,
		icon_ranges);

	IM_ASSERT(IconFont != nullptr);
	io.Fonts->Build();

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 12.0f;
	style.FrameRounding = 6.0f;
	style.GrabRounding = 6.0f;
	style.WindowPadding = ImVec2(15, 15);
	style.FramePadding = ImVec2(6, 4);

	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.08f, 1.0f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.1f, 0.15f, 1.0f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.3f, 0.5f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.215f, 0.207f, 0.243f, 1.000f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.266f, 0.266f, 0.305f, 1.000f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.0f, 0.7f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_Text] = ImVec4(0.8f, 0.8f, 0.85f, 1.0f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.0f, 0.4f, 0.7f, 0.5f);


	int width, height, channels;
	unsigned char* data = stbi_load("ZTAS.png", &width, &height, &channels, 4);
	if (!data) {
		Logger::log("Failed to load PNG");
		return;
	}


	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = data;
	initData.SysMemPitch = width * 4;

	ID3D11Texture2D* tex = nullptr;
	HRESULT hr = D3D11Device->CreateTexture2D(&desc, &initData, &tex);
	stbi_image_free(data);
	if (FAILED(hr)) { Logger::log("Failed to create texture"); return; }



	hr = D3D11Device->CreateShaderResourceView(tex, nullptr, &LogoSRV);
	tex->Release();
	if (FAILED(hr)) { Logger::log("Failed to create SRV"); return; }


}


bool ZScanGUI::VerticalMenuItem(const char* label)
{
	ImGui::PushID(label);

	ImVec2 MenuItemSize = ImVec2(180, 100);

	ImVec2 pos = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::InvisibleButton(label, MenuItemSize);


	if (!ImGui::IsItemHovered()) {
		ImU32 MenuItemColor = ImGui::GetColorU32(ImVec4(0.09f, 0.09f, 0.09f, 1.0f));

		DrawList->AddRectFilled(pos, ImVec2(pos.x + MenuItemSize.x, pos.y + MenuItemSize.y), MenuItemColor);
	}
	else {
		ImU32 MenuItemColor_Hovered = ImGui::GetColorU32(ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

		DrawList->AddRectFilled(pos, ImVec2(pos.x + MenuItemSize.x, pos.y + MenuItemSize.y), MenuItemColor_Hovered);
	}

	ImGui::PopID();

	return clicked;

}


bool ZScanGUI::ModuleMenu(CVParams& Parameters)
{
	static std::vector<FilterTypes> ActiveFilters;
	static const char* Filters[] = {
	"CLAHE",
	"Median Blur",
	"Bilateral Blur",
	"Gaussian Blur",
	"Threshold",
	"Morphology",
	"Skeletonize",
	"Sharpen"
	};
	static int SelectedFilterIndex = 0;
	static int ActiveFilterIndex = -1;


	ImGui::Text("Add Filter:");
	ImGui::SameLine();
	ImGui::PushItemWidth(150);
	ImGui::Combo("##FilterList", &SelectedFilterIndex, Filters, IM_ARRAYSIZE(Filters));
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if (ImGui::Button("+")) {
		ActiveFilters.push_back((FilterTypes)SelectedFilterIndex);
		Parameters.FilterOrder = ActiveFilters;
	}

	ImGui::Separator();

	for (int i = 0; i < ActiveFilters.size(); ++i) {

		ImGui::PushID(i);
		if (ImGui::Button("/\\") && i > 0) {
			std::swap(ActiveFilters[i], ActiveFilters[i - 1]);
			Parameters.FilterOrder = ActiveFilters;

			ImGui::PopID();
			return true;
		}
		ImGui::SameLine();
		if (ImGui::Button("\\/") && i < ActiveFilters.size() - 1) {
			std::swap(ActiveFilters[i], ActiveFilters[i + 1]);
			Parameters.FilterOrder = ActiveFilters;

			ImGui::PopID();
			return true;
		}

		ImGui::SameLine();
		if (ImGui::Button("-")) {
			ActiveFilters.erase(ActiveFilters.begin() + i);
			Parameters.FilterOrder = ActiveFilters;

			if (ActiveFilterIndex >= ActiveFilters.size())
				ActiveFilterIndex = (ActiveFilters.empty() ? -1 : (int)ActiveFilters.size() - 1);

			ImGui::PopID();
			return true;

		}



		ImGui::SameLine();

		std::string Item = Filters[ActiveFilters[i]];
		Item += "##filter" + std::to_string(i);

		if (ImGui::Selectable(Item.c_str())) {
			ActiveFilterIndex = i;
		}

		if (ActiveFilterIndex == i)
		{
			switch (ActiveFilters[i])
			{
			case FilterTypes::CLAHE:
			{
				ImGui::Separator();

				ImGui::SliderInt("Clahe Clip Limit", &Parameters.claheClipLimit, 0, 10);
				if (ImGui::SliderInt("Adaptive Threshold", &Parameters.adaptiveThreshold, 0, 255))
					Parameters.adaptiveThreshold = ClampKernel(Parameters.adaptiveThreshold);
				ImGui::SliderInt("Morph Kernel", &Parameters.morphKernel, 1, 21);

				ImGui::Separator();

				break;
			}

			case FilterTypes::MedianBlur:
			{
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Median Settings");

				ImGui::SliderInt("Kernel (odd)", &Parameters.medianK, 3, 15);
				Parameters.medianK = ClampKernel(Parameters.medianK);

				ImGui::Separator();

				break;
			}
			case FilterTypes::BilateralBlur:
			{
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Bilateral Settings");

				ImGui::SliderInt("Diameter d", &Parameters.bilateralD, 1, 25);
				ImGui::SliderFloat("Sigma Color", &Parameters.sigmaColor, 1.0f, 200.0f);
				ImGui::SliderFloat("Sigma Space", &Parameters.sigmaSpace, 1.0f, 200.0f);

				ClampNonNegative(Parameters.sigmaColor);
				ClampNonNegative(Parameters.sigmaSpace);

				ImGui::Separator();

				break;
			}
			case FilterTypes::GaussianBlur:
			{
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Gaussian Settings");

				ImGui::SliderInt("Kernel (odd)", &Parameters.gaussK, 3, 31);
				Parameters.gaussK = ClampKernel(Parameters.gaussK);

				ImGui::SliderFloat("SigmaX", &Parameters.sigmaX, 0.0f, 10.0f);
				ImGui::SliderFloat("SigmaY", &Parameters.sigmaY, 0.0f, 10.0f);

				ClampNonNegative(Parameters.sigmaX);
				ClampNonNegative(Parameters.sigmaY);

				ImGui::Separator();

				break;
			}
			case FilterTypes::Threshold:
			{
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Thresholding Settings");
				ImGui::Separator();

				const char* ThreshModes[] = { "Global", "Otsu", "Adaptive Mean", "Adaptive Gaussian" };
				int ActiveThreshMode = (int)Parameters.ThresholdType;

				if (ImGui::Combo("Method", &ActiveThreshMode, ThreshModes, IM_ARRAYSIZE(ThreshModes))) {
					Parameters.ThresholdType = (ThresholdType)ActiveThreshMode;
				}

				if (Parameters.ThresholdType == ThresholdType::Global) {
					ImGui::SliderFloat("Threshold Value", &Parameters.GlobalThreshold, 0.0f, 255.0f, "%.0f");
				}

				ImGui::SliderFloat("Max Binary Value", &Parameters.MaxBinaryValue, 0.0f, 255.0f, "%.0f");


				if (Parameters.ThresholdType > ThresholdType::Otsu) {
					if (ImGui::SliderInt("Block Size", &Parameters.AdaptiveBlockSize, 3, 99)) {
						if (Parameters.AdaptiveBlockSize % 2 == 0) Parameters.AdaptiveBlockSize++;
					}
					ImGui::SliderFloat("Constant (C)", &Parameters.AdaptiveC, -10.0f, 30.0f, "%.1f");
				}

				ImGui::Separator();
				break;
			}

			case FilterTypes::Morphology:
			{
				const char* shapes[] = { "Rectangle", "Cross", "Ellipse" };
				int currentShape = (int)Parameters.MorphShape;
				if (ImGui::Combo("Kernel Shape", &currentShape, shapes, IM_ARRAYSIZE(shapes))) {
					Parameters.MorphShape = (MorphType)currentShape;
				}
				ImGui::SliderInt("Kernel Size", &Parameters.MorphKernelSize, 1, 15);
				ImGui::SliderInt("Iterations", &Parameters.MorphIterations, 1, 10);
				break;

			}
			
			case FilterTypes::Skeletonize:
			{
				ImGui::SliderInt("Pruning (Spurs)", &Parameters.PruningIterations, 0, 50);
				break;
			}
			case FilterTypes::Sharpen:
			{
				const char* ShapenModes[] = { "Basic Kernel", "Unsharp Mask", "Laplacian", "Frangi (Vesselness)" };
				int ActiveSMode = (int) Parameters.SharpenType;

				if (ImGui::Combo("Enhance Method", &ActiveSMode, ShapenModes, IM_ARRAYSIZE(ShapenModes))) {
					Parameters.SharpenType = (SharpenTypes)ActiveSMode;
				}

				ImGui::Separator();

				switch (Parameters.SharpenType) {
				case SharpenTypes::SharpenKernel:
				{
					ImGui::SliderFloat("Kernel Strength", &Parameters.KernelStrength, 0.1f, 5.0f);
					break;
				}

				case SharpenTypes::SharpenUnsharp:
				{
					ImGui::SliderFloat("Sigma (Blur)", &Parameters.UnsharpSigma, 0.1f, 10.0f);
					ImGui::SliderFloat("Amount", &Parameters.UnsharpAmount, 0.1f, 5.0f);
					break;
				}

				case SharpenTypes::SharpenLaplacian:
				{
					if (ImGui::SliderInt("K-Size", &Parameters.LaplacianKSize, 1, 7)) {
						if (Parameters.LaplacianKSize % 2 == 0) Parameters.LaplacianKSize++;
					}
					ImGui::SliderFloat("Scale", &Parameters.LaplacianScale, 0.1f, 5.0f);
					break;
				}

				case SharpenTypes::Frangi:
				{
					ImGui::TextColored(ImVec4(1, 1, 0, 1), "Vessel Detection Mode");
					ImGui::SliderInt("Ridge K-Size", &Parameters.RidgeKSize, 1, 7);
					ImGui::SliderFloat("Ridge Scale", &Parameters.RidgeScale, 0.1f, 10.0f);
					ImGui::SliderFloat("Alpha (Noise)", &Parameters.RidgeAlpha, 0.01f, 1.0f);
					ImGui::SliderFloat("Beta (Blob)", &Parameters.RidgeBeta, 0.01f, 1.0f);
					break;
				}

					break;
				}
			}
			}

			
		}

		ImGui::PopID();
	}



	return false;
}

bool ZScanGUI::IconizedButton(const char* label, ImVec2& ButtonSize)
{
	ImGui::PushID(label);




	ImVec2 pos = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::InvisibleButton(label, ButtonSize);

	ImVec2 TextSize = ImGui::CalcTextSize(label);
	ImVec2 TextPos = ImVec2(pos.x + (ButtonSize.x - TextSize.x) * 0.5f, pos.y + (ButtonSize.y - TextSize.y) * 0.5f);


	if (!ImGui::IsItemHovered()) {
		ImU32 ButtonColor = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImU32 TextColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

		DrawList->AddRectFilled(pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), ButtonColor);
		DrawList->AddText(TextPos, TextColor, label);
	}
	else {
		ImU32 ButtonColor_Hovered = ImGui::GetColorU32(ImVec4(0.2f, 0.0f, 0.2f, 1.0f));
		ImU32 TextColor_Hovered = ImGui::GetColorU32(ImVec4(0.2f, 0.0f, 0.2f, 1.0f));

		DrawList->AddRectFilled(pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), ButtonColor_Hovered);
		DrawList->AddText(TextPos, TextColor_Hovered, label);
	}

	ImGui::PopID();

	return clicked;



}



void ZScanGUI::CenterItemX(const float ItemWidth)
{
	float space = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX((space - ItemWidth) * 0.5f);
}


bool ZScanGUI::DirScanCombo(const char* ID, std::vector<std::string>& Buffer, char ActiveBuffer[256], int ActiveBufferLen, int& SelectedImage)
{
	ImGui::PushID(ID);

	float arrowWidth = ImGui::GetFrameHeight();

	ImGui::InputText("##input", ActiveBuffer, 256, ImGuiInputTextFlags_EnterReturnsTrue);

	ImGui::SameLine();
	if (ImGui::ArrowButton("##arrow", ImGuiDir_Down))
	{
		App->ScanDirectory(ActiveBuffer);
		ImGui::OpenPopup("popup_dir");
	}


	bool ReturnState = false;

	if (ImGui::BeginPopup("popup_dir"))
	{
		for (int Index = 0; Index < (int)Buffer.size(); Index++)
		{
			const std::string& item = Buffer[Index];

			bool IsSelected = false;

			std::string label = item + "##" + std::to_string(Index);

			if (ImGui::Selectable(label.c_str(), IsSelected))
			{
				SelectedImage = Index;
				ReturnState = true;
			}

			if (IsSelected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndPopup();

	}


	ImGui::PopID();
	return ReturnState;
}