#include "D3DApp.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"
#include "Waves.h"

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

    Material* Mat = nullptr;

    // 此渲染项参与绘制的几何体。注意，绘制一个几何体可能会用到多个渲染项
    MeshGeometry *Geo = nullptr;

    // 图元拓扑
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced 方法的参数
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
    Opaque = 0,
    Count
};

class LitWavesApp : public D3DApp
{
  public:
    explicit LitWavesApp(HINSTANCE hInstance);
    LitWavesApp(const LitWavesApp &rhs) = delete;
    LitWavesApp &operator=(const LitWavesApp &rhs) = delete;
    ~LitWavesApp() override;

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
    void UpdateMaterialCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer &gt);
    void UpdateWaves(const GameTimer &gt);

    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildLandGeometry();
    void BuildWavesGeometryBuffers();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList *cmdList, const std::vector<RenderItem *> &ritems);

    float GetHillsHeight(float x, float z) const;
    XMFLOAT3 GetHillsNormal(float x, float z) const;

  private:
    // 实例化一个由 3 个帧资源元素所构成的向量，并留有特定的成员变量来记录当前的帧资源
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource *mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
//    std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    //  我们保存了一份波浪渲染项的引用（mWavesRitem），从而可以动态地调整其顶点缓冲区。由
    // 于渲染项的顶点缓冲区是个动态的缓冲区，并且每一帧都在发生改变，因此这样做很有必要。
    RenderItem *mWavesRitem = nullptr;

    // 存有所有渲染项的向量
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;

    // Render items divided by PSO.
    std::vector<RenderItem *> mRitemLayer[(int)RenderLayer::Count];

    std::unique_ptr<Waves> mWaves;

    PassConstants mMainPassCB;

    bool mIsWireframe = false;

    XMFLOAT3 mEyePos = {0.0f, 0.0f, 0.0f};
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f * XM_PI;
    float mPhi = XM_PIDIV2 - 0.1f;
    float mRadius = 50.0f;

    float mSunTheta = 1.25f * XM_PI;
    float mSunPhi = XM_PIDIV4;

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
        LitWavesApp theApp(hInstance);
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

LitWavesApp::LitWavesApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
}

LitWavesApp::~LitWavesApp()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();
}

bool LitWavesApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    // 重置命令列表为执行初始化命令做好准备工作
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildLandGeometry();
    BuildWavesGeometryBuffers();
    BuildRenderItems();
    BuildFrameResources();
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

void LitWavesApp::OnResize()
{
    D3DApp::OnResize();

    // 若用户调整了窗口尺寸，则更新纵横比并重新计算投影矩阵
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void LitWavesApp::Update(const GameTimer &gt)
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
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    UpdateObjectCBs(gt);
    UpdateMainPassCB(gt);
    UpdateWaves(gt);
}

void LitWavesApp::Draw(const GameTimer &gt)
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

    // 现在根签名不用描述符堆了，改用根常量，直接绑定 CBV 了。
    //    ID3D12DescriptorHeap *descriptorHeaps[] = {mCbvHeap.Get()};
    //    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    //    int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
    //    auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
    //    passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
    //
    //    // 令描述符表与渲染流水线相绑定。
    //    mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // 绑定渲染过程中所用的常量缓冲区。在每个渲染过程中，这段代码只需执行一次
    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

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

void LitWavesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void LitWavesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void LitWavesApp::OnMouseMove(WPARAM btnState, int x, int y)
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
        float dx = 0.2f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.2f * static_cast<float>(y - mLastMousePos.y);

        // 根据鼠标的输入更新摄像机的可视范围半径
        mRadius += dx - dy;

        // 限制可视半径的范围
        mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

// 如果我们把着色器程序当作一个函数，而将输入资源看作着色器的函数参数，那么根签名
// 则定义了函数签名（其实这就是“根签名”一词的由来）。通过绑定不同的资源作为参数，
// 着色器的输出也将有所差别。例如，顶点着色器的输出取决于实际向它输入的顶点数据以
// 及为它绑定的具体资源。
// 根签名只定义了应用程序要绑定到渲染流水线的资源，却没有真正地执行任何资源绑定操作。
// 只要率先通过命令列表（command list）设置好根签名 （SetGraphicsRootSignature），我们就能用
// ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable 方法令描述符表与渲染流水线相绑定。
void LitWavesApp::BuildRootSignature()
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

    // 此 “Land and Waves” 演示程序较之前 “Shape” 例程的另一个区别是：我们在前者中使用了根描述符，
    // 因此就可以摆脱描述符堆而直接绑定 CBV 了。为此，程序还要做如下改动：
    //
    // 1. 根签名需要变为取两个根 CBV，而不再是两个描述符表。
    // 2. 不采用 CBV 堆，更无需向其填充描述符。
    // 3. 涉及一种用于绑定根描述符的新语法。

    // 根参数可以是描述符表、根描述符或根常量
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];

    // 创建根 CBV
    // 指定的着色器常量缓冲区寄存器分别为 “b0” 和 “b1”
    slotRootParameter[0].InitAsConstantBufferView(0); // 物体的 CBV
    slotRootParameter[1].InitAsConstantBufferView(1); // 渲染过程 CBV

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

void LitWavesApp::BuildShadersAndInputLayout()
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

void LitWavesApp::BuildLandGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

    //
    // 获取我们所需要的顶点元素，并利用高度函数计算每个顶点的高度值
    // 另外，顶点的颜色要基于它们的高度而定
    // 所以，图像中才会有看起来如沙质的沙滩、山腰处的植被以及山峰处的积雪
    //

    std::vector<Vertex> vertices(grid.Vertices.size());
    for (size_t i = 0; i < grid.Vertices.size(); ++i)
    {
        auto &p = grid.Vertices[i].Position;
        vertices[i].Pos = p;
        vertices[i].Pos.y = GetHillsHeight(p.x, p.z);

        // 基于顶点高度为它上色
        if (vertices[i].Pos.y < -10.0f)
        {
            // 沙滩的颜色
            vertices[i].Color = XMFLOAT4(1.0f, 0.96f, 0.62f, 1.0f);
        }
        else if (vertices[i].Pos.y < 5.0f)
        {
            // 浅黄绿色
            vertices[i].Color = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.0f);
        }
        else if (vertices[i].Pos.y < 12.0f)
        {
            // 深黄绿色
            vertices[i].Color = XMFLOAT4(0.1f, 0.48f, 0.19f, 1.0f);
        }
        else if (vertices[i].Pos.y < 20.0f)
        {
            // 深棕色
            vertices[i].Color = XMFLOAT4(0.45f, 0.39f, 0.34f, 1.0f);
        }
        else
        {
            // 白雪皑皑
            vertices[i].Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> indices = grid.GetIndices16();
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "landGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(),
                                                        vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize,
                                                       geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;

    mGeometries["landGeo"] = std::move(geo);
}

void LitWavesApp::BuildWavesGeometryBuffers()
{
    std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
    assert(mWaves->VertexCount() < 0x0000ffff);

    // Iterate over each quad.
    int m = mWaves->RowCount();
    int n = mWaves->ColumnCount();
    int k = 0;
    for (int i = 0; i < m - 1; ++i)
    {
        for (int j = 0; j < n - 1; ++j)
        {
            indices[k] = i * n + j;
            indices[k + 1] = i * n + j + 1;
            indices[k + 2] = (i + 1) * n + j;

            indices[k + 3] = (i + 1) * n + j;
            indices[k + 4] = i * n + j + 1;
            indices[k + 5] = (i + 1) * n + j + 1;

            k += 6; // next quad
        }
    }

    UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
    UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "waterGeo";

    // Set dynamically.
    geo->VertexBufferCPU = nullptr;
    geo->VertexBufferGPU = nullptr;

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize,
                                                       geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;

    mGeometries["waterGeo"] = std::move(geo);
}

void LitWavesApp::BuildPSOs()
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

void LitWavesApp::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(
            std::make_unique<FrameResource>(md3dDevice.Get(), 1, (UINT)mAllRitems.size(), mWaves->VertexCount()));
    }
}

// 更新物体常量缓冲区
// 将物体世界坐标更新到当前 FrameResource 的常量缓冲里面。
void LitWavesApp::UpdateObjectCBs(const GameTimer &gt)
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
            // 这里只更新了当前 FrameResource 的物体常量缓冲
            currObjectCB->CopyData(item->ObjCBIndex, objectConstants);
            // 还需要对下一个 FrameResource 进行更新
            item->NumFramesDirty--;
        }
    }
}

// 在更新函数中，当材质数据有了变化（即存在所谓的“脏数据”）时，便会将其复制到常量缓冲区的
// 对应子区域内，因此 GPU 材质常量缓冲区中的数据总是与系统内存中的最新材质数据保持一致
void UpdateMaterialCBs(const GameTimer& gt){

}

// 更新渲染过程常量缓冲区
void LitWavesApp::UpdateMainPassCB(const GameTimer &gt)
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

void LitWavesApp::BuildRenderItems()
{
    auto wavesRitem = std::make_unique<RenderItem>();
    wavesRitem->World = MathHelper::Identity4x4();
    wavesRitem->ObjCBIndex = 0;
    wavesRitem->Geo = mGeometries["waterGeo"].get();
    wavesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
    wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

    mWavesRitem = wavesRitem.get();

    mRitemLayer[(int)RenderLayer::Opaque].push_back(wavesRitem.get());

    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->World = MathHelper::Identity4x4();
    gridRitem->ObjCBIndex = 1;
    gridRitem->Geo = mGeometries["landGeo"].get();
    gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

    mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());

    mAllRitems.push_back(std::move(wavesRitem));
    mAllRitems.push_back(std::move(gridRitem));
}

void LitWavesApp::DrawRenderItems(ID3D12GraphicsCommandList *cmdList, const std::vector<RenderItem *> &ritems)
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

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress();
        objCBAddress += ri->ObjCBIndex * objCBByteSize;

        // 以传递参数的方式将 CBV 与某个根描述符相绑定
        cmdList->SetGraphicsRootConstantBufferView(
            // RootParameterIndex：CBV 将要绑定到的根参数索引，即寄存器的槽位号。
            0,
            // BufferLocation：含有常量缓冲区数据资源的虚拟地址。
            objCBAddress);
        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void LitWavesApp::OnKeyboardInput(const GameTimer &gt)
{
    if (GetAsyncKeyState('1') & 0x8000)
        mIsWireframe = true;
    else
        mIsWireframe = false;
}

void LitWavesApp::UpdateCamera(const GameTimer &gt)
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

float LitWavesApp::GetHillsHeight(float x, float z) const
{
    return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 LitWavesApp::GetHillsNormal(float x, float z) const
{
    // n = (-df/dx, 1, -df/dz)
    XMFLOAT3 n(-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z), 1.0f,
               -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
    XMStoreFloat3(&n, unitNormal);

    return n;
}

// 模拟波浪并更新顶点缓冲区
void LitWavesApp::UpdateWaves(const GameTimer &gt)
{
    // 每隔 1/4 秒就要生成一个随机波浪
    static float t_base = 0.0f;
    if ((mTimer.TotalTime() - t_base) >= 0.25f)
    {
        t_base += 0.25f;

        int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
        int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

        float r = MathHelper::RandF(0.2f, 0.5f);

        mWaves->Disturb(i, j, r);
    }

    // 更新模拟的波浪
    mWaves->Update(gt.DeltaTime());

    // 用波浪方程求出的新数据来更新波浪顶点缓冲区
    auto currWavesVB = mCurrFrameResource->WavesVB.get();
    for (int i = 0; i < mWaves->VertexCount(); ++i)
    {
        Vertex v;

        v.Pos = mWaves->Position(i);
        v.Color = XMFLOAT4(DirectX::Colors::Blue);

        currWavesVB->CopyData(i, v);
    }

    // 将波浪渲染项的动态顶点缓冲区设置到当前帧的顶点缓冲区
    mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void LitWavesApp::BuildMaterials()
{
    auto grass = std::make_unique<Material>();
    grass->Name = "grass";
    grass->MatCBIndex = 0;
    grass->DiffuseAlbedo = XMFLOAT4(0.2f, 0.6f, 0.2f, 1.0f);
    grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
    grass->Roughness = 0.125f;

    // 当前这种水的材质定义得并不是很好，但是由于我们还未学会所需的全部渲染工具（如透明度、环境反
    // 射等），因此暂时先用这些数据解当务之急吧
    auto water = std::make_unique<Material>();
    water->Name = "water";
    water->MatCBIndex = 1;
    water->DiffuseAlbedo = XMFLOAT4(0.0f, 0.2f, 0.6f, 1.0f);
    water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    water->Roughness = 0.0f;

    mMaterials["grass"] = std::move(grass);
    mMaterials["water"] = std::move(water);
}
