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