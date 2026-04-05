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
	"Sharpen",
	"Canny Edge",
	"Sobel Derivative",
	"Brightness & Contrast",
	"Invert (Bitwise NOT)",
	"Gamma Correction",
	"Histogram Equalization",
	"Box Filter (Blur)",
	"Distance Transform"
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
		switch (Node.Type) {
		case FilterTypes::CLAHE: Node.Config = FilterCLAHE{}; break;
		case FilterTypes::MedianBlur: Node.Config = FilterMedianBlur{}; break;
		case FilterTypes::BilateralBlur: Node.Config = FilterBilateralBlur{}; break;
		case FilterTypes::GaussianBlur: Node.Config = FilterGaussianBlur{}; break;
		case FilterTypes::Threshold: Node.Config = FilterThreshold{}; break;
		case FilterTypes::Morphology: Node.Config = FilterMorphology{}; break;
		case FilterTypes::Skeletonize: Node.Config = FilterSkeletonize{}; break;
		case FilterTypes::Sharpen: Node.Config = FilterSharpen{}; break;
		case FilterTypes::Canny: Node.Config = FilterCanny{}; break;
		case FilterTypes::Sobel: Node.Config = FilterSobel{}; break;
		case FilterTypes::BrightnessContrast: Node.Config = FilterBrightnessContrast{}; break;
		case FilterTypes::Invert: Node.Config = FilterInvert{}; break;
		case FilterTypes::Gamma: Node.Config = FilterGamma{}; break;
		case FilterTypes::HistEqGlobal: Node.Config = FilterHistEq{}; break;
		case FilterTypes::BoxBlur: Node.Config = FilterBoxBlur{}; break;
		}
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
				auto& cfg = std::get<FilterCLAHE>(Node.Config);
				ImGui::Separator();

				ImGui::SliderFloat("Clahe Clip Limit", &cfg.claheClipLimit, 0, 10);
				ImGui::Separator();

				break;
			}

			case FilterTypes::MedianBlur:
			{
				auto& cfg = std::get<FilterMedianBlur>(Node.Config);
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Median Settings");

				ImGui::SliderInt("Kernel (odd)", &cfg.medianK, 3, 15);
				cfg.medianK = ClampKernel(cfg.medianK);

				ImGui::Separator();

				break;
			}
			case FilterTypes::BilateralBlur:
			{
				auto& cfg = std::get<FilterBilateralBlur>(Node.Config);
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Bilateral Settings");

				ImGui::SliderInt("Diameter d", &cfg.bilateralD, 1, 25);
				ImGui::SliderFloat("Sigma Color", &cfg.sigmaColor, 1.0f, 200.0f);
				ImGui::SliderFloat("Sigma Space", &cfg.sigmaSpace, 1.0f, 200.0f);

				ClampNonNegative(cfg.sigmaColor);
				ClampNonNegative(cfg.sigmaSpace);

				ImGui::Separator();

				break;
			}
			case FilterTypes::GaussianBlur:
			{
				auto& cfg = std::get<FilterGaussianBlur>(Node.Config);
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Gaussian Settings");

				ImGui::SliderInt("Kernel (odd)", &cfg.gaussK, 3, 31);
				cfg.gaussK = ClampKernel(cfg.gaussK);

				ImGui::SliderFloat("SigmaX", &cfg.sigmaX, 0.0f, 10.0f);
				ImGui::SliderFloat("SigmaY", &cfg.sigmaY, 0.0f, 10.0f);

				ClampNonNegative(cfg.sigmaX);
				ClampNonNegative(cfg.sigmaY);

				ImGui::Separator();

				break;
			}
			case FilterTypes::Threshold:
			{
				auto& cfg = std::get<FilterThreshold>(Node.Config);
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Thresholding Settings");
				ImGui::Separator();

				const char* ThreshModes[] = { "Global", "Otsu", "Adaptive Mean", "Adaptive Gaussian" };
				int ActiveThreshMode = (int)cfg.ThresholdType;

				if (ImGui::Combo("Method", &ActiveThreshMode, ThreshModes, IM_ARRAYSIZE(ThreshModes))) {
					cfg.ThresholdType = (ThresholdType)ActiveThreshMode;
				}

				if (cfg.ThresholdType == ThresholdType::Global) {
					ImGui::SliderFloat("Threshold Value", &cfg.GlobalThreshold, 0.0f, 255.0f, "%.0f");
				}

				ImGui::SliderFloat("Max Binary Value", &cfg.MaxBinaryValue, 0.0f, 255.0f, "%.0f");


				if (cfg.ThresholdType > ThresholdType::Otsu) {
					if (ImGui::SliderInt("Block Size", &cfg.AdaptiveBlockSize, 3, 99)) {
						if (cfg.AdaptiveBlockSize % 2 == 0) cfg.AdaptiveBlockSize++;
					}
					ImGui::SliderFloat("Constant (C)", &cfg.AdaptiveC, -10.0f, 30.0f, "%.1f");
				}

				ImGui::Separator();
				break;
			}

			case FilterTypes::Morphology:
			{
				auto& cfg = std::get<FilterMorphology>(Node.Config);
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
				int ActiveMorphType = (int)cfg.MorphType;

				const char* shapes[] = { "Rectangle", "Cross", "Ellipse", "Diamond"};
				int ActiveMorphShape = (int)cfg.MorphShape;

				if (ImGui::Combo("Morph Type", &ActiveMorphType, MorphNames, IM_ARRAYSIZE(MorphNames))) {
					cfg.MorphType = (cv::MorphTypes)ActiveMorphType;
				}

				if (ImGui::Combo("Kernel Shape", &ActiveMorphShape, shapes, IM_ARRAYSIZE(shapes))) {
					cfg.MorphShape = (cv::MorphShapes)ActiveMorphShape;
				}
				ImGui::SliderInt("Kernel Size", &cfg.MorphKernelSize, 1, 15);
				ImGui::SliderInt("Iterations", &cfg.MorphIterations, 1, 10);

				break;

			}

			case FilterTypes::Skeletonize:
			{
				auto& cfg = std::get<FilterSkeletonize>(Node.Config);
				ImGui::SliderInt("Pruning (Spurs)", &cfg.PruningIterations, 0, 50);
				break;
			}
			case FilterTypes::Sharpen:
			{
				auto& cfg = std::get<FilterSharpen>(Node.Config);
				const char* ShapenModes[] = { "Basic Kernel", "Unsharp Mask", "Laplacian", "Frangi (Vesselness)" };
				int ActiveSMode = (int)cfg.SharpenType;

				if (ImGui::Combo("Enhance Method", &ActiveSMode, ShapenModes, IM_ARRAYSIZE(ShapenModes))) {
					cfg.SharpenType = (SharpenTypes)ActiveSMode;
				}

				ImGui::Separator();

				switch (cfg.SharpenType) {
				case SharpenTypes::SharpenKernel:
				{
					ImGui::SliderFloat("Kernel Strength", &cfg.KernelStrength, 0.1f, 5.0f);
					break;
				}

				case SharpenTypes::SharpenUnsharp:
				{
					ImGui::SliderFloat("Sigma (Blur)", &cfg.UnsharpSigma, 0.1f, 10.0f);
					ImGui::SliderFloat("Amount", &cfg.UnsharpAmount, 0.1f, 5.0f);
					break;
				}

				case SharpenTypes::SharpenLaplacian:
				{
					if (ImGui::SliderInt("K-Size", &cfg.LaplacianKSize, 1, 7)) {
						if (cfg.LaplacianKSize % 2 == 0) cfg.LaplacianKSize++;
					}
					ImGui::SliderFloat("Scale", &cfg.LaplacianScale, 0.1f, 5.0f);
					ImGui::SliderFloat("Scale", &cfg.LaplacianSubAlpha, 0.0f, 2.0f);

					break;
				}

				case SharpenTypes::Frangi:
				{
					ImGui::TextColored(ImVec4(1, 1, 0, 1), "Vessel Detection Mode");
					ImGui::SliderInt("Ridge K-Size", &cfg.RidgeKSize, 1, 7);
					ImGui::SliderFloat("Ridge Scale", &cfg.RidgeScale, 0.1f, 10.0f);
					ImGui::SliderFloat("Alpha (Noise)", &cfg.RidgeAlpha, 0.01f, 1.0f);
					ImGui::SliderFloat("Beta (Blob)", &cfg.RidgeBeta, 0.01f, 1.0f);
					break;
				}

				}
				break;

			}
			case FilterTypes::Canny:
			{
				auto& cfg = std::get<FilterCanny>(Node.Config);
				ImGui::Separator();
				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Canny Edge Settings");
				ImGui::SliderFloat("Threshold 1", &cfg.threshold1, 0.0f, 500.0f);
				ImGui::SliderFloat("Threshold 2", &cfg.threshold2, 0.0f, 500.0f);
				ImGui::SliderInt("Aperture Size", &cfg.apertureSize, 3, 7);
				if (cfg.apertureSize % 2 == 0) cfg.apertureSize++;
				ImGui::Checkbox("L2 Gradient", &cfg.L2gradient);
				ImGui::Separator();
				break;
			}
			case FilterTypes::Sobel:
			{
				auto& cfg = std::get<FilterSobel>(Node.Config);
				ImGui::Separator();
				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Sobel Settings");
				ImGui::SliderInt("dx (X Derivative)", &cfg.dx, 0, 2);
				ImGui::SliderInt("dy (Y Derivative)", &cfg.dy, 0, 2);
				ImGui::SliderInt("Kernel Size", &cfg.ksize, 1, 7);
				if (cfg.ksize % 2 == 0) cfg.ksize++;
				ImGui::SliderFloat("Scale", &cfg.scale, 0.1f, 5.0f);
				ImGui::SliderFloat("Delta", &cfg.delta, 0.0f, 255.0f);
				ImGui::Separator();
				break;
			}
			case FilterTypes::BrightnessContrast:
			{
				auto& cfg = std::get<FilterBrightnessContrast>(Node.Config);
				ImGui::Separator();
				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Brightness & Contrast Settings");
				ImGui::SliderFloat("Contrast (Alpha)", &cfg.alpha, 0.0f, 3.0f);
				ImGui::SliderFloat("Brightness (Beta)", &cfg.beta, -100.0f, 100.0f);
				ImGui::Separator();
				break;
			}
			case FilterTypes::Invert:
			{
				ImGui::Separator();
				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Invert Mode");
				ImGui::Text("Inverts color mathematically: 255 - pixel");
				ImGui::Separator();
				break;
			}
			case FilterTypes::Gamma:
			{
				auto& cfg = std::get<FilterGamma>(Node.Config);
				ImGui::Separator();
				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Gamma Correction");
				ImGui::SliderFloat("Gamma Value", &cfg.gamma, 0.1f, 5.0f);
				ImGui::Separator();
				break;
			}
			case FilterTypes::HistEqGlobal:
			{
				ImGui::Separator();
				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Histogram Equalization");
				ImGui::Text("Standard global equalizeHist(), natively runs grayscale");
				ImGui::Separator();
				break;
			}
			case FilterTypes::BoxBlur:
			{
				auto& cfg = std::get<FilterBoxBlur>(Node.Config);
				ImGui::Separator();
				ImGui::TextColored(ImVec4(0, 1, 1, 1), "Box Filter Settings");
				ImGui::SliderInt("Kernel Size", &cfg.ksize, 1, 31);
				if (cfg.ksize % 2 == 0) cfg.ksize++;
				ImGui::Separator();
				break;
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