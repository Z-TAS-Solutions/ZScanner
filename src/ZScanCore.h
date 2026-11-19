#ifndef UNICODE
#define UNICODE
#endif

#ifndef ZSCANCORE_H
#define ZSCANCORE_H

#define NOMINMAX

#pragma once

#include "FilterTypes.hpp"
#include "Renderer.h"
#include "RendererCore.h"
#include "SSHHandler.h"
#include "Window.hpp"

#include "ZCore.h"
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <shellapi.h>
#include <string>
#include <variant>
#include <vector>

#define NOMINMAX
#include <Windows.h>
#include <wrl/client.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>

#define WM_TRAYICON (WM_USER + 1)

class ZScanGUI;

enum StreamMode { RTSP, TCP };

enum CaptureMode { Default, GStream };

struct StreamConfig
{
    int width          = 1280;
    int height         = 720;
    int bitDepth       = 0;
    int captureMode    = CaptureMode::Default;
    int fpsLimit       = 60;
    int renderFpsLimit = 75;
};

struct CameraConfig
{
    int  shutterSpeed = 0;
    int  iso          = 100;
    int  awbMode      = 0;
    int  rotation     = 0;
    bool hFlip        = false;
    bool vFlip        = false;
    int  timeout      = 5000;
};

enum MenuIndex { Dashboard, LiveFeed, ImageTest, Database, Settings };

enum LiveFeedState { CLOSED, READY, LIVE };

struct FSizeWxH
{
    float width;
    float height;

    FSizeWxH(float Width, float Height) : width(Width), height(Height) {}
};

struct ISizeWxH
{
    int width;
    int height;

    ISizeWxH(int Width, int Height) : width(Width), height(Height) {}
};

struct CVParams
{
    std::vector<FilterNode> Filters;

    int   adaptiveThreshold   = 11;
    float minDefectDepthRatio = 0.05f;
};

class ZScanCore
{

  public:
    std::vector<cv::Point2f> ROI;
    cv::Mat                  ROIFrame;
    bool                     verification = false;
    std::vector<std::string> Directories;
    MenuIndex                ActiveMenu     = MenuIndex::Dashboard;
    LiveFeedState            LiveFeedStatus = LiveFeedState::CLOSED;

    bool ScannerSignIn(
        const std::string& IP,
        int                Port,
        const std::string& Username,
        const std::string& PubKeyPath,
        const std::string& PrvKeyPath,
        const std::string& Passphrase
    );

    void UpdateStreamConfig(const StreamConfig& Config);

    void UpdateCameraConfig(const CameraConfig& Config);

    bool CheckScannerStatus();

    bool SetupGStreamerPipeline8Bit(
        const std::string& host,
        int                port,
        StreamMode         mode,
        bool               GPUAccel,
        bool               monochrome,
        cv::VideoCapture&  cap
    );

    bool SetupGStreamerPipeline10Bit(
        const std::string& host,
        int                port,
        StreamMode         mode,
        bool               GPUAccel,
        bool               monochrome,
        cv::VideoCapture&  cap
    );

    std::string GenerateStreamURL(StreamMode mode, std::string_view ip, int port);

    bool OpenStream(const std::string& url);

    bool OpenStream(const std::string_view ip, int port, StreamMode mode);

    bool OpenGStream8Bit(const std::string& ip, int port, StreamMode mode);

    bool OpenGStream10Bit(std::string ip, int port, StreamMode mode);

    void CloseStream();

    bool Capture2ImageAnalysis();

    bool CaptureROI2ImageAnalysis();

    bool ExportLiveFeedImage(const std::string& filename = "");

    inline void CaptureLiveFeed()
    {
        CaptureEngine.read(MainFrame);
        if (!CaptureEngine.read(MainFrame) || MainFrame.empty()) {
            return;
        }
    }

    inline void UpdateMainFeed(cv::Mat& srcFrame)
    {

        D3D11Context->UpdateSubresource(MainFeedTex, 0, nullptr, srcFrame.data, srcFrame.step, 0);
    }

    inline void UpdateSubFeed(cv::Mat& srcFrame)
    {
        if (!CheckSubFeedMonoExpansionStatus() || CheckSubFeedSizeMismatch(srcFrame)) {
            SetupSubFeedMonoExpansion(srcFrame);
        }

        D3D11Context->UpdateSubresource(SubFeedTex, 0, nullptr, srcFrame.data, srcFrame.step, 0);

        D3D11Context->OMSetRenderTargets(1, &SubOutputFeedRTV, nullptr);
        D3D11Context->RSSetViewports(1, &SubOutViewPort);

        D3D11Context->PSSetShader(PixelShader.Get(), nullptr, 0);
        D3D11Context->PSSetShaderResources(0, 1, &SubFeedSRV);

        D3D11Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        D3D11Context->Draw(4, 0);

        if (renderTargetView) {
            D3D11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);
        }
    }

    inline void Enroll()
    {
        if (MainImageFrame.empty())
            return;

        EnrolledFrame = MainImageFrame.clone();
        UpdateSubFeed(EnrolledFrame);

        cv::Mat vesselness, Lxx, Lxy, Lyy;
        vesselness = ZCore::PreProcessTest(MainImageFrame, Lxx, Lxy, Lyy);
        ZCore::GenerateTemplate(vesselness, Lxx, Lxy, Lyy, Template, TemplateMask);

        cv::Mat rainbow;
        Template.convertTo(rainbow, CV_8U, 85);
        cv::imshow("Identity Map", rainbow);

        std::cout << "Enrolled" << "\n";
    }

    inline void UpdateImageFeed(std::string FilePath)
    {

        MainImageFrame = cv::imread(FilePath, cv::IMREAD_GRAYSCALE);

        if (MainImageFrame.empty()) {
            MainImageFrame = cv::imread(FilePath, cv::IMREAD_UNCHANGED);
            if (MainImageFrame.depth() != CV_8U)
                MainImageFrame.convertTo(MainImageFrame, CV_8U);

            if (MainImageFrame.empty()) {
                return;
            }
        }

        if (CheckMainFeedSizeMismatch(MainImageFrame)) {
            ResizeMonoExpansionPipeline(MainImageFrame);
        }

        OriginalFrame = MainImageFrame.clone();

        SetReconfig();
        redraw = true;
    }

    inline void ApplyClahe() { CLengine->apply(MainFrame, MainFrame); }

    /*inline void UpdateClahe() {
            CLengine->setClipLimit(CV2Params.claheClipLimit);
            CLengine->setTilesGridSize(CV2Params.GridLimit);
    }*/

    inline void CheckTypeData(cv::Mat& Frame)
    {
        std::cout << Frame.type() << std::endl;
        std::cout << Frame.channels() << std::endl;
        std::cout << Frame.depth() << std::endl;
    }

    inline void SetReconfig() { reconfig = true; }

    inline void SetRedraw() { redraw = true; }

    inline void ToggleMatching()
    {
        matching = !matching;
        if (matching)
            std::cout << "Matching Enabled" << "\n";
        else
            std::cout << "Matching Disabled" << "\n";

        redraw = true;
    }

    inline void onRedrawDirMode() {}

    inline void ClearVerification()
    {
        verification = false;
        matching = false;
    }

    void ScanDirectory(const std::string path);

  protected:
    HANDLE* Events  = nullptr;
    DWORD   EventDW = NULL;

    ZSSHHandler SSHEngine{};

    ID3D11Device*           D3D11Device      = nullptr;
    ID3D11DeviceContext*    D3D11Context     = nullptr;
    IDXGISwapChain3*        swapchain        = nullptr;
    ID3D11RenderTargetView* renderTargetView = nullptr;

    ComPtr<ID3D11PixelShader>  PixelShader;
    ID3D11PixelShader*         PixelShaderPtr = nullptr;
    ComPtr<ID3D11VertexShader> VertexShader;
    ID3D11SamplerState*        Sampler;

    ID3D11Texture2D*          MainFeedTex = nullptr;
    ID3D11ShaderResourceView* MainFeedSRV = nullptr;

    ID3D11Texture2D*          MainOutputFeedTex = nullptr;
    ID3D11ShaderResourceView* MainOutputFeedSRV = nullptr;
    ID3D11RenderTargetView*   MainOutputFeedRTV = nullptr;
    D3D11_VIEWPORT            MainOutViewPort   = {};

    ID3D11Texture2D*          SubFeedTex = nullptr;
    ID3D11ShaderResourceView* SubFeedSRV = nullptr;

    ID3D11Texture2D*          SubOutputFeedTex = nullptr;
    ID3D11ShaderResourceView* SubOutputFeedSRV = nullptr;
    ID3D11RenderTargetView*   SubOutputFeedRTV = nullptr;
    D3D11_VIEWPORT            SubOutViewPort   = {};

    bool reconfig = false;
    bool redraw   = false;
    bool matching = false;

    CVParams CV2Params;

    cv::Mat MainFrame;

    cv::Mat MainImageFrame;

    cv::Mat OriginalFrame;

    cv::Mat EnrolledFrame;

    cv::Mat ClaheFrame;

    cv::Mat ThresholdFrame;

    cv::Mat MaskFrame;

    cv::Mat RFrame;

    cv::Mat Template;
    cv::Mat TemplateMask;

    cv::VideoCapture CaptureEngine;

    cv::Ptr<cv::CLAHE> CLengine;

    void InitializeMonoExpansion(ZRenderer& Renderer);

    void SetupMonoExpansionInput(cv::Mat& Frame);

    void SetupMonoExpansionOutput(const ISizeWxH& Size);

    void SetupMonoExpansion(cv::Mat& Frame);

    void ResizeMonoExpansionPipeline(cv::Mat& Frame);

    void ReleaseMonoExpansion();

    bool CheckMonoExpansionStatus();

    inline bool CheckMainFeedSizeMismatch(cv::Mat& Frame)
    {
        return (
            Frame.cols != (int)MainOutViewPort.Width || Frame.rows != (int)MainOutViewPort.Height
        );
    }

    void SetupSubFeedMonoExpansionInput(cv::Mat& Frame);

    void SetupSubFeedMonoExpansionOutput(const ISizeWxH& Size);

    void SetupSubFeedMonoExpansion(cv::Mat& Frame);

    void ResizeSubFeedMonoExpansionPipeline(cv::Mat& Frame);

    void ReleaseSubFeedMonoExpansion();

    bool CheckSubFeedMonoExpansionStatus();

    inline bool CheckSubFeedSizeMismatch(cv::Mat& Frame)
    {
        return (
            Frame.cols != (int)SubOutViewPort.Width || Frame.rows != (int)SubOutViewPort.Height
        );
    }
};

class ZScan : public ZScanCore
{
  public:
    void ZScanMain(HINSTANCE hInstance, int nCmdShow);

    inline void ModeSwitch(MenuIndex Index)
    {
        ActiveMenu = Index;
        switch (Index) {
        case MenuIndex::LiveFeed: {
            switch (LiveFeedStatus) {
            case LiveFeedState::CLOSED: {
                ReleaseMonoExpansion();
                break;
            }
            case LiveFeedState::READY:
            case LiveFeedState::LIVE: {
                if (!CheckMonoExpansionStatus() || CheckMainFeedSizeMismatch(MainFrame)) {
                    SetupMonoExpansion(MainFrame);
                }
                break;
            }
            }
            break;
        }

        case MenuIndex::ImageTest: {
            if (MainImageFrame.empty()) {
                ReleaseMonoExpansion();
            } else {
                if (!CheckMonoExpansionStatus() || CheckMainFeedSizeMismatch(MainImageFrame)) {
                    SetupMonoExpansion(MainImageFrame);

                    SetRedraw();
                } else
                    SetRedraw();
            }

            if (!EnrolledFrame.empty() &&
                (!CheckSubFeedMonoExpansionStatus() || CheckSubFeedSizeMismatch(EnrolledFrame))) {
                UpdateSubFeed(EnrolledFrame);
            }

            break;
        }
        }
    }

    void SetRenderFPS(const uint32_t& FPS)
    {
        FrameTimeLimit = std::chrono::nanoseconds(((1 / FPS) * 1000000000));
    }

  private:
    // GUI
    ZScanGUI* GUI = nullptr;

    HWND   hwnd;
    Window MainWindow;

    NOTIFYICONDATAW TrayIconData = {};

    std::chrono::steady_clock::duration FrameTimeLimit = std::chrono::nanoseconds(12 * 1000000);

    std::chrono::time_point<std::chrono::steady_clock> LastFrameTime =
        std::chrono::steady_clock::now();

    float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    static LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    MSG msg = {};

    void ZScanMainLoop();

    void InitTrayIcon(HWND hwnd);
};

#endif
