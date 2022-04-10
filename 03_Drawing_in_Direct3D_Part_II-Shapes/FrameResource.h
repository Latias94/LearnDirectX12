#pragma once

#include "MathHelper.h"
#include "UploadBuffer.h"
#include "d3dUtil.h"

// 绘制物体所用的常量数据
struct ObjectConstants
{
    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
};

struct PassConstants
{
    DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;
};

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

// 以 CPU 每帧都需更新的资源作为基本元素，创建一个环形数组（circular array，也有译作循环数组）。
// 我们称这些资源为帧资源（frame resource），而这种循环数组通常是由 3 个帧资源元素所构成的。
struct FrameResource
{
  public:
    FrameResource(ID3D12Device *device, UINT passCount, UINT objectCount);
    FrameResource(const FrameResource &rhs) = delete;
    FrameResource &operator=(const FrameResource &rhs) = delete;
    ~FrameResource();

    // 在 GPU 处理完与此命令分配器相关的命令之前，我们不能对它进行重置。
    // 所以每一帧都要有它们自己的命令分配器
    ComPtr<ID3D12CommandAllocator> CmdListAlloc;
    // 在 GPU 执行完引用此常量缓冲区的命令之前，我们不能对它进行更新。
    // 因此每一帧都要有它们自己的常量缓冲区
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

    // 通过围栏值将命令标记到此围栏点，这使我们可以检测到 GPU 是否还在使用这些帧资源
    UINT64 Fence = 0;
};
