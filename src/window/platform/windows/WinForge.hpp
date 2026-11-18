#ifndef WINFORGE_H
#define WINFORGE_H

#pragma once

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#include "ZLogger.h"

#include <Windows.h>
#include <cstdint>

using WindowProc = LRESULT (*)(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct WinConfig
{
    const wchar_t* ClassName  = L"WinForgeClass";
    const wchar_t* WindowName = L"Window";
    UINT           Width      = 1280;
    UINT           Height     = 720;
    DWORD          Style      = WS_POPUP;
    DWORD          ExStyle    = WS_EX_APPWINDOW;
    HINSTANCE      HInstance  = nullptr;
    WindowProc     Hook       = nullptr;

    WinConfig() = default;

    WinConfig(
        const wchar_t* WindowName_,
        UINT           Width_   = 1280,
        UINT           Height_  = 720,
        DWORD          Style_   = WS_POPUP,
        DWORD          ExStyle_ = WS_EX_APPWINDOW
    )
        : WindowName(WindowName_), Width(Width_), Height(Height_), Style(Style_), ExStyle(ExStyle_)
    {
    }
};

class WinForge
{
  public:
    WinForge() = default;
    explicit WinForge(WindowProc Hook);
    ~WinForge();

    WinForge(const WinForge&)            = delete;
    WinForge& operator=(const WinForge&) = delete;

    WinForge(WinForge&& other) noexcept;
    WinForge& operator=(WinForge&& other) noexcept;

    // Window creation
    bool Create(const WinConfig& Config);
    bool Create(
        const wchar_t* Title   = L"Window",
        uint32_t       Width   = 1280,
        uint32_t       Height  = 720,
        DWORD          Style   = WS_OVERLAPPEDWINDOW,
        DWORD          ExStyle = WS_EX_APPWINDOW
    );

    // Window Event Processing, So.. Call this once per frame in the render loop
    // Returns false when the window is closed or WM_QUIT is received.
    bool ProcessMessages();

    // Lifecycle & State
    bool GetState() const;
    void Close();
    void Destroy();

    // Resize tracking for DirectX swapchain
    bool     WasResized() const { return Resized; }
    void     ResetResized() { Resized = false; }
    uint32_t GetWidth() const { return Width; }
    uint32_t GetHeight() const { return Height; }
    void     SetSize(uint32_t NewWidth, uint32_t NewHeight);

    // Window controls
    HWND GetHwnd() const { return Hwnd; }
    void Show(int nCmdShow = SW_SHOW);
    void Hide();
    void SetTitle(const wchar_t* Title);

    // Overlay / HUD helpers
    void SetTopMost(bool TopMost);
    void SetClickThrough(bool ClickThrough);

    // External Window Event Proc
    void SetEventHook(WindowProc HookCallback) { Hook = HookCallback; }

    static LRESULT CALLBACK DefaultWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  private:
    HWND       Hwnd          = NULL;
    uint32_t   Width         = 1280;
    uint32_t   Height        = 720;
    bool       Resized       = false;
    bool       ExitRequested = false;
    WindowProc Hook          = nullptr;

    bool RegisterWindowClass(HINSTANCE hInstance, const wchar_t* className);
};

#endif // WINFORGE_H
