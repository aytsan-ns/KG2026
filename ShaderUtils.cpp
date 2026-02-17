#include "ShaderUtils.h"

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
