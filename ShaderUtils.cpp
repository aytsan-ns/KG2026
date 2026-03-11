#include "ShaderUtils.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

static std::wstring Extension(const std::wstring& path)
{
    size_t p = path.find_last_of(L'.');
    if (p == std::wstring::npos) return L"";
    return path.substr(p + 1);
}

static std::string WCSToMBS(const std::wstring& w)
{
    if (w.empty()) return {};

    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};

    std::string s(len, '\0');

    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], len, nullptr, nullptr);

    if (!s.empty() && s.back() == '\0')
        s.pop_back();

    return s;
}

static bool ReadFileBytes(const wchar_t* path, std::vector<unsigned char>& outData)
{
    FILE* pFile = nullptr;
    _wfopen_s(&pFile, path, L"rb");
    if (!pFile)
        return false;

    fseek(pFile, 0, SEEK_END);
    long sz = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    if (sz <= 0)
    {
        fclose(pFile);
        return false;
    }

    outData.resize((size_t)sz);
    fread(outData.data(), 1, (size_t)sz, pFile);
    fclose(pFile);
    return true;
}

#pragma pack(push, 1)
struct DDS_PIXELFORMAT
{
    UINT32 size;
    UINT32 flags;
    UINT32 fourCC;
    UINT32 RGBBitCount;
    UINT32 RBitMask;
    UINT32 GBitMask;
    UINT32 BBitMask;
    UINT32 ABitMask;
};

struct DDS_HEADER
{
    UINT32 size;
    UINT32 flags;
    UINT32 height;
    UINT32 width;
    UINT32 pitchOrLinearSize;
    UINT32 depth;
    UINT32 mipMapCount;
    UINT32 reserved1[11];
    DDS_PIXELFORMAT ddspf;
    UINT32 caps;
    UINT32 caps2;
    UINT32 caps3;
    UINT32 caps4;
    UINT32 reserved2;
};
#pragma pack(pop)

static UINT32 MakeFourCC(char a, char b, char c, char d)
{
    return ((UINT32)(UINT8)a)
        | (((UINT32)(UINT8)b) << 8)
        | (((UINT32)(UINT8)c) << 16)
        | (((UINT32)(UINT8)d) << 24);
}

UINT32 DivUp(UINT32 a, UINT32 b)
{
    return (a + b - 1u) / b;
}

bool IsBlockCompressed(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC3_UNORM:
        return true;
    default:
        return false;
    }
}

UINT32 GetBytesPerBlock(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_UNORM:
        return 8;
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC3_UNORM:
        return 16;
    default:
        return 0;
    }
}

static DXGI_FORMAT DDSFormatFromFourCC(UINT32 fourCC)
{
    if (fourCC == MakeFourCC('D', 'X', 'T', '1')) return DXGI_FORMAT_BC1_UNORM;
    if (fourCC == MakeFourCC('D', 'X', 'T', '3')) return DXGI_FORMAT_BC2_UNORM;
    if (fourCC == MakeFourCC('D', 'X', 'T', '5')) return DXGI_FORMAT_BC3_UNORM;
    return DXGI_FORMAT_UNKNOWN;
}

bool LoadDDS(const wchar_t* path, TextureDesc& outDesc, bool forceSingleMip)
{
    outDesc = {};

    std::vector<unsigned char> fileData;
    if (!ReadFileBytes(path, fileData))
        return false;

    if (fileData.size() < 4 + sizeof(DDS_HEADER))
        return false;

    if (std::memcmp(fileData.data(), "DDS ", 4) != 0)
        return false;

    DDS_HEADER header = {};
    std::memcpy(&header, fileData.data() + 4, sizeof(header));

    if (header.size != 124 || header.ddspf.size != 32)
        return false;

    if (header.ddspf.fourCC == MakeFourCC('D', 'X', '1', '0'))
    {
        return false;
    }

    DXGI_FORMAT fmt = DDSFormatFromFourCC(header.ddspf.fourCC);
    if (fmt == DXGI_FORMAT_UNKNOWN)
        return false;

    UINT32 mipLevelsInFile = std::max<UINT32>(1u, header.mipMapCount);
    UINT32 mipLevelsToLoad = forceSingleMip ? 1u : mipLevelsInFile;

    const size_t dataOffset = 4 + sizeof(DDS_HEADER);
    if (fileData.size() <= dataOffset)
        return false;

    const unsigned char* src = fileData.data() + dataOffset;
    const size_t srcSize = fileData.size() - dataOffset;

    UINT32 width = std::max<UINT32>(1u, header.width);
    UINT32 height = std::max<UINT32>(1u, header.height);

    size_t totalSize = 0;
    UINT32 w = width;
    UINT32 h = height;

    for (UINT32 mip = 0; mip < mipLevelsToLoad; ++mip)
    {
        UINT32 blockWidth = DivUp(std::max(1u, w), 4u);
        UINT32 blockHeight = DivUp(std::max(1u, h), 4u);
        size_t mipSize = (size_t)blockWidth * (size_t)blockHeight * (size_t)GetBytesPerBlock(fmt);
        totalSize += mipSize;

        w = std::max(1u, w / 2u);
        h = std::max(1u, h / 2u);
    }

    if (srcSize < totalSize)
        return false;

    outDesc.fmt = fmt;
    outDesc.width = width;
    outDesc.height = height;
    outDesc.mipLevels = mipLevelsToLoad;
    outDesc.bytes.assign(src, src + totalSize);
    outDesc.pData = outDesc.bytes.data();

    return true;
}

bool CompileShaderFromFile(const std::wstring& path, ID3DBlob** outCode)
{
    *outCode = nullptr;

    FILE* pFile = nullptr;
    _wfopen_s(&pFile, path.c_str(), L"rb");
    if (!pFile) return false;

    fseek(pFile, 0, SEEK_END);
    long sz = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    std::vector<char> data(sz);
    fread(data.data(), 1, sz, pFile);
    fclose(pFile);

    std::wstring ext = Extension(path);
    std::string entryPoint;
    std::string platform;

    if (ext == L"vs")
    {
        entryPoint = "vs";
        platform = "vs_5_0";
    }
    else if (ext == L"ps")
    {
        entryPoint = "ps";
        platform = "ps_5_0";
    }
    else
    {
        return false;
    }

    UINT flags1 = 0;
#ifdef _DEBUG
    flags1 |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrMsg = nullptr;

    HRESULT result = D3DCompile(
        data.data(), data.size(),
        WCSToMBS(path).c_str(),
        nullptr, nullptr,
        entryPoint.c_str(),
        platform.c_str(),
        flags1, 0,
        outCode, &pErrMsg);

    if (!SUCCEEDED(result) && pErrMsg != nullptr)
    {
        OutputDebugStringA((const char*)pErrMsg->GetBufferPointer());
    }

    SAFE_RELEASE(pErrMsg);
    assert(SUCCEEDED(result));
    return SUCCEEDED(result);
}