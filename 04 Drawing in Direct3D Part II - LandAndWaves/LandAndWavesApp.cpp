#include "D3DApp.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

// 此示例构建了一个三角形栅格（grid），并通过将其中的顶点偏移到不同的高度来创建地形（terrain）。
// 另外，还要使用另一个三角形栅格来表现水，动态的改变其顶点高度来创建波浪。
// 此例程还将针对不同的常量缓冲区而切换所用的根描述符，这使我们能够摆脱设置繁琐的 CBV 描述符堆。

// 3 个帧资源元素
const int gNumFrameResources = 3;

// 存储绘制图形所需参数的轻量级结构体。它会随着不同的应用程序而有所差别
struct RenderItem
{
    RenderItem() = default;
    // 描述物体局部空间相对于世界空间的世界矩阵
    // 它定义了物体位于世界空间中的位置、朝向以及大小
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    // 用已更新标志（dirty flag）来表示物体的相关数据已发生改变，这意味着我们此时需要更新常量缓冲区。
    // 由于每个 FrameResource 中都有一个物体常量缓冲区，所以我们必须对每个 FrameResource 都进行更新。
    // 即，当我们修改物体数据的时候，应当按 NumFramesDirty = gNumFrameResources 进行设置，从而使每个帧资源都得到更新
    int NumFramesDirty = gNumFrameResources;

    // 该索引指向的 GPU 常量缓冲区对应于当前渲染项中的物体常量缓冲区
    UINT ObjCBIndex = -1;

    // 此渲染项参与绘制的几何体。注意，绘制一个几何体可能会用到多个渲染项
    MeshGeometry *Geo = nullptr;

    // 图元拓扑
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced 方法的参数
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

// 请不要与 GeometryGenerator::Vertex 结构体相混淆
struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

class LandAndWavesApp : public D3DApp
{
  public:
    explicit LandAndWavesApp(HINSTANCE hInstance);
    LandAndWavesApp(const LandAndWavesApp &rhs) = delete;
    LandAndWavesApp &operator=(const LandAndWavesApp &rhs) = delete;
    ~LandAndWavesApp() override;

    bool Initialize() override;

  private:
    void OnResize() override;
    void Update(const GameTimer &gt) override;
    void Draw(const GameTimer &gt) override;

    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;
    void OnMouseMove(WPARAM btnState, int x, int y) override;

    void OnKeyboardInput(const GameTimer &gt);
    void UpdateCamera(const GameTimer &gt);
    void UpdateObjectCBs(const GameTimer &gt);
    void UpdateMainPassCB(const GameTimer &gt);

    void BuildDescriptorHeaps();
    void BuildConstantBufferViews();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildLandGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList *cmdList, const std::vector<RenderItem *> &ritems);

  private:
    // 实例化一个由 3 个帧资源元素所构成的向量，并留有特定的成员变量来记录当前的帧资源
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource *mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    // 存有所有渲染项的向量
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;

    // 根据 PSO 来划分渲染项
    std::vector<RenderItem *> mOpaqueRitems;
    //    std::vector<RenderItem*> mTransparentRitems;

    PassConstants mMainPassCB;

    UINT mPassCbvOffset = 0;

    bool mIsWireframe = false;

    XMFLOAT3 mEyePos = {0.0f, 0.0f, 0.0f};
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f * XM_PI;
    float mPhi = 0.2f * XM_PI;
    float mRadius = 15.0f;

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
        LandAndWavesApp theApp(hInstance);
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

LandAndWavesApp::LandAndWavesApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
}

LandAndWavesApp::~LandAndWavesApp()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();
}

bool LandAndWavesApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    // 重置命令列表为执行初始化命令做好准备工作
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildLandGeometry();
    BuildRenderItems();
    BuildFrameResources();
    BuildDescriptorHeaps();
    BuildConstantBufferViews();
    BuildPSOs();

    // 执行初始化命令
    ThrowIfFailed(mCommandList->Close());
    // 将待执行的命令列表加入命令队列
    ID3D12CommandList *cmdsLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // 等待初始化完成
    FlushCommandQueue();
    return true;
}

void LandAndWavesApp::OnResize()
{
    D3DApp::OnResize();

    // 若用户调整了窗口尺寸，则更新纵横比并重新计算投影矩阵
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void LandAndWavesApp::Update(const GameTimer &gt)
{
    OnKeyboardInput(gt);
    UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    // 在 CPU 端等待 GPU，直到后者执行完这个围栏点之前的所有命令
    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    UpdateObjectCBs(gt);
    UpdateMainPassCB(gt);
}

void LandAndWavesApp::Draw(const GameTimer &gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
    // 重复使用记录命令的相关内存
    // 只有当与 GPU 关联的命令列表执行完成时，我们才能将其重置
    ThrowIfFailed(cmdListAlloc->Reset());

    // 在通过 ExecuteCommandList 方法将某个命令列表加入命令队列后，我们便可以重置该命令列表。以
    // 此来复用命令列表及其内存
    // 重置命令列表并指定初始 PSO
    if (mIsWireframe)
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
    }

    // 设置视口和裁剪矩形。它们需要随着命令列表的重置而重置
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // 对资源的状态进行转换，将资源从呈现状态转换为渲染目标状态
    auto resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT,
                                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &resourceBarrier);

    // 清除后台缓冲区和深度缓冲区
    mCommandList->ClearRenderTargetView(
        // 待清除的资源 RTV
        CurrentBackBufferView(),
        // 定义即将为渲染目标填充的颜色
        Colors::LightSteelBlue,
        // pRects 数组中的元素数量。此值可以为 0。
        0,
        // 一个 D3D12_RECT 类型的数组，指定了渲染目标将要被清除的多个矩形区域。
        // 若设定此参数为 nullptr，则表示清除整个渲染目标。
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

    ID3D12DescriptorHeap *descriptorHeaps[] = {mCbvHeap.Get()};
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
    auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
    passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
    // 令描述符表与渲染流水线相绑定。
    mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);
    DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

    // 按照资源的用途指示其状态的转变，将资源从渲染目标状态转换回呈现状态
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

    // 增加围栏值，将之前的命令标记到此围栏点上
    mCurrFrameResource->Fence = ++mCurrentFence;

    // 向命令队列添加一条指令，以设置新的围栏点 GPU 还在执行我们此前向命令队列中传入的命令，
    // 所以，GPU 不会立即设置新的围栏点，这要等到它处理完 Signal() 函数之前的所有命令
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void LandAndWavesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void LandAndWavesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void LandAndWavesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & (MK_LBUTTON != 0)))
    {
        // 根据鼠标的移动距离计算旋转角度，令每个像素按此角度的 1/4 进行旋转
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
        // 根据鼠标的输入来更新摄像机绕立方体旋转的角度
        mTheta += dx;
        mPhi += dy;
        // 限制角度 mPhi 的范围
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        // 使场景中的每个像素按鼠标移动距离的 0.005 倍进行缩放
        float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

        // 根据鼠标的输入更新摄像机的可视范围半径
        mRadius += dx - dy;

        // 限制可视半径的范围
        mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

// 利用描述符将常量缓冲区绑定至渲染流水线上
// 如果有 3 个帧资源与 n 个渲染项，那么就应存在 3n 个物体常量缓冲区（object constant buffer）
// 以及 3 个渲染过程常量缓冲区（pass constant buffer）。因此，我们也就需要创建 3(n+1) 个常量缓冲区视图（CBV）。
// 这样一来，我们还要修改 CBV 堆以容纳额外的描述符：
void LandAndWavesApp::BuildDescriptorHeaps()
{
    UINT objCount = (UINT)mOpaqueRitems.size();

    // 我们需要为每个帧资源中的每一个物体都创建一个 CBV 描述符，obj CBV
    // 为了容纳每个帧资源中的渲染过程 CBV 而 +1。pass CBV
    UINT numDescriptors = (objCount + 1) * gNumFrameResources;

    // 保存渲染过程 CBV 的起始偏移量。在本程序中，这是排在最后面的 3 个描述符
    // n frame ObjCBVs | n+1 frame ObjCBVs | n+2 frame ObjCBVs | n frame passCBV | n+1 frame passCBV | n+2 frame passCBV
    mPassCbvOffset = objCount * gNumFrameResources;

    // 常量缓冲区描述符要存放在以 D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV 类型所建的描述符堆里。
    // 这种堆内可以混合存储常量缓冲区描述符、着色器资源描述符和无序访问（unordered access）描述符。
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = numDescriptors;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;

    // 这段代码与我们之前创建渲染目标和深度/模板缓冲区这两种资源描述符堆的过程很相似。然而，其中却有着一个重要的区别，
    // 那就是在创建供着色器程序访问资源的描述符时，我们要把标志 Flags 指定为 DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE。

    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));
}

// 现在，我们就可以用下列代码来填充 CBV 堆，其中描述符 0 至描述符 n−1 包含了第 0 个帧资源的物体 CBV，
// 描述符 n 至描述符 2n−1 容纳了第 1 个帧资源的物体 CBV，以此类推，描述符 2n 至描述符 3n−1 包含了
// 第 2 个帧资源的物体 CBV。最后，3n、3n+1 以及 3n+2 分别存有第 0 个、第 1 个和第 2 个帧资源的渲染过程 CBV：
void LandAndWavesApp::BuildConstantBufferViews()
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT objCount = (UINT)mOpaqueRitems.size();
    // 每个帧资源中的每一个物体都需要一个对应的 CBV 描述符
    for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
    {
        auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
        for (UINT i = 0; i < objCount; ++i)
        {
            D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

            // 偏移到缓冲区中第 i 个物体的常量缓冲区
            cbAddress += i * objCBByteSize;

            // 偏移到该物体在描述符堆中的 CBV
            int heapIndex = frameIndex * objCount + i;
            auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
            handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

            // 如果常量缓冲区存储了一个内有 n 个物体常量数据的常量数组，那么我们就可以通过 BufferLocation 和
            // SizeInBytes 参数来获取第 i 个物体的常量数据。考虑到硬件的需求（即硬件的最小分配空间），
            // 成员 SizeInBytes 与 BufferLocation 必须为 256B 的整数倍。
            // 若将上述两个成员的值都指定为 64，那么我们将看到调试错误
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = cbAddress;
            cbvDesc.SizeInBytes = objCBByteSize;

            md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
        }
    }

    UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    // 最后 3 个描述符依次是每个帧资源的渲染过程 CBV
    // n frame ObjCBVs | n+1 frame ObjCBVs | n+2 frame ObjCBVs | n frame passCBV | n+1 frame passCBV | n+2 frame passCBV
    for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
    {
        auto passCB = mFrameResources[frameIndex]->PassCB->Resource();

        // 每个帧资源的渲染过程缓冲区中只存有一个常量缓冲区
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

        // 偏移到描述符堆中对应的渲染过程 CBV
        int heapIndex = mPassCbvOffset + frameIndex;

        // 通过调用 ID3D12DescriptorHeap::GetCPUDescriptorHandleForHeapStart 方法，
        // 我们可以获得堆中第一个描述符的句柄。然而，我们当前堆内所存放的描述符已不止一个，
        // 所以仅使用此方法并不能找到其他描述符的句柄。此时，我们希望能够偏移到堆内的其他描述符处，
        // 为此需要了解到达堆内下一个相邻描述符的增量。这个增量的大小其实是由硬件来确定的，
        // 所以我们必须从设备上查询相关的信息。此外，该增量还依赖于堆的具体类型。这里增量为： mCbvSrvUavDescriptorSize
        // 只要知道了相邻描述符之间的增量大小，就能通过两种 CD3DX12_CPU_DESCRIPTOR_HANDLE::Offset 方法之一偏
        // 移到第 n 个描述符的句柄处：

        // 指定要偏移到的目标描述符的编号，将它与相邻描述符之间的增量相乘，以此来找到第 n 个描述符的句柄
        auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
        //        handle.Offset(n * mCbvSrvDescriptorSize);
        // 或者用另一个等价实现，先指定要偏移到第几个描述符，再给出描述符的增量大小
        handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = cbAddress;
        cbvDesc.SizeInBytes = passCBByteSize;

        md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
    }
}

// 如果我们把着色器程序当作一个函数，而将输入资源看作着色器的函数参数，那么根签名
// 则定义了函数签名（其实这就是“根签名”一词的由来）。通过绑定不同的资源作为参数，
// 着色器的输出也将有所差别。例如，顶点着色器的输出取决于实际向它输入的顶点数据以
// 及为它绑定的具体资源。
// 根签名只定义了应用程序要绑定到渲染流水线的资源，却没有真正地执行任何资源绑定操作。
// 只要率先通过命令列表（command list）设置好根签名 （SetGraphicsRootSignature），我们就能用
// ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable 方法令描述符表与渲染流水线相绑定。
void LandAndWavesApp::BuildRootSignature()
{
    // 在绘制调用开始执行之前，我们应将不同的着色器程序所需的各种类型的资源绑定到渲染流水线上。
    // 实际上，不同类型的资源会被绑定到特定的寄存器槽（register slot）上，以供着色器程序访问。
    // 比如说，前文代码中的顶点着色器和像素着色器需要的是一个绑定到寄存器 b0 的常量缓冲区。
    // 在本书的后续内容中，我们会用到这两种着色器更高级的配置方法，以使多个常量缓冲区、纹理（texture）
    // 和采样器（sampler）都能与各自的寄存器槽相绑定

    // 寄存器槽就是向着色器传递资源的手段，register(*#)中*表示寄存器传递的资源类型，可以是 t（表示着色器资源视图）、
    // s（采样器）、u（无序访问视图）以及 b（常量缓冲区视图），#则为所用的寄存器编号。

    // 在 Direct3D 中，根签名由 ID3D12RootSignature 接口来表示，并以一组描述绘制调用过程中着
    // 色器所需资源的根参数（root parameter）定义而成。根参数可以是根常量（root constant）、根描述符（root
    // descriptor）或者描述符表（descriptor table）。我们在本章中仅使用描述符表，其他根参数均在第 7 章中
    // 进行讨论。描述符表指定的是描述符堆中存有描述符的一块连续区域。

    // 我们将在第 7 章对 CD3DX12_ROOT_PARAMETER 和 CD3DX12_DESCRIPTOR_RANGE 这两种辅助结构进行更加细致的解读，

    // 我们的着色器程序需要获取两个描述符表，因为这两个 CBV（常量缓冲区视图）有着不同的更新频率
    // ——渲染过程 CBV 仅需在每个渲染过程中设置一次，而物体 CBV 则要针对每一个渲染项进行配置

    // 创建一个只存有一个 CBV 的描述符表
    CD3DX12_DESCRIPTOR_RANGE cbvTable0;
    cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                   // 表中的描述符数量
                   1,
                   // 将这段描述符区域绑定至此基准着色器寄存器（base shader register）
                   0);

    CD3DX12_DESCRIPTOR_RANGE cbvTable1;
    cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

    // 根参数可以是描述符表、根描述符或根常量
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];

    // 创建根 CBV
    slotRootParameter[0].InitAsDescriptorTable(1,           // 描述符区域的数量
                                               &cbvTable0); // 指向描述符区域数组的指针
    slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

    // 上面这段代码创建了一个根参数，目的是将含有一个 CBV 的描述符表绑定到常量缓冲区寄存器 0，即 HLSL 代码中的
    // register(b0)。

    // 根签名由一组根参数构成
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
                                            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    // 创建仅含一个槽位（该槽位指向一个仅由单个常量缓冲区组成的描述符区域）的根签名
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        // Direct3D 12 规定，必须先将根签名的描述布局进行序列化处理（serialize），待其转换为以 ID3DBlob接口表示的序列化
        // 数据格式后，才可将它传入
        // CreateRootSignature方法，正式创建根签名。在此，可以设置将根签名按何种版本（1.0，1.1） 进行序列化处理。在
        // Windows 10（14393）版以后，可以 D3D12SerializeVersionedRootSignature 方法代之。
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
    {
        OutputDebugStringA((char *)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
                                                  serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void LandAndWavesApp::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;

    // 运行时编译 Shader
    mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_1");

    // 加载离线编译的 Shader 字节码，节省编译时间+提前发现编译错误
    //    mShaders["standardVS"] = d3dUtil::LoadBinary(L"Shaders\\color_vs.cso");
    //    mShaders["opaquePS"] = d3dUtil::LoadBinary(L"Shaders\\color_ps.cso");

    mInputLayout = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
}

void LandAndWavesApp::BuildLandGeometry()
{

}

void LandAndWavesApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    // 输入布局描述，此结构体中有两个成员：一个由 D3D12_INPUT_ELEMENT_DESC
    // 元素构成的数组，以及一个表示此数组中元素数量的无符号整数。
    opaquePsoDesc.InputLayout = {mInputLayout.data(), (UINT)mInputLayout.size()};
    // 指向一个与此 PSO 相绑定的根签名的指针。该根签名一定要与此 PSO 指定的着色器相兼容。
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    // 待绑定的顶点着色器。此成员由结构体 D3D12_SHADER_BYTECODE 表示，这个结构体存
    // 有指向已编译好的字节码数据的指针，以及该字节码数据所占的字节大小。
    opaquePsoDesc.VS = {reinterpret_cast<BYTE *>(mShaders["standardVS"]->GetBufferPointer()),
                        mShaders["standardVS"]->GetBufferSize()};
    // 待绑定的像素着色器
    opaquePsoDesc.PS = {reinterpret_cast<BYTE *>(mShaders["opaquePS"]->GetBufferPointer()),
                        mShaders["opaquePS"]->GetBufferSize()};
    // 指定用来配置光栅器的光栅化状态。
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    // 指定混合（blending）操作所用的混合状态。我们将在后续章节中讨论此状态组，
    // 目前仅将此成员指定为默认的 CD3DX12_BLEND_DESC(D3D12_DEFAULT)。
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    // 指定用于配置深度/模板测试的深度/模板状态。我们将在后续章节中对此状态进行讨论，
    // 目前只把它设为默认的 CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT)。
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    // 多重采样最多可采集 32 个样本。借此参数的 32 位整数值，即可设置每个采样点的采集情况（采集或禁止采集）。
    // 例如，若禁用了第 5 位（将第 5 位设置为 0），则将不会对第 5 个样本进行采样。
    // 当然，要禁止采集第 5 个样本的前提是，所用的多重采样至少要有 5 个样本。
    // 假如一个应用程序仅使用了单采样（single sampling），那么只能针对该参数的第 1 位进行配置。
    // 一般来说，使用的都是默认值 0xffffffff，即表示对所有的采样点都进行采样。
    opaquePsoDesc.SampleMask = UINT_MAX;
    // 指定图元的拓扑类型
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // 同时所用的渲染目标数量（即 RTVFormats 数组中渲染目标格式的数量）
    opaquePsoDesc.NumRenderTargets = 1;
    // 渲染目标的格式。利用该数组实现向多渲染目标同时进行写操作。使用此 PSO
    // 的渲染目标的格式设定应当与此参数相匹配。
    opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
    // 深度/模板缓冲区的格式。使用此 PSO 的深度/模板缓冲区的格式设定应当与此参数相匹配。
    opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    // 描述多重采样对每个像素采样的数量及其质量级别。此参数应与渲染目标的对应设置相匹配。
    opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

    // ID3D12PipelineState 对象集合了大量的流水线状态信息。为了保证性能，我们将所有这些对
    // 象都集总在一起，一并送至渲染流水线。通过这样的一个集合，Direct3D 便可以确定所有的状态是否彼
    // 此兼容，而驱动程序则能够据此而提前生成硬件本地指令及其状态。在 Direct3D 11 的状态模型中，这些
    // 渲染状态片段都是要分开配置的。然而这些状态实际都有一定的联系，以致如果其中的一个状态发生改变，
    // 那么驱动程序可能就要为了另一个相关的独立状态而对硬件重新进行编程。由于一些状态在配置流
    // 水线时需要改变，因而硬件状态也就可能被频繁地改写。为了避免这些冗余的操作，驱动程序往往会推
    // 迟针对硬件状态的编程动作，直到明确整条流水线的状态发起绘制调用后，才正式生成对应的本地指令与状态。
    // 但是，这种延迟操作需要驱动在运行时进行额外的记录工作，即追踪状态的变化，而后才能在
    // 运行时生成改写硬件状态的本地代码。在 Direct3D 12 的新模型中，驱动程序可以在初始化期间生成对流
    // 水线状态编程的全部代码，这便是我们将大多数的流水线状态指定为一个集合所带来的好处。

    // 并非所有的渲染状态都封装于 PSO 内，如视口（viewport）和裁剪矩形（scissor rectangle）等
    // 属性就独立于 PSO。由于将这些状态的设置与其他的流水线状态分隔开来会更有效，所以把它们
    // 强行集中在 PSO 内也并不会为之增添任何优势。
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

    // 复制 opaquePsoDesc，修改为线框模式
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
    opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(
        md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}

void LandAndWavesApp::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, (UINT)mAllRitems.size()));
    }
}

// 更新物体常量缓冲区
// 将物体世界坐标更新到当前 FrameResource 的常量缓冲里面。
void LandAndWavesApp::UpdateObjectCBs(const GameTimer &gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (const auto &item : mAllRitems)
    {
        // 只要常量发生了改变就得更新常量缓冲区内的数据。而且要对每个帧资源都进行更新
        if (item->NumFramesDirty > 0)
        {
            XMMATRIX world = XMLoadFloat4x4(&item->World);
            ObjectConstants objectConstants;
            XMStoreFloat4x4(&objectConstants.World, XMMatrixTranspose(world));
            currObjectCB->CopyData(item->ObjCBIndex, objectConstants);
            // 还需要对下一个 FrameResource 进行更新
            item->NumFramesDirty--;
        }
    }
}

// 更新渲染过程常量缓冲区
void LandAndWavesApp::UpdateMainPassCB(const GameTimer &gt)
{
    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    auto determinant = XMMatrixDeterminant(view);
    XMMATRIX invView = XMMatrixInverse(&determinant, view);
    determinant = XMMatrixDeterminant(proj);
    XMMATRIX invProj = XMMatrixInverse(&determinant, proj);
    determinant = XMMatrixDeterminant(viewProj);
    XMMATRIX invViewProj = XMMatrixInverse(&determinant, viewProj);

    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    mMainPassCB.EyePosW = mEyePos;
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);

    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();
    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}

void LandAndWavesApp::BuildRenderItems()
{
    auto boxRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
    boxRitem->ObjCBIndex = 0;
    boxRitem->Geo = mGeometries["shapeGeo"].get();
    boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
    mAllRitems.push_back(std::move(boxRitem));

    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->World = MathHelper::Identity4x4();
    gridRitem->ObjCBIndex = 1;
    gridRitem->Geo = mGeometries["shapeGeo"].get();
    gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
    mAllRitems.push_back(std::move(gridRitem));

    // 构建图 7.6 所示的柱体和球体
    UINT objCBIndex = 2;
    for (int i = 0; i < 5; ++i)
    {
        auto leftCylRitem = std::make_unique<RenderItem>();
        auto rightCylRitem = std::make_unique<RenderItem>();
        auto leftSphereRitem = std::make_unique<RenderItem>();
        auto rightSphereRitem = std::make_unique<RenderItem>();

        XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
        XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

        XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
        XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

        XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
        leftCylRitem->ObjCBIndex = objCBIndex++;
        leftCylRitem->Geo = mGeometries["shapeGeo"].get();
        leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
        leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
        leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

        XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
        rightCylRitem->ObjCBIndex = objCBIndex++;
        rightCylRitem->Geo = mGeometries["shapeGeo"].get();
        rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
        rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
        rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

        XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
        leftSphereRitem->ObjCBIndex = objCBIndex++;
        leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
        leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
        leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
        leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

        XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
        rightSphereRitem->ObjCBIndex = objCBIndex++;
        rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
        rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
        rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
        rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

        mAllRitems.push_back(std::move(leftCylRitem));
        mAllRitems.push_back(std::move(rightCylRitem));
        mAllRitems.push_back(std::move(leftSphereRitem));
        mAllRitems.push_back(std::move(rightSphereRitem));
    }
    // 此演示程序中的所有渲染项都是非透明的
    for (auto &e : mAllRitems)
        mOpaqueRitems.push_back(e.get());
}
void LandAndWavesApp::DrawRenderItems(ID3D12GraphicsCommandList *cmdList, const std::vector<RenderItem *> &ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    auto objectCB = mCurrFrameResource->ObjectCB->Resource();
    // 对于每个渲染项来说...
    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        auto vertexBufferView = ri->Geo->VertexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
        auto indexBufferView = ri->Geo->IndexBufferView();
        cmdList->IASetIndexBuffer(&indexBufferView);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        // 为了绘制当前的帧资源和当前物体，偏移到描述符堆中对应的 CBV 处
        UINT cbvIndex = mCurrFrameResourceIndex * (UINT)mOpaqueRitems.size() + ri->ObjCBIndex;
        auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
        cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);

        // 令描述符表与渲染流水线相绑定
        cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);
        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void LandAndWavesApp::OnKeyboardInput(const GameTimer &gt)
{
    if (GetAsyncKeyState('1') & 0x8000)
        mIsWireframe = true;
    else
        mIsWireframe = false;
}

void LandAndWavesApp::UpdateCamera(const GameTimer &gt)
{
    // https://zh.wikipedia.org/wiki/%E7%90%83%E5%BA%A7%E6%A8%99%E7%B3%BB
    // 由于摄像机涉及环绕物体旋转等操作，所以先利用球坐标表示变换（可将摄像机视为针对目标物体的可控“侦查卫星”，
    // 用鼠标调整摄像机与物体间的距离（也就是球面半径）以及观察角度），再将其转换为笛卡儿坐标的表示更为方便。
    // 而这一摄像机系统也被称为旋转摄像机系统（orbiting camera system，也有作环绕摄像机系统）。
    // 顺便提一句，除了上述两种坐标系之外，圆柱坐标系（cylindrical coordinate system，也有作柱面坐标系）
    // 有时也会派上用场，因此掌握这 3 种坐标系间的转换对实际应用也会大有裨益。

    // 由球坐标（也有译作球面坐标）转换为笛卡儿坐标
    // 数学中通常使用的球坐标(r, θ, φ) ：径向距离 r，方位角 θ（theta）与极角 φ（phi）
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);
    // 构建观察矩阵
    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);
}
