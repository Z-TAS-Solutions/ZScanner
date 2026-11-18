#pragma once

#include <cstdint>

class LinuxForge {
public:
    LinuxForge() = default;
    ~LinuxForge() { Destroy(); }

    bool Create(const char* title = "Window", uint32_t width = 1280, uint32_t height = 720) {
        (void)title;
        Width = width;
        Height = height;
        Running = true;
        return true;
    }

    void Destroy() { Running = false; }
    void Close() { Running = false; }

    bool PollEvents() { return Running; }
    bool IsOpen() const { return Running; }
    bool ShouldClose() const { return !Running; }

    bool WasResized() const { return Resized; }
    void ResetResized() { Resized = false; }

    uint32_t GetWidth() const { return Width; }
    uint32_t GetHeight() const { return Height; }
    void SetSize(uint32_t w, uint32_t h) { Width = w; Height = h; Resized = true; }

private:
    uint32_t Width   = 1280;
    uint32_t Height  = 720;
    bool     Running = false;
    bool     Resized = false;
};
