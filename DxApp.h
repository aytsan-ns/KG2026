#pragma once
#include "DxCommon.h"

class DxApp
{
public:
    bool Init(HINSTANCE hInstance);
    int  Run();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool InitWindow(HINSTANCE hInstance);
    bool InitDirectX();
    void CreateBackBufferRTV();
    void Render();
    void OnResize(UINT newWidth, UINT newHeight);
    void Cleanup();

private:
    HWND m_hWnd = nullptr;

    UINT m_width = 1280;
    UINT m_height = 720;

    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pDeviceContext = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;
    ID3D11RenderTargetView* m_pBackBufferRTV = nullptr;

    IDXGIFactory* m_pFactory = nullptr;
    IDXGIAdapter* m_pSelectedAdapter = nullptr;
};
