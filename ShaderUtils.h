#pragma once
#include "DxCommon.h"

bool CompileShaderFromFile(const std::wstring& path, ID3DBlob** outCode);
