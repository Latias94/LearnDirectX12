#include "D3DApp.h"
#include "DXTrace.h"
#include "assert.h"

float D3DApp::AspectRatio() const
{
    return static_cast<float>(mClientWidth) / mClientHeight;
}
D3DApp::~D3DApp()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();
}

// 1.  用 D3D12CreateDevice 函数创建  ID3D12Device 接口实例。
// 2.  创建一个 ID3D12Fence 对象，并查询描述符的大小。
// 3.  检测用户设备对 4X MSAA 质量级别的支持情况。
// 4.  依次创建命令队列、命令列表分配器和主命令列表。
// 5.  描述并创建交换链。
// 6.  创建应用程序所需的描述符堆。
// 7.  调整后台缓冲区的大小，并为它创建渲染目标视图。
// 8.  创建深度/模板缓冲区及与之关联的深度/模板视图。
// 9.  设置视口（viewport）和裁剪矩形（scissor rectangle）。
bool D3DApp::InitDirect3D()
{
    // --------- 创建设备

#if defined(DEBUG) || defined(_DEBUG)
    {
        // 启用 D3D12 的调试层
        ComPtr<ID3D12Debug> debugController;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
        debugController->EnableDebugLayer();
    }
#endif
    // DXGIFactory2 for debugging flag.
    ThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&mdxgiFactory)));

    // 尝试创建硬件设备
    // pAdapter：指定在创建设备时所用的显示适配器。若将此参数设定为空指针，则使用主显示适配器（默认适配器）。
    // MinimumFeatureLevel：应用程序需要硬件所支持的最低功能级别。如果适配器不支持此功能级别，则设备创建失败。
    //在我们的框架中指定的是 D3D_FEATURE_LEVEL_11_0（即支持 Direct3D 11 的特性）。
    HRESULT hardwareResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&md3dDevice));
    // 当调用 D3D12CreateDevice 失败后，程序将回退至 WARP 设备
    if (FAILED(hardwareResult))
    {
        ComPtr<IDXGIAdapter> pWarpAdapter;
        ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));
        ThrowIfFailed(D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&md3dDevice)));
    }

    // --------- 创建围栏并获取描述符的大小
    ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
    mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // --------- 检测对 4X MSAA 质量级别的支持
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
    msQualityLevels.Format = mBackBufferFormat;
    msQualityLevels.SampleCount = 4;
    msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels.NumQualityLevels = 0;
    ThrowIfFailed(md3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels,
                                                  sizeof(msQualityLevels)));
    m4xMsaaQuality = msQualityLevels.NumQualityLevels;
    assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");
}
