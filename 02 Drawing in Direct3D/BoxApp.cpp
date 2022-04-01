#include "D3DApp.h"
using namespace DirectX;
using namespace DirectX::PackedVector;

struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};
struct ObjectConstants
{
    XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

class BoxApp : public D3DApp
{
  public:
    BoxApp(HINSTANCE hInstance);
    BoxApp(const BoxApp &rhs) = delete;
    BoxApp &operator=(const BoxApp &rhs) = delete;
    ~BoxApp();

    virtual bool Initialize() override;

  private:
    virtual void OnResize() override;
    virtual void Update(const GameTimer &gt) override;
    virtual void Draw(const GameTimer &gt) override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry();
    void BuildPSO();

  private:
    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    //    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
    //
    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    ComPtr<ID3D12PipelineState> mPSO = nullptr;

    //    XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    //    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    //    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f * XM_PI;
    float mPhi = XM_PIDIV4;
    float mRadius = 5.0f;

    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
    // 为调试版本开启运行时内存检测，方便监督内存泄露的情况
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        BoxApp theApp(hInstance);
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

BoxApp::BoxApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
}

BoxApp::~BoxApp()
{
}

bool BoxApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    // 重置命令列表为执行初始化命令做好准备工作
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    return true;
}

void BoxApp::OnResize()
{
    D3DApp::OnResize();
}

void BoxApp::Update(const GameTimer &gt)
{
}

void BoxApp::Draw(const GameTimer &gt)
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
void BoxApp::BuildBoxGeometry()
{
    // clang-format off
    std::array<Vertex, 8> vertices =
        {
            Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
            Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
            Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
            Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
            Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
            Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
            Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
            Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
        };

    std::array<std::uint16_t, 36> indices =
    {
        // 立方体前表面
        0, 1, 2,
        0, 2, 3,

        // 立方体后表面
        4, 6, 5,
        4, 7, 6,

        // 立方体左表面
        4, 5, 1,
        4, 1, 0,

        // 立方体右表面
        3, 2, 6,
        3, 6, 7,

        // 立方体上表面
        1, 5, 6,
        1, 6, 2,

        // 立方体下表面
        4, 0, 3,
        4, 3, 7
        };
    // clang-format on

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);
    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->Name = "boxGeo";

    const UINT64 vbByteSize = 8 * sizeof(Vertex);
    // 我们无须为顶点缓冲区视图创建描述符堆。
    ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    VertexBufferGPU =
        d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices, vbByteSize, VertexBufferUploader);
    auto vertexBufferView = D3D12_VERTEX_BUFFER_VIEW{
        // 待创建视图的顶点缓冲区资源虚拟地址。我们可以通过 ID3D12Resource::GetGPUVirtualAddress 方法来获得此地址。
        VertexBufferGPU->GetGPUVirtualAddress(),
        // 待创建视图的顶点缓冲区大小（用字节表示）
        vbByteSize,
        // 每个顶点元素所占用的字节数
        sizeof(Vertex)};
    D3D12_VERTEX_BUFFER_VIEW vertexBuffers[1] = {vertexBufferView};
    mCommandList->IASetVertexBuffers(0, 1, vertexBuffers);

    // 将顶点缓冲区设置到输入槽上并不会对其执行实际的绘制操作，而是仅为顶点数据送至渲染流
    // 水线做好准备而已。这最后一步才是通过 ID3D12GraphicsCommandList::DrawInstanced 方法真正地绘制顶点。
    mCommandList->DrawInstanced(
        // 每个实例要绘制的顶点数量
        3,
        // 用于实现一种被称作实例化（instancing）的高级技术。就目前来说，我们只绘制一个实例，因而将此参数设置为 1
        8,
        // 指定顶点缓冲区内第一个被绘制顶点的索引（该索引值以 0 为基准）。
        0,
        // 用于实现一种被称作实例化的高级技术，暂时只需将其设置为 0。
        0);
    // 指定顶点被定义为何种图元
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 索引缓冲区
    // 我们也无须为索引缓冲区视图创建描述符堆
    ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
    ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;
    VertexBufferGPU =
        d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices, vbByteSize, VertexBufferUploader);
    auto indexBufferView = D3D12_INDEX_BUFFER_VIEW{
        // 待创建视图的顶点缓冲区资源虚拟地址。我们可以通过 ID3D12Resource::GetGPUVirtualAddress 方法来获得此地址。
        VertexBufferGPU->GetGPUVirtualAddress(),
        // 待创建视图的顶点缓冲区大小（用字节表示）
        vbByteSize,
        // 每个顶点元素所占用的字节数
        sizeof(Vertex)};
}
