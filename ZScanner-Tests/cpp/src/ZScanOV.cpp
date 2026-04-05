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
		FilterNode Node;
		Node.Type = (FilterTypes)SelectedFilterIndex;
		Parameters.Filters.push_back(Node);
	}

	ImGui::Separator();

	for (int i = 0; i < Parameters.Filters.size(); ++i) {

		ImGui::PushID(i);
		if (ImGui::Button("/\\") && i > 0) {
			std::swap(Parameters.Filters[i], Parameters.Filters[i - 1]);

			ImGui::PopID();
			return true;
		}
		ImGui::SameLine();
		if (ImGui::Button("\\/") && i < Parameters.Filters.size() - 1) {
			std::swap(Parameters.Filters[i], Parameters.Filters[i + 1]);

			ImGui::PopID();
			return true;
		}

		ImGui::SameLine();
		if (ImGui::Button("-")) {
			Parameters.Filters.erase(Parameters.Filters.begin() + i);

			if (ActiveFilterIndex >= Parameters.Filters.size())
				ActiveFilterIndex = (Parameters.Filters.empty() ? -1 : (int)Parameters.Filters.size() - 1);

			ImGui::PopID();
			return true;

		}



		ImGui::SameLine();

		std::string Item = Filters[Parameters.Filters[i].Type];
		Item += "##filter" + std::to_string(i);

		if (ImGui::Selectable(Item.c_str())) {
			ActiveFilterIndex = i;
		}

		if (ActiveFilterIndex == i)
		{
			FilterNode& Node = Parameters.Filters[i];
			switch (Node.Type)
			{
			case FilterTypes::CLAHE:
			{
				ImGui::Separator();

				ImGui::SliderFloat("Clahe Clip Limit", &Node.claheClipLimit, 0, 10);
				ImGui::Separator();

				break;
			}

			case FilterTypes::MedianBlur:
			{
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Median Settings");

				ImGui::SliderInt("Kernel (odd)", &Node.medianK, 3, 15);
				Node.medianK = ClampKernel(Node.medianK);

				ImGui::Separator();

				break;
			}
			case FilterTypes::BilateralBlur:
			{
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Bilateral Settings");

				ImGui::SliderInt("Diameter d", &Node.bilateralD, 1, 25);
				ImGui::SliderFloat("Sigma Color", &Node.sigmaColor, 1.0f, 200.0f);
				ImGui::SliderFloat("Sigma Space", &Node.sigmaSpace, 1.0f, 200.0f);

				ClampNonNegative(Node.sigmaColor);
				ClampNonNegative(Node.sigmaSpace);

				ImGui::Separator();

				break;
			}
			case FilterTypes::GaussianBlur:
			{
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Gaussian Settings");

				ImGui::SliderInt("Kernel (odd)", &Node.gaussK, 3, 31);
				Node.gaussK = ClampKernel(Node.gaussK);

				ImGui::SliderFloat("SigmaX", &Node.sigmaX, 0.0f, 10.0f);
				ImGui::SliderFloat("SigmaY", &Node.sigmaY, 0.0f, 10.0f);

				ClampNonNegative(Node.sigmaX);
				ClampNonNegative(Node.sigmaY);

				ImGui::Separator();

				break;
			}
			case FilterTypes::Threshold:
			{
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Thresholding Settings");
				ImGui::Separator();

				const char* ThreshModes[] = { "Global", "Otsu", "Adaptive Mean", "Adaptive Gaussian" };
				int ActiveThreshMode = (int)Node.ThresholdType;

				if (ImGui::Combo("Method", &ActiveThreshMode, ThreshModes, IM_ARRAYSIZE(ThreshModes))) {
					Node.ThresholdType = (ThresholdType)ActiveThreshMode;
				}

				if (Node.ThresholdType == ThresholdType::Global) {
					ImGui::SliderFloat("Threshold Value", &Node.GlobalThreshold, 0.0f, 255.0f, "%.0f");
				}

				ImGui::SliderFloat("Max Binary Value", &Node.MaxBinaryValue, 0.0f, 255.0f, "%.0f");


				if (Node.ThresholdType > ThresholdType::Otsu) {
					if (ImGui::SliderInt("Block Size", &Node.AdaptiveBlockSize, 3, 99)) {
						if (Node.AdaptiveBlockSize % 2 == 0) Node.AdaptiveBlockSize++;
					}
					ImGui::SliderFloat("Constant (C)", &Node.AdaptiveC, -10.0f, 30.0f, "%.1f");
				}

				ImGui::Separator();
				break;
			}

			case FilterTypes::Morphology:
			{
				const char* MorphNames[] = {
					"ERODE",
					"DILATE",
					"OPEN",
					"CLOSE",
					"GRADIENT",
					"TOPHAT",
					"BLACKHAT",
					"HITMISS"
				};
				int ActiveMorphType = (int)Node.MorphType;

				const char* shapes[] = { "Rectangle", "Cross", "Ellipse", "Diamond"};
				int ActiveMorphShape = (int)Node.MorphShape;

				if (ImGui::Combo("Morph Type", &ActiveMorphType, MorphNames, IM_ARRAYSIZE(MorphNames))) {
					Node.MorphType = (cv::MorphTypes)ActiveMorphType;
				}

				if (ImGui::Combo("Kernel Shape", &ActiveMorphShape, shapes, IM_ARRAYSIZE(shapes))) {
					Node.MorphShape = (cv::MorphShapes)ActiveMorphShape;
				}
				ImGui::SliderInt("Kernel Size", &Node.MorphKernelSize, 1, 15);
				ImGui::SliderInt("Iterations", &Node.MorphIterations, 1, 10);

				break;

			}

			case FilterTypes::Skeletonize:
			{
				ImGui::SliderInt("Pruning (Spurs)", &Node.PruningIterations, 0, 50);
				break;
			}
			case FilterTypes::Sharpen:
			{
				const char* ShapenModes[] = { "Basic Kernel", "Unsharp Mask", "Laplacian", "Frangi (Vesselness)" };
				int ActiveSMode = (int)Node.SharpenType;

				if (ImGui::Combo("Enhance Method", &ActiveSMode, ShapenModes, IM_ARRAYSIZE(ShapenModes))) {
					Node.SharpenType = (SharpenTypes)ActiveSMode;
				}

				ImGui::Separator();

				switch (Node.SharpenType) {
				case SharpenTypes::SharpenKernel:
				{
					ImGui::SliderFloat("Kernel Strength", &Node.KernelStrength, 0.1f, 5.0f);
					break;
				}

				case SharpenTypes::SharpenUnsharp:
				{
					ImGui::SliderFloat("Sigma (Blur)", &Node.UnsharpSigma, 0.1f, 10.0f);
					ImGui::SliderFloat("Amount", &Node.UnsharpAmount, 0.1f, 5.0f);
					break;
				}

				case SharpenTypes::SharpenLaplacian:
				{
					if (ImGui::SliderInt("K-Size", &Node.LaplacianKSize, 1, 7)) {
						if (Node.LaplacianKSize % 2 == 0) Node.LaplacianKSize++;
					}
					ImGui::SliderFloat("Scale", &Node.LaplacianScale, 0.1f, 5.0f);
					ImGui::SliderFloat("Scale", &Node.LaplacianSubAlpha, 0.0f, 2.0f);

					break;
				}

				case SharpenTypes::Frangi:
				{
					ImGui::TextColored(ImVec4(1, 1, 0, 1), "Vessel Detection Mode");
					ImGui::SliderInt("Ridge K-Size", &Node.RidgeKSize, 1, 7);
					ImGui::SliderFloat("Ridge Scale", &Node.RidgeScale, 0.1f, 10.0f);
					ImGui::SliderFloat("Alpha (Noise)", &Node.RidgeAlpha, 0.01f, 1.0f);
					ImGui::SliderFloat("Beta (Blob)", &Node.RidgeBeta, 0.01f, 1.0f);
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