#include "WinForge.hpp"
#include <errhandlingapi.h>

WinForge::WinForge(WindowProc HookCallback) : Hook(HookCallback) {}

WinForge::~WinForge()
{
    Destroy();
}

WinForge::WinForge(WinForge&& other) noexcept
    : Hwnd(other.Hwnd), Width(other.Width), Height(other.Height), Resized(other.Resized),
      ExitRequested(other.ExitRequested), Hook(other.Hook)
{
    if (Hwnd) {
        SetWindowLongPtrW(Hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
    other.Hwnd = NULL;
}

WinForge& WinForge::operator=(WinForge&& other) noexcept
{
    if (this != &other) {
        Destroy();

        Hwnd          = other.Hwnd;
        Width         = other.Width;
        Height        = other.Height;
        Resized       = other.Resized;
        ExitRequested = other.ExitRequested;
        Hook          = other.Hook;

        if (Hwnd) {
            SetWindowLongPtrW(Hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }
        other.Hwnd = NULL;
    }
    return *this;
}

bool WinForge::RegisterWindowClass(HINSTANCE hInstance, const wchar_t* className)
{
    WNDCLASSEXW wndClass = {};
    if (GetClassInfoExW(hInstance, className, &wndClass)) {
        return true;
    }

    wndClass.cbSize        = sizeof(WNDCLASSEXW);
    wndClass.style         = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc   = DefaultWndProc;
    wndClass.hInstance     = hInstance;
    wndClass.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wndClass.lpszClassName = className;

    return RegisterClassExW(&wndClass) != 0;
}

bool WinForge::Create(const WinConfig& Config)
{
    Destroy();

    HINSTANCE hInstance = Config.HInstance ? Config.HInstance : GetModuleHandleW(nullptr);

    if (!RegisterWindowClass(hInstance, Config.ClassName)) {
        Logger::log("Window Class Reg Died : {}", GetLastError());
        return false;
    }

    const int ScreenWidth  = GetSystemMetrics(SM_CXSCREEN);
    const int ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    const int PosX         = (ScreenWidth - static_cast<int>(Config.Width)) / 2;
    const int PosY         = (ScreenHeight - static_cast<int>(Config.Height)) / 2;

    Width         = Config.Width;
    Height        = Config.Height;
    ExitRequested = false;
    Resized       = false;
    if (Config.Hook) {
        Hook = Config.Hook;
    }

    Hwnd = CreateWindowExW(
        Config.ExStyle,
        Config.ClassName,
        Config.WindowName,
        Config.Style,
        PosX,
        PosY,
        Config.Width,
        Config.Height,
        nullptr,
        nullptr,
        hInstance,
        this
    );

    if (Hwnd == NULL) {
        Logger::log("Window Creation Failed : {}", GetLastError());
        return false;
    }

    if (Config.ExStyle & WS_EX_LAYERED) {
        SetLayeredWindowAttributes(Hwnd, 0, 255, LWA_ALPHA);
    }

    ShowWindow(Hwnd, SW_SHOW);
    UpdateWindow(Hwnd);

    return true;
}

bool WinForge::Create(
    const wchar_t* Title, uint32_t Width_, uint32_t Height_, DWORD Style, DWORD ExStyle
)
{
    WinConfig Config;
    Config.WindowName = Title ? Title : L"Window";
    Config.Width      = (Width_ > 0) ? Width_ : 1280;
    Config.Height     = (Height_ > 0) ? Height_ : 720;
    Config.Style      = Style;
    Config.ExStyle    = ExStyle;
    return Create(Config);
}

bool WinForge::ProcessMessages()
{
    MSG Msg = {};
    while (PeekMessageW(&Msg, nullptr, 0, 0, PM_REMOVE)) {
        if (Msg.message == WM_QUIT) {
            ExitRequested = true;
            return false;
        }
        TranslateMessage(&Msg);
        DispatchMessageW(&Msg);
    }
    return !ExitRequested && (Hwnd != NULL);
}

bool WinForge::GetState() const
{
    return !ExitRequested && (Hwnd != NULL) && IsWindow(Hwnd);
}

void WinForge::Close()
{
    ExitRequested = true;
    if (Hwnd && IsWindow(Hwnd)) {
        PostMessageW(Hwnd, WM_CLOSE, 0, 0);
    }
}

void WinForge::Destroy()
{
    ExitRequested = true;
    if (Hwnd && IsWindow(Hwnd)) {
        DestroyWindow(Hwnd);
        Hwnd = NULL;
    }
}

void WinForge::Show(int nCmdShow)
{
    if (Hwnd && IsWindow(Hwnd)) {
        ShowWindow(Hwnd, nCmdShow);
        UpdateWindow(Hwnd);
    }
}

void WinForge::Hide()
{
    if (Hwnd && IsWindow(Hwnd)) {
        ShowWindow(Hwnd, SW_HIDE);
    }
}

void WinForge::SetTitle(const wchar_t* Title)
{
    if (Hwnd && IsWindow(Hwnd)) {
        SetWindowTextW(Hwnd, Title);
    }
}

void WinForge::SetSize(uint32_t NewWidth, uint32_t NewHeight)
{
    Width  = NewWidth;
    Height = NewHeight;
    if (Hwnd && IsWindow(Hwnd)) {
        SetWindowPos(
            Hwnd, NULL, 0, 0, NewWidth, NewHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
        );
    }
}

void WinForge::SetTopMost(bool TopMost)
{
    if (Hwnd && IsWindow(Hwnd)) {
        SetWindowPos(
            Hwnd,
            TopMost ? HWND_TOPMOST : HWND_NOTOPMOST,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
        );
    }
}

void WinForge::SetClickThrough(bool ClickThrough)
{
    if (Hwnd && IsWindow(Hwnd)) {
        LONG_PTR exStyle = GetWindowLongPtrW(Hwnd, GWL_EXSTYLE);
        if (ClickThrough) {
            exStyle |= (WS_EX_TRANSPARENT | WS_EX_LAYERED);
        } else {
            exStyle &= ~WS_EX_TRANSPARENT;
        }
        SetWindowLongPtrW(Hwnd, GWL_EXSTYLE, exStyle);
    }
}

LRESULT CALLBACK WinForge::DefaultWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WinForge* Window = reinterpret_cast<WinForge*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    // Forwarding to custom hooks to intercept events
    if (Window && Window->Hook) {
        if (Window->Hook(hwnd, uMsg, wParam, lParam)) {
            return true;
        }
    }

    switch (uMsg) {
    case WM_NCCREATE: {
        auto CreateStructw = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(
            hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(CreateStructw->lpCreateParams)
        );
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    case WM_SIZE: {
        if (Window) {
            uint32_t newWidth  = LOWORD(lParam);
            uint32_t newHeight = HIWORD(lParam);
            if (newWidth != Window->Width || newHeight != Window->Height) {
                Window->Width   = newWidth;
                Window->Height  = newHeight;
                Window->Resized = true;
            }
        }
        return 0;
    }

    case WM_SETCURSOR:
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return TRUE;

    case WM_CLOSE: {
        if (Window) {
            Window->ExitRequested = true;
        }
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_DESTROY: {
        if (Window) {
            Window->ExitRequested = true;
            Window->Hwnd          = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
