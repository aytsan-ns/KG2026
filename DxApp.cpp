#include "DxApp.h"

bool DxApp::Init(HINSTANCE hInstance)
{
    if (!InitWindow(hInstance))
        return false;

    if (!InitDirectX())
        return false;

    return true;
}

bool DxApp::InitWindow(HINSTANCE hInstance)
{
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = DxApp::WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = _T("DX11WindowClass");

    if (!RegisterClassEx(&wcex))
        return false;

    m_hWnd = CreateWindow(
        wcex.lpszClassName,
        _T("Задание 1 | Смирнова Анастасия"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        nullptr, nullptr,
        hInstance,
        this);

    if (!m_hWnd)
        return false;

    {
        RECT rc;
        rc.left = 0;
        rc.right = (LONG)m_width;
        rc.top = 0;
        rc.bottom = (LONG)m_height;
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, TRUE);
        MoveWindow(m_hWnd, 100, 100, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    }

    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);

    return true;
}

bool DxApp::InitDirectX()
{
    HRESULT result = S_OK;

    result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&m_pFactory);
    if (FAILED(result))
        return false;

    {
        IDXGIAdapter* pAdapter = nullptr;
        UINT adapterIdx = 0;
        while (SUCCEEDED(m_pFactory->EnumAdapters(adapterIdx, &pAdapter)))
        {
            DXGI_ADAPTER_DESC desc;
            pAdapter->GetDesc(&desc);

            if (wcscmp(desc.Description, L"Microsoft Basic Render Driver") != 0)
            {
                m_pSelectedAdapter = pAdapter;
                break;
            }

            pAdapter->Release();
            pAdapter = nullptr;
            adapterIdx++;
        }
    }
    assert(m_pSelectedAdapter != nullptr);

    D3D_FEATURE_LEVEL level;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    result = D3D11CreateDevice(
        m_pSelectedAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        NULL,
        flags,
        levels, 1,
        D3D11_SDK_VERSION,
        &m_pDevice,
        &level,
        &m_pDeviceContext);

    assert(level == D3D_FEATURE_LEVEL_11_0);
    assert(SUCCEEDED(result));
    if (FAILED(result))
        return false;

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = m_width;
    swapChainDesc.BufferDesc.Height = m_height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = m_hWnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = true;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = 0;

    result = m_pFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
    assert(SUCCEEDED(result));
    if (FAILED(result))
        return false;

    CreateBackBufferRTV();
    return true;
}

void DxApp::CreateBackBufferRTV()
{
    assert(m_pSwapChain != nullptr);
    assert(m_pDevice != nullptr);

    ID3D11Texture2D* pBackBuffer = NULL;
    HRESULT result = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    assert(SUCCEEDED(result));

    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pBackBufferRTV);
        assert(SUCCEEDED(result));
        SAFE_RELEASE(pBackBuffer);
    }
}

void DxApp::Render()
{
    m_pDeviceContext->ClearState();

    ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, nullptr);

    static const FLOAT BackColor[4] = { 0.90f, 0.85f, 0.95f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pBackBufferRTV, BackColor);

    HRESULT result = m_pSwapChain->Present(0, 0);
    assert(SUCCEEDED(result));
}

void DxApp::OnResize(UINT newWidth, UINT newHeight)
{
    if (!m_pSwapChain || !m_pDevice || !m_pDeviceContext)
        return;

    if (newWidth == 0 || newHeight == 0)
        return;

    m_width = newWidth;
    m_height = newHeight;

    m_pDeviceContext->ClearState();
    SAFE_RELEASE(m_pBackBufferRTV);

    HRESULT result = m_pSwapChain->ResizeBuffers(
        2,
        m_width, m_height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0);
    assert(SUCCEEDED(result));

    CreateBackBufferRTV();
}

void DxApp::Cleanup()
{
    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pSwapChain);

    SAFE_RELEASE(m_pDeviceContext);

#ifdef _DEBUG
    if (m_pDevice)
    {
        ID3D11Debug* d3dDebug = nullptr;
        if (SUCCEEDED(m_pDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug)))
        {
            d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
            d3dDebug->Release();
        }
    }
#endif

    SAFE_RELEASE(m_pDevice);

    SAFE_RELEASE(m_pSelectedAdapter);
    SAFE_RELEASE(m_pFactory);
}

int DxApp::Run()
{
    MSG msg = {};
    bool exit = false;

    while (!exit)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
                exit = true;
        }

        if (!exit)
            Render();
    }

    Cleanup();
    return (int)msg.wParam;
}

LRESULT CALLBACK DxApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    DxApp* app = nullptr;

    if (message == WM_NCCREATE)
    {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        app = (DxApp*)cs->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)app);
    }
    else
    {
        app = (DxApp*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    switch (message)
    {
    case WM_SIZE:
        if (app)
        {
            UINT newWidth = (UINT)LOWORD(lParam);
            UINT newHeight = (UINT)HIWORD(lParam);
            app->OnResize(newWidth, newHeight);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}
