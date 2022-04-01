#pragma once

//#include "DDSTextureLoader.h"
//#include "MathHelper.h"
#include "d3dx12.h"
#include <D3Dcompiler.h>
#include <DirectXCollision.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <wrl.h>

// 使用模板别名(C++11)简化类型名
template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

/*
#if defined(_DEBUG)
    #ifndef Assert
    #define Assert(x, description)                                  \
    {                                                               \
        static bool ignoreAssert = false;                           \
        if(!ignoreAssert && !(x))                                   \
        {                                                           \
            Debug::AssertResult result = Debug::ShowAssertDialog(   \
            (L#x), description, AnsiToWString(__FILE__), __LINE__); \
        if(result == Debug::AssertIgnore)                           \
        {                                                           \
            ignoreAssert = true;                                    \
        }                                                           \
                    else if(result == Debug::AssertBreak)           \
        {                                                           \
            __debugbreak();                                         \
        }                                                           \
        }                                                           \
    }
    #endif
#else
    #ifndef Assert
    #define Assert(x, description)
    #endif
#endif
    */

class d3dUtil
{
  public:
    static bool IsKeyDown(int vkeyCode);

    static std::string ToString(HRESULT hr);

    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        // Constant buffers must be a multiple of the minimum hardware
        // allocation size (usually 256 bytes).  So round up to nearest
        // multiple of 256.  We do this by adding 255 and then masking off
        // the lower 2 bytes which store all bits < 256.
        // Example: Suppose byteSize = 300.
        // (300 + 255) & ~255
        // 555 & ~255
        // 0x022B & ~0x00ff
        // 0x022B & 0xff00
        // 0x0200
        // 512
        return (byteSize + 255) & ~255;
    }

    //    static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device *device, ID3D12GraphicsCommandList *cmdList, const void *initData, UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource> &uploadBuffer);

    //    static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
    //        const std::wstring& filename,
    //        const D3D_SHADER_MACRO* defines,
    //        const std::string& entrypoint,
    //        const std::string& target);
};

inline std::wstring AnsiToWString(const std::string &str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

#ifndef ReleaseCom
#define ReleaseCom(x)                                                                                                  \
    {                                                                                                                  \
        if (x)                                                                                                         \
        {                                                                                                              \
            x->Release();                                                                                              \
            x = 0;                                                                                                     \
        }                                                                                                              \
    }
#endif
