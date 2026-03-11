#pragma once
#include "DxCommon.h"

struct TextureDesc
{
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    UINT32 width = 0;
    UINT32 height = 0;
    UINT32 mipLevels = 0;

    std::vector<unsigned char> bytes;
    const void* pData = nullptr;
};

bool CompileShaderFromFile(const std::wstring& path, ID3DBlob** outCode);

bool LoadDDS(const wchar_t* path, TextureDesc& outDesc, bool forceSingleMip = false);

UINT32 DivUp(UINT32 a, UINT32 b);
bool IsBlockCompressed(DXGI_FORMAT fmt);
UINT32 GetBytesPerBlock(DXGI_FORMAT fmt);