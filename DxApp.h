#pragma once
#include "DxCommon.h"

struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;
struct ID3D11Texture2D;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilView;
struct ID3D11DepthStencilState;
struct ID3D11BlendState;

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

    bool InitScene();

    bool CreateCubeGeometry();
    bool CreateSkyboxGeometry();

    bool CreateCubeShadersAndLayout();
    bool CreateSkyboxShadersAndLayout();
    bool CreateTransparentShader();

    bool CreateConstantBuffers();
    bool CreateCubeTexture();
    bool CreateCubeNormalMapTexture();
    bool CreateCubemapTexture();
    bool CreateSampler();

    bool CreateDepthBuffer();
    bool CreateDepthStates();
    bool CreateBlendStates();

private:
    HWND m_hWnd = nullptr;

    UINT m_width = 1280;
    UINT m_height = 720;

    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pDeviceContext = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;
    ID3D11RenderTargetView* m_pBackBufferRTV = nullptr;
    ID3D11Texture2D* m_pDepthBuffer = nullptr;
    ID3D11DepthStencilView* m_pDepthBufferDSV = nullptr;

    IDXGIFactory* m_pFactory = nullptr;
    IDXGIAdapter* m_pSelectedAdapter = nullptr;

    ID3D11Buffer* m_pCubeVertexBuffer = nullptr;
    ID3D11Buffer* m_pCubeIndexBuffer = nullptr;
    ID3D11VertexShader* m_pCubeVertexShader = nullptr;
    ID3D11PixelShader* m_pCubePixelShader = nullptr;
    ID3D11PixelShader* m_pTransparentPixelShader = nullptr;
    ID3D11InputLayout* m_pCubeInputLayout = nullptr;
    UINT m_cubeIndexCount = 0;

    ID3D11Buffer* m_pSkyVertexBuffer = nullptr;
    ID3D11Buffer* m_pSkyIndexBuffer = nullptr;
    ID3D11VertexShader* m_pSkyVertexShader = nullptr;
    ID3D11PixelShader* m_pSkyPixelShader = nullptr;
    ID3D11InputLayout* m_pSkyInputLayout = nullptr;
    UINT m_skyIndexCount = 0;

    ID3D11Buffer* m_pObjectBuffer = nullptr;
    ID3D11Buffer* m_pSceneBuffer = nullptr;

    ID3D11Texture2D* m_pCubeTexture = nullptr;
    ID3D11ShaderResourceView* m_pCubeTextureView = nullptr;

    ID3D11Texture2D* m_pCubeNormalMapTexture = nullptr;
    ID3D11ShaderResourceView* m_pCubeNormalMapView = nullptr;

    ID3D11Texture2D* m_pCubemapTexture = nullptr;
    ID3D11ShaderResourceView* m_pCubemapView = nullptr;

    ID3D11SamplerState* m_pSampler = nullptr;
    ID3D11RasterizerState* m_pSkyRasterizerState = nullptr;
    ID3D11DepthStencilState* m_pOpaqueDepthState = nullptr;
    ID3D11DepthStencilState* m_pTransDepthState = nullptr;
    ID3D11DepthStencilState* m_pSkyDepthState = nullptr;
    ID3D11BlendState* m_pTransBlendState = nullptr;

    bool  m_mouseDown = false;
    POINT m_lastMouse = { 0, 0 };
    float m_camYaw = 0.0f;
    float m_camPitch = 0.0f;
    float m_camDist = 3.0f;
};