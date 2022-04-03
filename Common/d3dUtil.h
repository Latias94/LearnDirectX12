#pragma once

//#include "DDSTextureLoader.h"
//#include "MathHelper.h"
#include "DXTrace.h"
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
template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

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

//    static std::string ToString(HRESULT hr);

    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        // 常量缓冲区的大小必须是硬件最小分配空间（通常是 256B）的整数倍
        // 为此，要将其凑整为满足需求的最小的 256 的整数倍。我们现在通过为输入值 byteSize 加上 255，
        // 再屏蔽求和结果的低 2 字节（即计算结果中小于 256 的数据部分）来实现这一点
        // 例如：假设 byteSize = 300
        // (300 + 255) & ~255
        // 555 & ~255
        // 0x022B & ~0x00ff
        // 0x022B & 0xff00
        // 0x0200
        // 512
        return (byteSize + 255) & ~255;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring &filename);

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device *device, ID3D12GraphicsCommandList *cmdList, const void *initData, UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource> &uploadBuffer);

    static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::wstring &filename, const D3D_SHADER_MACRO *defines,
                                                          const std::string &entrypoint, const std::string &target);
};

// 利用 SubmeshGeometry 来定义 MeshGeometry 中存储的单个几何体
// 此结构体适用于将多个几何体数据存于一个顶点缓冲区和一个索引缓冲区的情况
// 它提供了对存于顶点缓冲区和索引缓冲区中的单个几何体进行绘制所需的数据和偏移量，我们可以据此来
// 实现图 6.3 中所描绘的技术
struct SubmeshGeometry
{
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    // 我们将每个物体的第一个顶点相对于全局顶点缓冲区的位置叫作它的基准顶点地址（base vertex location）。
    // 通常来讲，一个物体的新索引是通过原始索引加上它的基准顶点地址来获取的。
    INT BaseVertexLocation = 0;

    // 通过此子网格来定义当前 SubmeshGeometry 结构体中所存几何体的包围盒（bounding box）。我们
    // 将在本书的后续章节中使用此数据
    DirectX::BoundingBox Bounds;
};

struct MeshGeometry
{
    // 指定此几何体网格集合的名称，这样我们就能根据此名找到它
    std::string Name;

    // 系统内存中的副本。由于顶点/索引可以是泛型格式（具体格式依用户而定），所以用 Blob 类型来表示
    // 待用户在使用时再将其转换为适当的类型
    ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

    ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

    ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    // 与缓冲区相关的数据
    UINT VertexByteStride = 0;
    UINT VertexBufferByteSize = 0;
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
    UINT IndexBufferByteSize = 0;

    // 一个 MeshGeometry 结构体能够存储一组顶点/索引缓冲区中的多个几何体
    // 若利用下列容器来定义子网格几何体，我们就能单独地绘制出其中的子网格（单个几何体）
    std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        // 待创建视图的顶点缓冲区资源虚拟地址。我们可以通过 ID3D12Resource::GetGPUVirtualAddress 方法来获得此地址。
        vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        // 每个顶点元素所占用的字节数
        vbv.StrideInBytes = VertexByteStride;
        // 待创建视图的顶点缓冲区大小（用字节表示）
        vbv.SizeInBytes = VertexBufferByteSize;

        return vbv;
    }

    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        // 待创建视图的顶点缓冲区资源虚拟地址。我们可以通过 ID3D12Resource::GetGPUVirtualAddress 方法来获得此地址。
        ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
        // 索引的格式必须为表示 16 位索引的 DXGI_FORMAT_R16_UINT 类型，或表示 32 位索引的 DXGI_FORMAT_R32_UINT 类型。
        // 16 位的索引可以减少内存和带宽的占用，但如果索引值范围超过了 16 位数据的表达范围，则也只能采用 32 位索引了。
        ibv.Format = IndexFormat;
        // 待创建视图的顶点缓冲区大小（用字节表示）
        ibv.SizeInBytes = IndexBufferByteSize;
        return ibv;
    }

    // 待数据上传至 GPU 后，我们就能释放这些内存了
    void DisposeUploaders()
    {
        VertexBufferUploader = nullptr;
        IndexBufferUploader = nullptr;
    }
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
