#include "DxApp.h"
#include "ShaderUtils.h"
#include <DirectXMath.h>
#include <windowsx.h>
#include <cmath>
#include <algorithm>
#include <cfloat>

using namespace DirectX;

struct CubeVertex
{
    float x, y, z;
    float tx, ty, tz;
    float nx, ny, nz;
    float u, v;
};

struct SkyVertex
{
    float x, y, z;
};

struct Light
{
    DirectX::XMFLOAT4 pos;
    DirectX::XMFLOAT4 color;
};

struct ObjectBuffer
{
    DirectX::XMFLOAT4X4 model;
    DirectX::XMFLOAT4X4 normalModel;
    DirectX::XMFLOAT4 size;
    DirectX::XMFLOAT4 material;
    DirectX::XMFLOAT4 color;
};

struct SceneBuffer
{
    DirectX::XMFLOAT4X4 vp;
    DirectX::XMFLOAT4 cameraPos;
    DirectX::XMINT4 lightCount;
    Light lights[10];
    DirectX::XMFLOAT4 ambientColor;
};

struct TransparentCubeInstance
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
    float scale;
    float sortKey;
};

static const UINT MaxInst = 100;

struct InstancedGeom
{
    DirectX::XMFLOAT4X4 model;
    DirectX::XMFLOAT4X4 normalModel;
    DirectX::XMFLOAT4 shineSpeedTexIdNM; // x - shininess, y - speed, z - texture id, w - normal map flag
    DirectX::XMFLOAT4 posAngle;          // xyz - position, w - angle
};

struct GeomBufferInstData
{
    InstancedGeom geomBuffer[MaxInst];
};

struct VisibleIdsBuffer
{
    DirectX::XMUINT4 ids[MaxInst];
};

struct OpaqueCubeInstance
{
    DirectX::XMFLOAT3 position;
    float scale;
    float shininess;
    float textureId;
    float hasNormalMap;
};

static void MakeCubeBounds(const DirectX::XMFLOAT3& position, float scale,
    DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax)
{
    const float h = 0.5f * scale;
    outMin = DirectX::XMFLOAT3(position.x - h, position.y - h, position.z - h);
    outMax = DirectX::XMFLOAT3(position.x + h, position.y + h, position.z + h);
}

static float EvaluatePlane(const DirectX::XMFLOAT4& plane, const DirectX::XMFLOAT3& p)
{
    return plane.x * p.x + plane.y * p.y + plane.z * p.z + plane.w;
}

static DirectX::XMFLOAT4 BuildPlane(
    const DirectX::XMFLOAT3& p0,
    const DirectX::XMFLOAT3& p1,
    const DirectX::XMFLOAT3& p2,
    const DirectX::XMFLOAT3& p3)
{
    using namespace DirectX;

    XMVECTOR v0 = XMLoadFloat3(&p0);
    XMVECTOR v1 = XMLoadFloat3(&p1);
    XMVECTOR v3 = XMLoadFloat3(&p3);

    XMVECTOR norm = XMVector3Normalize(XMVector3Cross(v1 - v0, v3 - v0));

    XMVECTOR avg =
        (XMLoadFloat3(&p0) +
            XMLoadFloat3(&p1) +
            XMLoadFloat3(&p2) +
            XMLoadFloat3(&p3)) * 0.25f;

    XMFLOAT3 n;
    XMStoreFloat3(&n, norm);

    float d = -XMVectorGetX(XMVector3Dot(avg, norm));
    return XMFLOAT4(n.x, n.y, n.z, d);
}

static void FlipPlaneIfNeeded(DirectX::XMFLOAT4& plane, const DirectX::XMFLOAT3& insidePoint)
{
    if (EvaluatePlane(plane, insidePoint) < 0.0f)
    {
        plane.x = -plane.x;
        plane.y = -plane.y;
        plane.z = -plane.z;
        plane.w = -plane.w;
    }
}

static void BuildViewFrustum(
    const DirectX::XMFLOAT3& camPos,
    const DirectX::XMFLOAT3& camRight,
    const DirectX::XMFLOAT3& camUp,
    const DirectX::XMFLOAT3& camForward,
    float nearDist,
    float farDist,
    float fov,
    float aspectRatio,
    DirectX::XMFLOAT4 outFrustum[6])
{
    using namespace DirectX;

    const float nearHalfW = tanf(fov * 0.5f) * nearDist;
    const float nearHalfH = nearHalfW * aspectRatio;
    const float farHalfW = tanf(fov * 0.5f) * farDist;
    const float farHalfH = farHalfW * aspectRatio;

    XMVECTOR p = XMLoadFloat3(&camPos);
    XMVECTOR r = XMVector3Normalize(XMLoadFloat3(&camRight));
    XMVECTOR u = XMVector3Normalize(XMLoadFloat3(&camUp));
    XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&camForward));

    XMVECTOR nearCenter = p + f * nearDist;
    XMVECTOR farCenter = p + f * farDist;

    XMVECTOR ntl = nearCenter + u * nearHalfH - r * nearHalfW;
    XMVECTOR ntr = nearCenter + u * nearHalfH + r * nearHalfW;
    XMVECTOR nbl = nearCenter - u * nearHalfH - r * nearHalfW;
    XMVECTOR nbr = nearCenter - u * nearHalfH + r * nearHalfW;

    XMVECTOR ftl = farCenter + u * farHalfH - r * farHalfW;
    XMVECTOR ftr = farCenter + u * farHalfH + r * farHalfW;
    XMVECTOR fbl = farCenter - u * farHalfH - r * farHalfW;
    XMVECTOR fbr = farCenter - u * farHalfH + r * farHalfW;

    XMFLOAT3 Pntl, Pntr, Pnbl, Pnbr, Pftl, Pftr, Pfbl, Pfbr;
    XMStoreFloat3(&Pntl, ntl);
    XMStoreFloat3(&Pntr, ntr);
    XMStoreFloat3(&Pnbl, nbl);
    XMStoreFloat3(&Pnbr, nbr);
    XMStoreFloat3(&Pftl, ftl);
    XMStoreFloat3(&Pftr, ftr);
    XMStoreFloat3(&Pfbl, fbl);
    XMStoreFloat3(&Pfbr, fbr);

    XMFLOAT3 insidePoint;
    XMStoreFloat3(&insidePoint, (nearCenter + farCenter) * 0.5f);

    outFrustum[0] = BuildPlane(Pnbl, Pnbr, Pntr, Pntl); // near
    outFrustum[1] = BuildPlane(Pfbl, Pftl, Pftr, Pfbr); // far
    outFrustum[2] = BuildPlane(Pnbl, Pntl, Pftl, Pfbl); // left
    outFrustum[3] = BuildPlane(Pnbr, Pfbr, Pftr, Pntr); // right
    outFrustum[4] = BuildPlane(Pntl, Pntr, Pftr, Pftl); // top
    outFrustum[5] = BuildPlane(Pnbl, Pfbl, Pfbr, Pnbr); // bottom

    for (int i = 0; i < 6; ++i)
        FlipPlaneIfNeeded(outFrustum[i], insidePoint);
}

static bool IsBoxInside(const DirectX::XMFLOAT4 frustum[6],
    const DirectX::XMFLOAT3& bbMin,
    const DirectX::XMFLOAT3& bbMax)
{
    for (int i = 0; i < 6; ++i)
    {
        const DirectX::XMFLOAT4& plane = frustum[i];

        const float px = std::signbit(plane.x) ? bbMin.x : bbMax.x;
        const float py = std::signbit(plane.y) ? bbMin.y : bbMax.y;
        const float pz = std::signbit(plane.z) ? bbMin.z : bbMax.z;

        const float s = plane.x * px + plane.y * py + plane.z * pz + plane.w;
        if (s < 0.0f)
            return false;
    }

    return true;
}

static float ComputeSkySphereRadius(float nearPlane, float viewWidth, float viewHeight)
{
    const float halfW = viewWidth * 0.5f;
    const float halfH = viewHeight * 0.5f;
    const float r = sqrtf(nearPlane * nearPlane + halfW * halfW + halfH * halfH);
    return r * 1.05f;
}

static DirectX::XMMATRIX MakeCubeModel(const DirectX::XMFLOAT3& position, float scale)
{
    return DirectX::XMMatrixScaling(scale, scale, scale) *
        DirectX::XMMatrixTranslation(position.x, position.y, position.z);
}

static DirectX::XMMATRIX MakeNormalMatrix(DirectX::FXMMATRIX model)
{
    return DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, model));
}

static float ComputeTransparentSortKey(const DirectX::XMFLOAT3& cameraPos,
    const DirectX::XMFLOAT3& cubePos,
    float scale)
{
    const float h = 0.5f * scale;

    const DirectX::XMFLOAT3 corners[8] =
    {
        { -h, -h, -h }, { -h, -h,  h },
        { -h,  h, -h }, { -h,  h,  h },
        {  h, -h, -h }, {  h, -h,  h },
        {  h,  h, -h }, {  h,  h,  h }
    };

    float maxDistSq = 0.0f;

    for (const auto& corner : corners)
    {
        const float x = cubePos.x + corner.x;
        const float y = cubePos.y + corner.y;
        const float z = cubePos.z + corner.z;

        const float dx = x - cameraPos.x;
        const float dy = y - cameraPos.y;
        const float dz = z - cameraPos.z;

        maxDistSq = std::max(maxDistSq, dx * dx + dy * dy + dz * dz);
    }

    return maxDistSq;
}

bool DxApp::InitScene()
{
    if (!CreateCubeGeometry())
        return false;

    if (!CreateSkyboxGeometry())
        return false;

    if (!CreateCubeShadersAndLayout())
        return false;

    if (!CreateSkyboxShadersAndLayout())
        return false;

    if (!CreateTransparentShader())
        return false;

    if (!CreatePostProcessShaders())
        return false;

    if (!CreateConstantBuffers())
        return false;

    if (!CreateCubeTexture())
        return false;

    if (!CreateCubeNormalMapTexture())
        return false;

    if (!CreateCubemapTexture())
        return false;

    if (!CreateSampler())
        return false;

    if (!CreateDepthBuffer())
        return false;

    if (!CreateColorBuffer())
        return false;

    if (!CreateDepthStates())
        return false;

    if (!CreateBlendStates())
        return false;

    {
        D3D11_RASTERIZER_DESC rsDesc = {};
        rsDesc.FillMode = D3D11_FILL_SOLID;
        rsDesc.CullMode = D3D11_CULL_NONE;
        rsDesc.FrontCounterClockwise = FALSE;
        rsDesc.DepthClipEnable = TRUE;

        HRESULT result = m_pDevice->CreateRasterizerState(&rsDesc, &m_pSkyRasterizerState);
        if (FAILED(result))
            return false;

        SetResourceName(m_pSkyRasterizerState, "SkyRasterizerState");
    }

    return true;
}

bool DxApp::CreateConstantBuffers()
{
    HRESULT result;

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(ObjectBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;

        result = m_pDevice->CreateBuffer(&desc, nullptr, &m_pObjectBuffer);
        assert(SUCCEEDED(result));
        if (FAILED(result)) return false;

        SetResourceName(m_pObjectBuffer, "ObjectBuffer");
    }

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(SceneBuffer);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        result = m_pDevice->CreateBuffer(&desc, nullptr, &m_pSceneBuffer);
        assert(SUCCEEDED(result));
        if (FAILED(result)) return false;

        SetResourceName(m_pSceneBuffer, "SceneBuffer");
    }

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(GeomBufferInstData);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;

        result = m_pDevice->CreateBuffer(&desc, nullptr, &m_pGeomBufferInst);
        assert(SUCCEEDED(result));
        if (FAILED(result)) return false;

        SetResourceName(m_pGeomBufferInst, "GeomBufferInst");
    }

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(VisibleIdsBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;

        result = m_pDevice->CreateBuffer(&desc, nullptr, &m_pGeomBufferInstVis);
        assert(SUCCEEDED(result));
        if (FAILED(result)) return false;

        SetResourceName(m_pGeomBufferInstVis, "GeomBufferInstVis");
    }

    return true;
}

bool DxApp::CreateCubeGeometry()
{
    static const CubeVertex Vertices[] =
    {
        // Front (-Z)
        { -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f, -1.0f,   0.0f, 1.0f },
        { -0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f, -1.0f,   0.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f, -1.0f,   1.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f, -1.0f,   1.0f, 1.0f },

        // Back (+Z)
        {  0.5f, -0.5f,  0.5f,  -1.0f, 0.0f, 0.0f,   0.0f, 0.0f,  1.0f,   0.0f, 1.0f },
        {  0.5f,  0.5f,  0.5f,  -1.0f, 0.0f, 0.0f,   0.0f, 0.0f,  1.0f,   0.0f, 0.0f },
        { -0.5f,  0.5f,  0.5f,  -1.0f, 0.0f, 0.0f,   0.0f, 0.0f,  1.0f,   1.0f, 0.0f },
        { -0.5f, -0.5f,  0.5f,  -1.0f, 0.0f, 0.0f,   0.0f, 0.0f,  1.0f,   1.0f, 1.0f },

        // Left (-X)
        { -0.5f, -0.5f,  0.5f,   0.0f, 0.0f,-1.0f,  -1.0f, 0.0f,  0.0f,   0.0f, 1.0f },
        { -0.5f,  0.5f,  0.5f,   0.0f, 0.0f,-1.0f,  -1.0f, 0.0f,  0.0f,   0.0f, 0.0f },
        { -0.5f,  0.5f, -0.5f,   0.0f, 0.0f,-1.0f,  -1.0f, 0.0f,  0.0f,   1.0f, 0.0f },
        { -0.5f, -0.5f, -0.5f,   0.0f, 0.0f,-1.0f,  -1.0f, 0.0f,  0.0f,   1.0f, 1.0f },

        // Right (+X)
        {  0.5f, -0.5f, -0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,  0.0f,   0.0f, 1.0f },
        {  0.5f,  0.5f, -0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,  0.0f,   0.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,  0.0f,   1.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,  0.0f,   1.0f, 1.0f },

        // Top (+Y)
        { -0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f,  0.0f,   0.0f, 1.0f },
        { -0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f,  0.0f,   0.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f,  0.0f,   1.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f,  0.0f,   1.0f, 1.0f },

        // Bottom (-Y)
        { -0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 0.0f,   0.0f,-1.0f,  0.0f,   0.0f, 1.0f },
        { -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   0.0f,-1.0f,  0.0f,   0.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   0.0f,-1.0f,  0.0f,   1.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 0.0f,   0.0f,-1.0f,  0.0f,   1.0f, 1.0f },
    };

    static const USHORT Indices[] =
    {
        0, 1, 2,   0, 2, 3,
        4, 5, 6,   4, 6, 7,
        8, 9, 10,  8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23
    };

    m_cubeIndexCount = (UINT)(sizeof(Indices) / sizeof(Indices[0]));

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(Vertices);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = Vertices;

    HRESULT result = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pCubeVertexBuffer);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pCubeVertexBuffer, "CubeVertexBuffer");

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(Indices);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = Indices;

    result = m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pCubeIndexBuffer);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pCubeIndexBuffer, "CubeIndexBuffer");
    return true;
}

bool DxApp::CreateSkyboxGeometry()
{
    const UINT stacks = 16;
    const UINT slices = 32;

    std::vector<SkyVertex> vertices;
    std::vector<USHORT> indices;

    for (UINT stack = 0; stack <= stacks; ++stack)
    {
        float v = (float)stack / (float)stacks;
        float phi = XM_PI * v;

        float y = cosf(phi);
        float r = sinf(phi);

        for (UINT slice = 0; slice <= slices; ++slice)
        {
            float u = (float)slice / (float)slices;
            float theta = XM_2PI * u;

            float x = r * cosf(theta);
            float z = r * sinf(theta);

            vertices.push_back({ x, y, z });
        }
    }

    for (UINT stack = 0; stack < stacks; ++stack)
    {
        for (UINT slice = 0; slice < slices; ++slice)
        {
            USHORT a = (USHORT)(stack * (slices + 1) + slice);
            USHORT b = (USHORT)(a + 1);
            USHORT c = (USHORT)((stack + 1) * (slices + 1) + slice);
            USHORT d = (USHORT)(c + 1);

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);

            indices.push_back(b);
            indices.push_back(d);
            indices.push_back(c);
        }
    }

    m_skyIndexCount = (UINT)indices.size();

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = (UINT)(vertices.size() * sizeof(SkyVertex));
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    HRESULT result = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pSkyVertexBuffer);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pSkyVertexBuffer, "SkyVertexBuffer");

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = (UINT)(indices.size() * sizeof(USHORT));
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    result = m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pSkyIndexBuffer);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pSkyIndexBuffer, "SkyIndexBuffer");
    return true;
}

bool DxApp::CreateCubeShadersAndLayout()
{
    ID3DBlob* vsCode = nullptr;
    ID3DBlob* psCode = nullptr;

    if (!CompileShaderFromFile(L"Cube.vs", &vsCode))
        return false;

    HRESULT result = m_pDevice->CreateVertexShader(
        vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
        nullptr, &m_pCubeVertexShader);
    assert(SUCCEEDED(result));
    if (FAILED(result)) { SAFE_RELEASE(vsCode); return false; }

    SetResourceName(m_pCubeVertexShader, "Cube.vs");

    if (!CompileShaderFromFile(L"Cube.ps", &psCode))
    {
        SAFE_RELEASE(vsCode);
        return false;
    }

    result = m_pDevice->CreatePixelShader(
        psCode->GetBufferPointer(), psCode->GetBufferSize(),
        nullptr, &m_pCubePixelShader);
    assert(SUCCEEDED(result));
    if (FAILED(result)) { SAFE_RELEASE(vsCode); SAFE_RELEASE(psCode); return false; }

    SetResourceName(m_pCubePixelShader, "Cube.ps");

    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    result = m_pDevice->CreateInputLayout(
        InputDesc, 4,
        vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
        &m_pCubeInputLayout);

    SAFE_RELEASE(vsCode);
    SAFE_RELEASE(psCode);

    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pCubeInputLayout, "CubeInputLayout");
    return true;
}

bool DxApp::CreateSkyboxShadersAndLayout()
{
    ID3DBlob* vsCode = nullptr;
    ID3DBlob* psCode = nullptr;

    if (!CompileShaderFromFile(L"Skybox.vs", &vsCode))
        return false;

    HRESULT result = m_pDevice->CreateVertexShader(
        vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
        nullptr, &m_pSkyVertexShader);
    assert(SUCCEEDED(result));
    if (FAILED(result)) { SAFE_RELEASE(vsCode); return false; }

    SetResourceName(m_pSkyVertexShader, "Skybox.vs");

    if (!CompileShaderFromFile(L"Skybox.ps", &psCode))
    {
        SAFE_RELEASE(vsCode);
        return false;
    }

    result = m_pDevice->CreatePixelShader(
        psCode->GetBufferPointer(), psCode->GetBufferSize(),
        nullptr, &m_pSkyPixelShader);
    assert(SUCCEEDED(result));
    if (FAILED(result)) { SAFE_RELEASE(vsCode); SAFE_RELEASE(psCode); return false; }

    SetResourceName(m_pSkyPixelShader, "Skybox.ps");

    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    result = m_pDevice->CreateInputLayout(
        InputDesc, 1,
        vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
        &m_pSkyInputLayout);

    SAFE_RELEASE(vsCode);
    SAFE_RELEASE(psCode);

    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pSkyInputLayout, "SkyInputLayout");
    return true;
}

bool DxApp::CreateTransparentShader()
{
    ID3DBlob* psCode = nullptr;

    if (!CompileShaderFromFile(L"Transparent.ps", &psCode))
        return false;

    HRESULT result = m_pDevice->CreatePixelShader(
        psCode->GetBufferPointer(), psCode->GetBufferSize(),
        nullptr, &m_pTransparentPixelShader);

    SAFE_RELEASE(psCode);

    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pTransparentPixelShader, "Transparent.ps");
    return true;
}

bool DxApp::CreatePostProcessShaders()
{
    ID3DBlob* vsCode = nullptr;
    ID3DBlob* psCode = nullptr;

    if (!CompileShaderFromFile(L"Sepia.vs", &vsCode))
        return false;

    HRESULT result = m_pDevice->CreateVertexShader(
        vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
        nullptr, &m_pSepiaVertexShader);
    assert(SUCCEEDED(result));
    if (FAILED(result))
    {
        SAFE_RELEASE(vsCode);
        return false;
    }

    SetResourceName(m_pSepiaVertexShader, "Sepia.vs");

    if (!CompileShaderFromFile(L"Sepia.ps", &psCode))
    {
        SAFE_RELEASE(vsCode);
        return false;
    }

    result = m_pDevice->CreatePixelShader(
        psCode->GetBufferPointer(), psCode->GetBufferSize(),
        nullptr, &m_pSepiaPixelShader);
    assert(SUCCEEDED(result));
    if (FAILED(result))
    {
        SAFE_RELEASE(vsCode);
        SAFE_RELEASE(psCode);
        return false;
    }

    SetResourceName(m_pSepiaPixelShader, "Sepia.ps");

    SAFE_RELEASE(vsCode);
    SAFE_RELEASE(psCode);
    return true;
}

bool DxApp::CreateCubeTexture()
{
    TextureDesc textureDesc[2];

    if (!LoadDDS(L"kitty.dds", textureDesc[0], false))
        return false;

    if (!LoadDDS(L"cube.dds", textureDesc[1], false))
        return false;

    if (textureDesc[0].fmt != textureDesc[1].fmt ||
        textureDesc[0].width != textureDesc[1].width ||
        textureDesc[0].height != textureDesc[1].height ||
        textureDesc[0].mipLevels != textureDesc[1].mipLevels)
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = textureDesc[0].width;
    desc.Height = textureDesc[0].height;
    desc.MipLevels = textureDesc[0].mipLevels;
    desc.ArraySize = 2;
    desc.Format = textureDesc[0].fmt;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    std::vector<D3D11_SUBRESOURCE_DATA> data;
    data.resize(desc.MipLevels * desc.ArraySize);

    for (UINT32 j = 0; j < desc.ArraySize; ++j)
    {
        const unsigned char* pSrcData = reinterpret_cast<const unsigned char*>(textureDesc[j].pData);

        UINT32 width = desc.Width;
        UINT32 height = desc.Height;

        for (UINT32 i = 0; i < desc.MipLevels; ++i)
        {
            UINT32 blockWidth = DivUp(width, 4u);
            UINT32 blockHeight = DivUp(height, 4u);
            UINT32 pitch = blockWidth * GetBytesPerBlock(desc.Format);

            data[j * desc.MipLevels + i].pSysMem = pSrcData;
            data[j * desc.MipLevels + i].SysMemPitch = pitch;
            data[j * desc.MipLevels + i].SysMemSlicePitch = 0;

            pSrcData += pitch * blockHeight;

            width = std::max(1u, width / 2u);
            height = std::max(1u, height / 2u);
        }
    }

    HRESULT result = m_pDevice->CreateTexture2D(&desc, data.data(), &m_pCubeTexture);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pCubeTexture, "CubeTextureArray");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.ArraySize = desc.ArraySize;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
    srvDesc.Texture2DArray.MostDetailedMip = 0;

    result = m_pDevice->CreateShaderResourceView(m_pCubeTexture, &srvDesc, &m_pCubeTextureView);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pCubeTextureView, "CubeTextureArrayView");
    return true;
}

bool DxApp::CreateCubeNormalMapTexture()
{
    TextureDesc textureDesc;
    if (!LoadDDS(L"cube_nm.dds", textureDesc, false))
        return false;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = textureDesc.width;
    desc.Height = textureDesc.height;
    desc.MipLevels = textureDesc.mipLevels;
    desc.ArraySize = 1;
    desc.Format = textureDesc.fmt;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    std::vector<D3D11_SUBRESOURCE_DATA> data;
    data.resize(textureDesc.mipLevels);

    const unsigned char* pSrcData = reinterpret_cast<const unsigned char*>(textureDesc.pData);

    UINT32 width = desc.Width;
    UINT32 height = desc.Height;

    for (UINT32 i = 0; i < desc.MipLevels; ++i)
    {
        UINT32 blockWidth = DivUp(width, 4u);
        UINT32 blockHeight = DivUp(height, 4u);
        UINT32 pitch = blockWidth * GetBytesPerBlock(desc.Format);

        data[i].pSysMem = pSrcData;
        data[i].SysMemPitch = pitch;
        data[i].SysMemSlicePitch = 0;

        pSrcData += pitch * blockHeight;

        width = std::max(1u, width / 2u);
        height = std::max(1u, height / 2u);
    }

    HRESULT result = m_pDevice->CreateTexture2D(&desc, data.data(), &m_pCubeNormalMapTexture);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pCubeNormalMapTexture, "cube_nm.dds");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    result = m_pDevice->CreateShaderResourceView(m_pCubeNormalMapTexture, &srvDesc, &m_pCubeNormalMapView);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pCubeNormalMapView, "CubeNormalMapView");
    return true;
}

bool DxApp::CreateCubemapTexture()
{
    const std::wstring faces[6] =
    {
        L"posx.dds", L"negx.dds",
        L"posy.dds", L"negy.dds",
        L"posz.dds", L"negz.dds"
    };

    TextureDesc texDescs[6];
    for (int i = 0; i < 6; ++i)
    {
        if (!LoadDDS(faces[i].c_str(), texDescs[i], true))
            return false;
    }

    for (int i = 1; i < 6; ++i)
    {
        if (texDescs[i].fmt != texDescs[0].fmt ||
            texDescs[i].width != texDescs[0].width ||
            texDescs[i].height != texDescs[0].height)
        {
            return false;
        }
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = texDescs[0].width;
    desc.Height = texDescs[0].height;
    desc.MipLevels = 1;
    desc.ArraySize = 6;
    desc.Format = texDescs[0].fmt;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    UINT32 blockWidth = DivUp(desc.Width, 4u);
    UINT32 blockHeight = DivUp(desc.Height, 4u);
    UINT32 pitch = blockWidth * GetBytesPerBlock(desc.Format);

    D3D11_SUBRESOURCE_DATA data[6] = {};
    for (int i = 0; i < 6; ++i)
    {
        data[i].pSysMem = texDescs[i].pData;
        data[i].SysMemPitch = pitch;
        data[i].SysMemSlicePitch = 0;
    }

    HRESULT result = m_pDevice->CreateTexture2D(&desc, data, &m_pCubemapTexture);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pCubemapTexture, "SkyCubemap");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.MostDetailedMip = 0;

    result = m_pDevice->CreateShaderResourceView(m_pCubemapTexture, &srvDesc, &m_pCubemapView);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pCubemapView, "CubemapView");
    return true;
}

bool DxApp::CreateSampler()
{
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_ANISOTROPIC;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.MinLOD = -FLT_MAX;
    desc.MaxLOD = FLT_MAX;
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = 16;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.BorderColor[0] = 1.0f;
    desc.BorderColor[1] = 1.0f;
    desc.BorderColor[2] = 1.0f;
    desc.BorderColor[3] = 1.0f;

    HRESULT result = m_pDevice->CreateSamplerState(&desc, &m_pSampler);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pSampler, "MainSampler");
    return true;
}

bool DxApp::CreateDepthBuffer()
{
    SAFE_RELEASE(m_pDepthBufferDSV);
    SAFE_RELEASE(m_pDepthBuffer);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    HRESULT result = m_pDevice->CreateTexture2D(&desc, nullptr, &m_pDepthBuffer);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pDepthBuffer, "DepthBuffer");

    result = m_pDevice->CreateDepthStencilView(m_pDepthBuffer, nullptr, &m_pDepthBufferDSV);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pDepthBufferDSV, "DepthBufferDSV");
    return true;
}

bool DxApp::CreateColorBuffer()
{
    SAFE_RELEASE(m_pColorBufferSRV);
    SAFE_RELEASE(m_pColorBufferRTV);
    SAFE_RELEASE(m_pColorBuffer);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    HRESULT result = m_pDevice->CreateTexture2D(&desc, nullptr, &m_pColorBuffer);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pColorBuffer, "ColorBuffer");

    result = m_pDevice->CreateRenderTargetView(m_pColorBuffer, nullptr, &m_pColorBufferRTV);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pColorBufferRTV, "ColorBufferRTV");

    result = m_pDevice->CreateShaderResourceView(m_pColorBuffer, nullptr, &m_pColorBufferSRV);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pColorBufferSRV, "ColorBufferSRV");
    return true;
}

bool DxApp::CreateDepthStates()
{
    HRESULT result;

    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_GREATER;
        desc.StencilEnable = FALSE;

        result = m_pDevice->CreateDepthStencilState(&desc, &m_pOpaqueDepthState);
        assert(SUCCEEDED(result));
        if (FAILED(result)) return false;

        SetResourceName(m_pOpaqueDepthState, "OpaqueDepthState");
    }

    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_GREATER;
        desc.StencilEnable = FALSE;

        result = m_pDevice->CreateDepthStencilState(&desc, &m_pTransDepthState);
        assert(SUCCEEDED(result));
        if (FAILED(result)) return false;

        SetResourceName(m_pTransDepthState, "TransDepthState");
    }

    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
        desc.StencilEnable = FALSE;

        result = m_pDevice->CreateDepthStencilState(&desc, &m_pSkyDepthState);
        assert(SUCCEEDED(result));
        if (FAILED(result)) return false;

        SetResourceName(m_pSkyDepthState, "SkyDepthState");
    }

    return true;
}

bool DxApp::CreateBlendStates()
{
    D3D11_BLEND_DESC desc = {};
    desc.AlphaToCoverageEnable = FALSE;
    desc.IndependentBlendEnable = FALSE;
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_RED |
        D3D11_COLOR_WRITE_ENABLE_GREEN |
        D3D11_COLOR_WRITE_ENABLE_BLUE;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;

    HRESULT result = m_pDevice->CreateBlendState(&desc, &m_pTransBlendState);
    assert(SUCCEEDED(result));
    if (FAILED(result)) return false;

    SetResourceName(m_pTransBlendState, "TransparentBlendState");
    return true;
}

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
        _T("Задание 7 | Смирнова Анастасия"),
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

    if (!InitScene())
        return false;

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

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)m_width;
    viewport.Height = (FLOAT)m_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &viewport);

    D3D11_RECT rect = {};
    rect.left = 0;
    rect.top = 0;
    rect.right = (LONG)m_width;
    rect.bottom = (LONG)m_height;
    m_pDeviceContext->RSSetScissorRects(1, &rect);

    DirectX::XMMATRIX cam = DirectX::XMMatrixTranslation(0.0f, 0.0f, -m_camDist);
    cam = DirectX::XMMatrixMultiply(cam, DirectX::XMMatrixRotationY(m_camYaw));
    cam = DirectX::XMMatrixMultiply(cam, DirectX::XMMatrixRotationX(m_camPitch));
    DirectX::XMMATRIX v = DirectX::XMMatrixInverse(nullptr, cam);

    const float f = 100.0f;
    const float n = 0.1f;
    const float fov = DirectX::XM_PI / 3.0f;
    const float aspectRatio = (float)m_height / (float)m_width;

    const float nearViewWidth = tanf(fov / 2.0f) * 2.0f * n;
    const float nearViewHeight = nearViewWidth * aspectRatio;

    const float farViewWidth = tanf(fov / 2.0f) * 2.0f * f;
    const float farViewHeight = farViewWidth * aspectRatio;

    DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveLH(farViewWidth, farViewHeight, f, n);
    DirectX::XMMATRIX vp = DirectX::XMMatrixMultiply(v, p);

    DirectX::XMVECTOR camPosV = DirectX::XMVector3TransformCoord(DirectX::XMVectorZero(), cam);
    DirectX::XMFLOAT3 camPos;
    DirectX::XMStoreFloat3(&camPos, camPosV);

    D3D11_MAPPED_SUBRESOURCE subresource = {};
    HRESULT mapRes = m_pDeviceContext->Map(m_pSceneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource);
    assert(SUCCEEDED(mapRes));
    if (SUCCEEDED(mapRes))
    {
        SceneBuffer& scene = *reinterpret_cast<SceneBuffer*>(subresource.pData);
        DirectX::XMStoreFloat4x4(&scene.vp, vp);
        scene.cameraPos = DirectX::XMFLOAT4(camPos.x, camPos.y, camPos.z, 1.0f);

        scene.lightCount = DirectX::XMINT4{ 2, 0, 0, 0 };

        scene.lights[0].pos = DirectX::XMFLOAT4(-1.8f, 1.5f, -0.7f, 1.0f);
        scene.lights[0].color = DirectX::XMFLOAT4(1.0f, 0.95f, 0.85f, 1.0f);

        scene.lights[1].pos = DirectX::XMFLOAT4(1.6f, 1.0f, 2.4f, 1.0f);
        scene.lights[1].color = DirectX::XMFLOAT4(0.55f, 0.70f, 1.0f, 1.0f);

        for (int i = 2; i < 10; ++i)
        {
            scene.lights[i].pos = DirectX::XMFLOAT4(0, 0, 0, 0);
            scene.lights[i].color = DirectX::XMFLOAT4(0, 0, 0, 0);
        }

        scene.ambientColor = DirectX::XMFLOAT4(0.12f, 0.12f, 0.14f, 1.0f);

        m_pDeviceContext->Unmap(m_pSceneBuffer, 0);
    }

    ID3D11SamplerState* samplers[] = { m_pSampler };
    m_pDeviceContext->PSSetSamplers(0, 1, samplers);

    
    {
        ID3D11RenderTargetView* views[] = { m_pColorBufferRTV };
        m_pDeviceContext->OMSetRenderTargets(1, views, m_pDepthBufferDSV);

        static const FLOAT BackColor[4] = { 0.90f, 0.85f, 0.95f, 1.0f };
        m_pDeviceContext->ClearRenderTargetView(m_pColorBufferRTV, BackColor);
        m_pDeviceContext->ClearDepthStencilView(m_pDepthBufferDSV, D3D11_CLEAR_DEPTH, 0.0f, 0);

        
        {
            std::vector<OpaqueCubeInstance> opaqueCubes =
            {
                { DirectX::XMFLOAT3(-0.90f, 0.00f, 0.10f), 1.0f, 32.0f, 0.0f, 1.0f },
                { DirectX::XMFLOAT3(0.90f, 0.00f, 1.20f), 1.0f, 32.0f, 1.0f, 1.0f },
                { DirectX::XMFLOAT3(-2.40f, 0.00f, 2.20f), 1.0f, 24.0f, 0.0f, 0.0f },
                { DirectX::XMFLOAT3(2.50f, 0.20f, 3.20f), 1.2f, 48.0f, 1.0f, 1.0f },
                { DirectX::XMFLOAT3(0.00f,-0.60f, 5.50f), 1.0f, 12.0f, 0.0f, 0.0f }
            };

            DirectX::XMFLOAT4X4 camM;
            DirectX::XMStoreFloat4x4(&camM, cam);

            DirectX::XMFLOAT3 camRight(camM._11, camM._12, camM._13);
            DirectX::XMFLOAT3 camUp(camM._21, camM._22, camM._23);
            DirectX::XMFLOAT3 camForward(camM._31, camM._32, camM._33);

            DirectX::XMFLOAT4 frustum[6];
            BuildViewFrustum(camPos, camRight, camUp, camForward, n, f, fov, aspectRatio, frustum);

            GeomBufferInstData geomData = {};
            VisibleIdsBuffer visData = {};

            const UINT cubeCount = (UINT)std::min<size_t>(opaqueCubes.size(), MaxInst);

            for (UINT i = 0; i < cubeCount; ++i)
            {
                const auto& cube = opaqueCubes[i];
                DirectX::XMMATRIX model = MakeCubeModel(cube.position, cube.scale);

                DirectX::XMStoreFloat4x4(&geomData.geomBuffer[i].model, model);
                DirectX::XMStoreFloat4x4(&geomData.geomBuffer[i].normalModel, MakeNormalMatrix(model));
                geomData.geomBuffer[i].shineSpeedTexIdNM = DirectX::XMFLOAT4(
                    cube.shininess,
                    0.0f,
                    cube.textureId,
                    cube.hasNormalMap);
                geomData.geomBuffer[i].posAngle = DirectX::XMFLOAT4(
                    cube.position.x,
                    cube.position.y,
                    cube.position.z,
                    0.0f);
            }

            UINT visibleCount = 0;
            for (UINT i = 0; i < cubeCount; ++i)
            {
                DirectX::XMFLOAT3 bbMin, bbMax;
                MakeCubeBounds(opaqueCubes[i].position, opaqueCubes[i].scale, bbMin, bbMax);

                if (IsBoxInside(frustum, bbMin, bbMax))
                {
                    visData.ids[visibleCount] = DirectX::XMUINT4(i, 0, 0, 0);
                    ++visibleCount;
                }
            }

            m_pDeviceContext->UpdateSubresource(m_pGeomBufferInst, 0, nullptr, &geomData, 0, 0);
            m_pDeviceContext->UpdateSubresource(m_pGeomBufferInstVis, 0, nullptr, &visData, 0, 0);

            if (visibleCount > 0)
            {
                UINT stride = sizeof(CubeVertex);
                UINT offset = 0;
                ID3D11Buffer* vbs[] = { m_pCubeVertexBuffer };
                ID3D11ShaderResourceView* resources[] = { m_pCubeTextureView, m_pCubeNormalMapView };
                ID3D11Buffer* cbs[] = { m_pSceneBuffer, m_pGeomBufferInst, m_pGeomBufferInstVis };

                m_pDeviceContext->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
                m_pDeviceContext->IASetIndexBuffer(m_pCubeIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
                m_pDeviceContext->IASetInputLayout(m_pCubeInputLayout);
                m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                m_pDeviceContext->VSSetShader(m_pCubeVertexShader, nullptr, 0);
                m_pDeviceContext->PSSetShader(m_pCubePixelShader, nullptr, 0);

                m_pDeviceContext->VSSetConstantBuffers(0, 3, cbs);
                m_pDeviceContext->PSSetConstantBuffers(0, 3, cbs);

                m_pDeviceContext->PSSetShaderResources(0, 2, resources);
                m_pDeviceContext->OMSetDepthStencilState(m_pOpaqueDepthState, 0);
                m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

                m_pDeviceContext->DrawIndexedInstanced(m_cubeIndexCount, visibleCount, 0, 0, 0);
            }
        }

        
        {
            UINT stride = sizeof(SkyVertex);
            UINT offset = 0;
            ID3D11Buffer* vbs[] = { m_pSkyVertexBuffer };
            ID3D11ShaderResourceView* resources[] = { m_pCubemapView };
            ID3D11Buffer* cbs[] = { m_pObjectBuffer, m_pSceneBuffer };

            m_pDeviceContext->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
            m_pDeviceContext->IASetIndexBuffer(m_pSkyIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
            m_pDeviceContext->IASetInputLayout(m_pSkyInputLayout);
            m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            m_pDeviceContext->VSSetShader(m_pSkyVertexShader, nullptr, 0);
            m_pDeviceContext->PSSetShader(m_pSkyPixelShader, nullptr, 0);

            m_pDeviceContext->VSSetConstantBuffers(0, 2, cbs);
            m_pDeviceContext->PSSetConstantBuffers(0, 2, cbs);

            m_pDeviceContext->RSSetState(m_pSkyRasterizerState);
            m_pDeviceContext->OMSetDepthStencilState(m_pSkyDepthState, 0);
            m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

            ObjectBuffer obj = {};
            DirectX::XMMATRIX skyModel = DirectX::XMMatrixIdentity();
            DirectX::XMStoreFloat4x4(&obj.model, skyModel);
            DirectX::XMStoreFloat4x4(&obj.normalModel, MakeNormalMatrix(skyModel));
            obj.size = DirectX::XMFLOAT4(
                ComputeSkySphereRadius(n, nearViewWidth, nearViewHeight),
                0.0f, 0.0f, 0.0f);
            obj.material = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
            obj.color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

            m_pDeviceContext->UpdateSubresource(m_pObjectBuffer, 0, nullptr, &obj, 0, 0);
            m_pDeviceContext->PSSetShaderResources(0, 1, resources);
            m_pDeviceContext->DrawIndexed(m_skyIndexCount, 0, 0);
        }

        m_pDeviceContext->RSSetState(nullptr);
    }

    
    {
        ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
        ID3D11ShaderResourceView* resources[] = { m_pColorBufferSRV };

        m_pDeviceContext->OMSetRenderTargets(1, views, nullptr);
        static const FLOAT BlackColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_pDeviceContext->ClearRenderTargetView(m_pBackBufferRTV, BlackColor);

        m_pDeviceContext->PSSetSamplers(0, 1, samplers);
        m_pDeviceContext->PSSetShaderResources(0, 1, resources);

        m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);
        m_pDeviceContext->RSSetState(nullptr);
        m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

        m_pDeviceContext->IASetInputLayout(nullptr);
        m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_pDeviceContext->VSSetShader(m_pSepiaVertexShader, nullptr, 0);
        m_pDeviceContext->PSSetShader(m_pSepiaPixelShader, nullptr, 0);

        m_pDeviceContext->Draw(6, 0);

        ID3D11ShaderResourceView* nullSRV[] = { nullptr };
        m_pDeviceContext->PSSetShaderResources(0, 1, nullSRV);
    }

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

    SAFE_RELEASE(m_pColorBufferSRV);
    SAFE_RELEASE(m_pColorBufferRTV);
    SAFE_RELEASE(m_pColorBuffer);

    SAFE_RELEASE(m_pDepthBufferDSV);
    SAFE_RELEASE(m_pDepthBuffer);
    SAFE_RELEASE(m_pBackBufferRTV);

    HRESULT result = m_pSwapChain->ResizeBuffers(
        2,
        m_width, m_height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0);
    assert(SUCCEEDED(result));

    CreateBackBufferRTV();

    bool depthOk = CreateDepthBuffer();
    assert(depthOk);
    (void)depthOk;

    bool colorOk = CreateColorBuffer();
    assert(colorOk);
    (void)colorOk;
}

void DxApp::Cleanup()
{
    if (m_pDeviceContext)
    {
        m_pDeviceContext->ClearState();
        m_pDeviceContext->Flush();
    }

    SAFE_RELEASE(m_pTransBlendState);
    SAFE_RELEASE(m_pSkyDepthState);
    SAFE_RELEASE(m_pTransDepthState);
    SAFE_RELEASE(m_pOpaqueDepthState);
    SAFE_RELEASE(m_pSkyRasterizerState);
    SAFE_RELEASE(m_pSampler);

    SAFE_RELEASE(m_pColorBufferSRV);
    SAFE_RELEASE(m_pColorBufferRTV);
    SAFE_RELEASE(m_pColorBuffer);

    SAFE_RELEASE(m_pCubemapView);
    SAFE_RELEASE(m_pCubemapTexture);

    SAFE_RELEASE(m_pCubeNormalMapView);
    SAFE_RELEASE(m_pCubeNormalMapTexture);

    SAFE_RELEASE(m_pCubeTextureView);
    SAFE_RELEASE(m_pCubeTexture);

    SAFE_RELEASE(m_pCubeInputLayout);
    SAFE_RELEASE(m_pCubeVertexShader);
    SAFE_RELEASE(m_pCubePixelShader);
    SAFE_RELEASE(m_pTransparentPixelShader);
    SAFE_RELEASE(m_pSepiaVertexShader);
    SAFE_RELEASE(m_pSepiaPixelShader);
    SAFE_RELEASE(m_pCubeVertexBuffer);
    SAFE_RELEASE(m_pCubeIndexBuffer);

    SAFE_RELEASE(m_pSkyInputLayout);
    SAFE_RELEASE(m_pSkyVertexShader);
    SAFE_RELEASE(m_pSkyPixelShader);
    SAFE_RELEASE(m_pSkyVertexBuffer);
    SAFE_RELEASE(m_pSkyIndexBuffer);

    SAFE_RELEASE(m_pGeomBufferInstVis);
    SAFE_RELEASE(m_pGeomBufferInst);
    SAFE_RELEASE(m_pObjectBuffer);
    SAFE_RELEASE(m_pSceneBuffer);

    SAFE_RELEASE(m_pDepthBufferDSV);
    SAFE_RELEASE(m_pDepthBuffer);

    SAFE_RELEASE(m_pDepthBufferDSV);
    SAFE_RELEASE(m_pDepthBuffer);
    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pSwapChain);

    SAFE_RELEASE(m_pDeviceContext);

#ifdef _DEBUG
    if (m_pDevice)
    {
        ID3D11Debug* d3dDebug = nullptr;
        if (SUCCEEDED(m_pDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug)))
        {
            UINT references = m_pDevice->Release();
            m_pDevice = nullptr;

            if (references > 1)
            {
                d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
            }

            d3dDebug->Release();
        }
        else
        {
            SAFE_RELEASE(m_pDevice);
        }
    }
#else
    SAFE_RELEASE(m_pDevice);
#endif

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

    case WM_LBUTTONDOWN:
        if (app)
        {
            app->m_mouseDown = true;
            app->m_lastMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            SetCapture(hWnd);
        }
        return 0;

    case WM_LBUTTONUP:
        if (app)
        {
            app->m_mouseDown = false;
            ReleaseCapture();
        }
        return 0;

    case WM_MOUSEMOVE:
        if (app && app->m_mouseDown)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            int dx = x - app->m_lastMouse.x;
            int dy = y - app->m_lastMouse.y;
            app->m_lastMouse = { x, y };

            const float sens = 0.01f;
            app->m_camYaw += dx * sens;
            app->m_camPitch += dy * sens;
        }
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}