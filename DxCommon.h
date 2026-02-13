#pragma once

#include <windows.h>
#include <tchar.h>
#include <assert.h>

#include <d3d11.h>
#include <dxgi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#define SAFE_RELEASE(p) do { if ((p) != nullptr) { (p)->Release(); (p) = nullptr; } } while (0)
