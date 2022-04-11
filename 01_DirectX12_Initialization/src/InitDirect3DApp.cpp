#include "D3DApp.h"

class InitDirect3DApp : public D3DApp
{
  public:
    InitDirect3DApp(HINSTANCE hInstance);
    ~InitDirect3DApp();

    virtual bool Initialize() override;

  private:
    virtual void OnResize() override;
    virtual void Update(const GameTimer &gt) override;
    virtual void Draw(const GameTimer &gt) override;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
    // 为调试版本开启运行时内存检测，方便监督内存泄露的情况
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        InitDirect3DApp theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (DxException &e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

InitDirect3DApp::InitDirect3DApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
}

InitDirect3DApp::~InitDirect3DApp()
{
}

bool InitDirect3DApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    return true;
}

void InitDirect3DApp::OnResize()
{
    D3DApp::OnResize();
}

void InitDirect3DApp::Update(const GameTimer &gt)
{
}

void InitDirect3DApp::Draw(const GameTimer &gt)
{
    // 重复使用记录命令的相关内存
    // 只有当与 GPU 关联的命令列表执行完成时，我们才能将其重置
    ThrowIfFailed(mDirectCmdListAlloc->Reset());

    // 在通过 ExecuteCommandList 方法将某个命令列表加入命令队列后，我们便可以重置该命令列表。以
    // 此来复用命令列表及其内存
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // 对资源的状态进行转换，将资源从呈现状态转换为渲染目标状态
    auto resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT,
                                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &resourceBarrier);

    // 设置视口和裁剪矩形。它们需要随着命令列表的重置而重置
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // 清除后台缓冲区和深度缓冲区
    FLOAT lightSteelBlue[4] = {0.690196097f, 0.768627524f, 0.870588303f, 1.000000000f};
    mCommandList->ClearRenderTargetView(
        // 待清除的资源 RTV
        CurrentBackBufferView(),
        // 定义即将为渲染目标填充的颜色
        lightSteelBlue,
        // pRects 数组中的元素数量。此值可以为 0。
        0,
        // 一个 D3D12_RECT 类型的数组，指定了渲染目标将要被清除的多个矩形区域。若设定此参数为nullptr，
        // 则表示清除整个渲染目标。
        nullptr);
    mCommandList->ClearDepthStencilView(
        // 待清除的深度/模板缓冲区 DSV
        DepthStencilView(),
        // 该标志用于指定即将清除的是深度缓冲区还是模板缓冲区。我们可以将此参数设置为 D3D12_CLEAR_FLAG_DEPTH
        // 或 D3D12_CLEAR_FLAG_STENCIL，也可以用按位或运算符连接两者，表示同时清除这两种缓冲区。
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        // 以此值来清除深度缓冲区
        1.0f,
        // 以此值来清除模板缓冲区
        0,
        // pRects 数组内的元素数量。可以将此值设置为 0。
        0,
        // 一个 D3D12_RECT 类型的数组，用以指定资源视图将要被清除的多个矩形区域。
        // 将此值设置为 nullptr，则表示清除整个渲染目标。
        nullptr);

    auto currentBackBufferView = CurrentBackBufferView();
    auto depthStencilView = DepthStencilView();

    // 设置我们希望在渲染流水线上使用的渲染目标和深度/模板缓冲区
    mCommandList->OMSetRenderTargets(
        // 待绑定的 RTV 数量，即 pRenderTargetDescriptors 数组中的元素个数。
        // 在使用多渲染目标这种高级技术时会涉及此参数。就目前来说，我们总是使用一个 RTV。
        1,
        // 指向 RTV 数组的指针，用于指定我们希望绑定到渲染流水线上的渲染目标。
        &currentBackBufferView,
        // 如果 pRenderTargetDescriptors 数组中的所有 RTV 对象在描述符堆中都是连续存放的，
        // 就将此值设为 true，否则设为 false。
        true,
        // 指向一个 DSV 的指针，用于指定我们希望绑定到渲染流水线上的深度/模板缓冲区。
        &depthStencilView);

    // 再次对资源状态进行转换，将资源从渲染目标状态转换回呈现状态
    resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                           D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &resourceBarrier);

    // 完成命令的记录
    ThrowIfFailed(mCommandList->Close());

    // 将待执行的命令列表加入命令队列
    ID3D12CommandList *cmdsLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // 交换后台缓冲区和前台缓冲区
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // 等待此帧的命令执行完毕。当前的实现没有什么效率，也过于简单
    // 我们在后面将重新组织渲染部分的代码，以免在每一帧都要等待
    FlushCommandQueue();
}
