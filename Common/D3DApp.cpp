
#include "D3DApp.h"
#include "d3dUtil.h"
#include <cassert>
#include <vector>
#include <windowsx.h>

D3DApp *D3DApp::mApp = nullptr;
D3DApp *D3DApp::GetApp()
{
    return mApp;
}

D3DApp::D3DApp(HINSTANCE hInstance) : mhAppInst(hInstance)
{
    // Only one D3DApp can be constructed.
    assert(mApp == nullptr);
    mApp = this;
}

D3DApp::~D3DApp()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();
}

HINSTANCE D3DApp::AppInst() const
{
    return mhAppInst;
}

HWND D3DApp::MainWnd() const
{
    return mhMainWnd;
}

float D3DApp::AspectRatio() const
{
    return static_cast<float>(mClientWidth) / mClientHeight;
}

bool D3DApp::Get4xMsaaState() const
{
    return m4xMsaaState;
}

void D3DApp::Set4xMsaaState(bool value)
{
    if (m4xMsaaState != value)
    {
        m4xMsaaState = value;

        // Recreate the swapchain and buffers with new multisample settings.
        CreateSwapChain();
        OnResize();
    }
}

bool D3DApp::Initialize()
{
    if (!InitMainWindow())
        return false;

    if (!InitDirect3D())
        return false;

    // Do the initial resize code.
    OnResize();

    return true;
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
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
// https://stackoverflow.com/questions/14006988/winrt-c-comptr-getaddressof-vs
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
    // 由于我们所用的平台必能支持 4X MSAA 这一功能，其返回值应该也总是大于 0
    assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

#ifdef _DEBUG
    LogAdapters();
#endif

    // --------- 创建命令队列和命令列表
    CreateCommandObjects();

    // --------- 描述并创建交换链
    CreateSwapChain();

    // --------- 创建应用程序所需的描述符堆
    CreateRtvAndDsvDescriptorHeaps();

    // -> OnResize
    return true;
}

void D3DApp::CreateSwapChain()
{
    // 释放之前所创的交换链，随后再进行重建
    mSwapChain.Reset();

    DXGI_SWAP_CHAIN_DESC sd;
    // BufferDesc 这个结构体描述了待创建后台缓冲区的属性。
    sd.BufferDesc.Width = mClientWidth;   // 缓冲区分辨率的宽度
    sd.BufferDesc.Height = mClientHeight; // 缓冲区分辨率的高度
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = mBackBufferFormat;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED; // 逐行扫描 vs. 隔行扫描
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;                 // 图像如何相对于屏幕进行拉伸
    // SampleDesc 多重采样的质量级别以及对每个像素的采样次数，可参见 4.1.8 节。
    // 对于单次采样来说，我们要将采样数量指定为 1，质量级别指定为 0。
    sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    // 由于我们要将数据渲染至后台缓冲区（即用它作为渲染目标），因此将此参数指定为 DXGI_USAGE_RENDER_TARGET_OUTPUT。
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    // 交换链中所用的缓冲区数量。我们将它指定为 2，即采用双缓冲。
    sd.BufferCount = SwapChainBufferCount;
    // 渲染窗口的句柄。
    sd.OutputWindow = mhMainWnd;
    // 若指定为 true，程序将在窗口模式下运行；如果指定为 false，则采用全屏模式。
    sd.Windowed = true;
    // DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL forces DXGI to guarantee that
    // the contents of each back buffer is preserved across IDXGISwapChain::Present calls,
    // whereas DXGI_SWAP_EFFECT_FLIP_DISCARD doesn't provide this guarantee.
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    // 可选标志。如果将其指定为 DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH，那么，当程序切换为全屏模式时，
    // 它将选择最适于当前应用程序窗口尺寸的显示模式。如果没有指定该标志，当程序切换为全屏模式时，将采用当前桌面的显示模式。
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    //注意，交换链需要通过命令队列对其进行刷新
    ThrowIfFailed(mdxgiFactory->CreateSwapChain(mCommandQueue.Get(), &sd, mSwapChain.GetAddressOf()));
}

void D3DApp::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
    ThrowIfFailed(md3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                     IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));
    ThrowIfFailed(md3dDevice->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, // 存储的是一系列可供 GPU 直接执行的命令
        mDirectCmdListAlloc.Get(),         // 关联命令分配器
        nullptr, // 初始化流水线状态对象，这里由于我们不会发起任何绘制命令，所以也就不会用到流水线状态对象。
        IID_PPV_ARGS(mCommandList.GetAddressOf())));
    // 首先要将命令列表置于关闭状态。这是因为在第一次引用命令列表时，我们要对它进行重置，而在调用重置方法之前又需先将其关闭
    mCommandList->Close();
}

void D3DApp::FlushCommandQueue()
{
    // 增加围栏值，接下来将命令标记到此围栏点
    mCurrentFence++;

    // 向命令队列中添加一条用来设置新围栏点的命令
    // 由于这条命令要交由 GPU处理（即由 GPU端来修改围栏值），所以在 GPU处理完命令队列中此 Signal()
    // 以前的所有命令之前，它并不会设置新的围栏点

    // ID3D12CommandQueue::Signal 方法从 GPU 端设置围栏值，而 ID3D12Fence::Signal 方法则从 CPU 端设置围栏值。
    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

    // 在 CPU 端等待 GPU，直到后者执行完这个围栏点之前的所有命令
    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

        // 若 GPU 命中当前的围栏（即执行到 Signal()指令，修改了围栏值），则激发预定事件
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

        // 等待 GPU 命中围栏，激发事件
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Forward hwnd on because we can get messages (e.g., WM_CREATE)
    // before CreateWindow returns, and thus before mhMainWnd is valid.
    return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

bool D3DApp::InitMainWindow()
{
    // 第一项任务便是通过填写 WNDCLASS 结构体，并根据其中描述的特征来创建一个窗口
    WNDCLASS wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = mhAppInst;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = L"MainWnd";

    // 下一步，我们要在 Windows 系统中为上述 WNDCLASS 注册一个实例，这样一来，即可据此创建窗口
    if (!RegisterClass(&wc))
    {
        MessageBox(0, L"RegisterClass Failed.", 0, 0);
        return false;
    }

    // Compute window rectangle dimensions based on requested client area dimensions.
    RECT R = {0, 0, mClientWidth, mClientHeight};
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
    int width = R.right - R.left;
    int height = R.bottom - R.top;

    // 注册过 WNDCLASS 实例之后，我们就能用 CreateWindow 函数来创建一个窗口了。
    // 此函数返回的就是所建窗口的句柄（一个 HWND 类型的数值）。如果创建失败，返回的句柄数值为 0。
    // 窗口句柄是一种窗口的引用方式，Windows 系统会在内部对此进行管理。处理窗口的大量 Win32 API函数都需要
    // 把 HWND 类型作为参数，以此来确定要对哪一个窗口执行相应的操作
    mhMainWnd = CreateWindow(L"MainWnd",              // 创建此窗口采用的是前面注册的 WNDCLASS 实例
                             mMainWndCaption.c_str(), // 窗口标题
                             WS_OVERLAPPEDWINDOW,     // 窗口的样式标志
                             CW_USEDEFAULT,           // x 坐标
                             CW_USEDEFAULT,           // y 坐标
                             width,                   // 窗口宽度
                             height,                  // 窗口高度
                             0,                       // 父窗口
                             0,                       // 菜单句柄
                             mhAppInst,               // 应用程序实例句柄
                             // 可在此设置一些创建窗口所用的其他参数
                             0);
    if (!mhMainWnd)
    {
        MessageBox(0, L"CreateWindow Failed.", 0, 0);
        return false;
    }
    // 尽管窗口已经创建完毕，但仍没有显示出来。因此，最后一步便是调用下面的两个函数，将刚刚创建
    // 的窗口展示出来并对它进行更新。可以看出，我们为这两个函数都传入了窗口句柄，这样一来，它们
    // 就知道需要展示以及更新的窗口是哪一个
    ShowWindow(mhMainWnd, SW_SHOW);
    UpdateWindow(mhMainWnd);

    return true;
}

int D3DApp::Run()
{
    MSG msg = {0};

    mTimer.Reset();
    // 在获取 WM_QUIT消息之前，该函数会一直保持循环。GetMessage 函数只有在收到 WM_QUIT 消
    // 息时才会返回 0（false），这会造成循环终止；而若发生错误，它便会返回-1。还需注意的一点
    // 是，在未有信息到来之时，GetMessage 函数会令此应用程序线程进入休眠状态
    while (msg.message != WM_QUIT)
    {
        // If there are Window messages then process them.
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // Otherwise, do animation/game stuff.
        else
        {
            mTimer.Tick();

            if (!mAppPaused)
            {
                CalculateFrameStats();
                Update(mTimer);
                Draw(mTimer);
            }
            else
            {
                Sleep(100);
            }
        }
    }

    return (int)msg.wParam;
}

void D3DApp::CalculateFrameStats()
{
    // 这段代码计算了每秒的平均帧数，也计算了每帧的平均渲染时间
    // 这些统计值都会被附加到窗口的标题栏中

    static int frameCnt = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;

    // 以 1 秒为统计周期来计算平均帧数以及每帧的平均渲染时间
    if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
    {
        float fps = (float)frameCnt; // fps = frameCnt / 1
        float mspf = 1000.0f / fps;

        std::wstring fpsStr = std::to_wstring(fps);
        // 计算渲染一帧画面所花费的毫秒数
        std::wstring mspfStr = std::to_wstring(mspf);

        std::wstring windowText = mMainWndCaption + L"  fps: " + fpsStr + L"  mspf: " + mspfStr;
        SetWindowText(mhMainWnd, windowText.c_str());

        // 为计算下一组平均值而重置
        frameCnt = 0;
        timeElapsed += 1.0f;
    }
}

void D3DApp::LogAdapters()
{
    // 通常来说，显示适配器（display adapter）是一种硬件设备（例如独立显卡），
    // 然而系统也可以用软件显示适配器来模拟硬件的图形处理功能。
    // 一个系统中可能会存在数个适配器（比如装有数块显卡）。适配器用接口 IDXGIAdapter 来表示。

    UINT i = 0;
    IDXGIAdapter *adapter = nullptr;
    std::vector<IDXGIAdapter *> adapterList;
    while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        std::wstring text = L"***Adapter: ";
        text += desc.Description;
        text += L"\n";

        OutputDebugString(text.c_str());

        adapterList.push_back(adapter);

        ++i;
    }

    for (auto &item : adapterList)
    {
        LogAdapterOutputs(item);
        ReleaseCom(item);
    }
}

void D3DApp::LogAdapterOutputs(IDXGIAdapter *adapter)
{
    // 另外，一个系统也可能装有数个显示设备。我们称每一台显示设备都是一个显示输出（display output，有的文档
    // 也作 adapter output，适配器输出）实例，用 IDXGIOutput接口来表示。每个适配器都与一组显示输出相关联。
    // 举个例子，考虑这样一个系统，该系统共有两块显卡和 3 台显示器，其中一块显卡与两台显示器相连，
    // 第三台显示器则与另一块显卡相连。在这种情况下，一块适配器与两个显示输出相关联，而另一块则仅有一个显示输出与之关联。
    UINT i = 0;
    IDXGIOutput *output = nullptr;
    while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);

        std::wstring text = L"***Output: ";
        text += desc.DeviceName;
        text += L"\n";
        OutputDebugString(text.c_str());

        LogOutputDisplayModes(output, DXGI_FORMAT_B8G8R8A8_UNORM);

        ReleaseCom(output);

        ++i;
    }
}

void D3DApp::LogOutputDisplayModes(IDXGIOutput *output, DXGI_FORMAT format)
{
    // 在进入全屏模式之时，枚举显示模式就显得尤为重要。为了获得最优的全屏性能，我们所指定的显示模式（包括刷新率）
    // 一定要与显示器支持的显示模式完全匹配。根据枚举出来的显示模式进行选定，便可以保证这一点。

    UINT count = 0;
    UINT flags = 0;

    // 以 nullptr 作为参数调用此函数来获取符合条件的显示模式的个数
    output->GetDisplayModeList(format, flags, &count, nullptr);

    std::vector<DXGI_MODE_DESC> modeList(count);
    output->GetDisplayModeList(format, flags, &count, &modeList[0]);

    for (auto &x : modeList)
    {
        UINT n = x.RefreshRate.Numerator;
        UINT d = x.RefreshRate.Denominator;
        // clang-format off
        std::wstring text =
            L"Width = " + std::to_wstring(x.Width) + L" " +
            L"Height = " + std::to_wstring(x.Height) + L" " +
            L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
            L"\n";
        // clang-format on
        OutputDebugString(text.c_str());
    }
}

void D3DApp::OnResize()
{
    assert(md3dDevice);
    assert(mSwapChain);
    assert(mDirectCmdListAlloc);

    // Flush before changing any resources.
    FlushCommandQueue();

    // --------- Reset

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Release the previous resources we will be recreating.
    for (int i = 0; i < SwapChainBufferCount; ++i)
        mSwapChainBuffer[i].Reset();
    mDepthStencilBuffer.Reset();

    // Resize the swap chain.
    ThrowIfFailed(mSwapChain->ResizeBuffers(SwapChainBufferCount, mClientWidth, mClientHeight, mBackBufferFormat,
                                            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    mCurrBackBuffer = 0;

    // --------- 调整后台缓冲区的大小，并为它创建渲染目标视图

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (UINT i = 0; i < SwapChainBufferCount; i++)
    {
        // 获得交换链内的第 i 个缓冲区
        ThrowIfFailed(mSwapChain->GetBuffer(
            // Buffer：希望获得的特定后台缓冲区的索引（有时后台缓冲区并不只一个，所以需要用索引来指明）。
            i,
            // ppSurface：返回一个指向 ID3D12Resource 接口的指针，这便是希望获得的后台缓冲区。
            IID_PPV_ARGS(&mSwapChainBuffer[i])));

        // 为此缓冲区创建一个 RTV
        md3dDevice->CreateRenderTargetView(
            // pResource：指定用作渲染目标的资源。这里是后台缓冲区（即为后台缓冲区创建了一个渲染目标视图）。
            mSwapChainBuffer[i].Get(),
            // pDesc：指向 D3D12_RENDER_TARGET_VIEW_DESC
            // 数据结构实例的指针。该结构体描述了资源中元素的数据类型（格式）。
            // 如果该资源在创建时已指定了具体格式（即此资源不是无类型格式，not
            // typeless），那么就可以把这个参数设为空指针，表示采用该资源创建时的格式， 为它的第一个 mipmap
            // 层级（后台缓冲区只有一种 mipmap 层级，有关 mipmap 的内容将在第 9 章展开讨论）创建一个视图。
            // 由于已经指定了后台缓冲区的格式，因此就将这个参数设置为空指针。
            nullptr,
            // DestDescriptor：引用所创建渲染目标视图的描述符句柄。
            rtvHeapHandle);
        // 偏移到描述符堆中的下一个缓冲区
        rtvHeapHandle.Offset(1, mRtvDescriptorSize);
    }

    // --------- 创建深度/模板缓冲区及与之关联的深度/模板视图

    // 深度缓冲区其实就是一种 2D 纹理，它存储着离观察者最近的可视对象的深度信息（如果使用了模板，还会附有模板信息）。
    // 纹理是一种 GPU 资源，因此我们要通过填写 D3D12_RESOURCE_DESC 结构体来描述纹理资源，
    // 再用 ID3D12Device::CreateCommittedResource 方法来创建它。

    // 创建深度/模板缓冲区及其视图
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    // 以纹素为单位来表示的纹理宽度。对于缓冲区资源来说，此项是缓冲区占用的字节数。
    depthStencilDesc.Width = mClientWidth;
    depthStencilDesc.Height = mClientHeight; // 以纹素为单位来表示的纹理高度。
    // 以纹素为单位来表示的纹理深度，或者（对于 1D 纹理和 2D 纹理来说）是纹理数组的大小。
    // 注意，Direct3D 中并不存在 3D 纹理数组的概念。
    depthStencilDesc.DepthOrArraySize = 1;
    // mipmap 层级的数量。我们会在第 9 章讲纹理时介绍 mipmap。对于深度/模板缓冲区而言，只能有一个 mipmap 级别。
    depthStencilDesc.MipLevels = 1;
    // DXGI_FORMAT 枚举类型中的成员之一，用于指定纹素的格式。
    // 对于深度/模板缓冲区来说，此格式需要从 4.1.5 节介绍的格式中选择。
    depthStencilDesc.Format = mDepthStencilFormat;
    // 多重采样的质量级别以及对每个像素的采样次数
    // 为了存储每个子像素的颜色和深度/模板信息，所用后台缓冲区和深度缓冲区的大小要 4 倍于屏幕的分辨率。
    // 因此，深度/模板缓冲区与渲染目标的多重采样设置一定要相匹配。
    depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    // D3D12_TEXTURE_LAYOUT 枚举类型的成员之一，用于指定纹理的布局。
    // 我们暂时还不用考虑这个问题，在此将它指定为 D3D12_TEXTURE_LAYOUT_UNKNOWN 即可。
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    // 与资源有关的杂项标志。对于一个深度/模板缓冲区资源来说，要将此项指定为
    // D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL。
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = mDepthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    // 为了使性能达到最佳，通常应将资源放置于默认堆中。
    // 只有在需要使用上传堆或回读堆的特性之时，才选用其他类型的堆。
    CD3DX12_HEAP_PROPERTIES headProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        // （资源欲提交至的）堆所具有的属性。有一些属性是针对高级用法而设。
        &headProperties,
        // 与（资源欲提交至的）堆有关的额外选项标志。
        // 通常将它设为 D3D12_HEAP_FLAG_NONE。
        D3D12_HEAP_FLAG_NONE,
        // 指向一个 D3D12_RESOURCE_DESC 实例的指针，用它描述待建的资源。
        &depthStencilDesc,
        // 不管何时，每个资源都会处于一种特定的使用状态。
        // 在资源创建时，需要用此参数来设置它的初始状态。对于深度/模板缓冲区来说，
        // 通常将其初始状态设置为 D3D12_RESOURCE_STATE_COMMON，再利用
        // ResourceBarrier 方法辅以 D3D12_RESOURCE_STATE_DEPTH_WRITE 状态，
        // 将其转换为可以绑定在渲染流水线上的深度/模板缓冲区。
        D3D12_RESOURCE_STATE_COMMON,
        // 指向一个 D3D12_CLEAR_VALUE 对象的指针，它描述了一个用于清除资源的优化值。选择适当的优化清除值，
        // 可提高清除操作的执行速度。若不希望指定优化清除值，可把此参数设为 nullptr。
        &optClear,
        // 返回一个指向 ID3D12Resource 的指针，即新建的资源
        IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

    // 利用此资源的格式，为整个资源的第 0 mip 层创建描述符
    // 第二个参数是指向 D3D12_DEPTH_STENCIL_VIEW_DESC 结构体的指针。
    // 这个结构体描述了资源中元素的数据类型（格式）。如果资源在创建时已指定了具体格式（即此资源不是无类型格式），
    // 那么就可以把该参数设为空指针，表示以该资源创建时的格式为它的第一个 mipmap 层级创建一个视图
    // （在创建深度/模板缓冲区时就只有一个 mipmap 层级，mipmap 的相关知识将在第 9 章中进行讨论）。
    // 由于我们已经为深度/模板缓冲区设置了具体格式，所以向此参数传入空指针。
    md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, DepthStencilView());

    // 将资源从初始状态转换为深度缓冲区
    auto resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
                                                                D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mCommandList->ResourceBarrier(1, &resourceBarrier);

    // --------- Execute the resize commands.

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList *cmdsLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until resize is complete.
    FlushCommandQueue();

    // --------- 设置视口（viewport）
    mScreenViewport.TopLeftX = 0;
    mScreenViewport.TopLeftY = 0;
    mScreenViewport.Width = static_cast<float>(mClientWidth);
    mScreenViewport.Height = static_cast<float>(mClientHeight);
    // 令深度值保持不变。
    mScreenViewport.MinDepth = 0.0f;
    mScreenViewport.MaxDepth = 1.0f;

    // 命令列表一旦被重置，视口也就需要随之而重置。
    // 第一个参数是要绑定的视口数量（有些高级效果需要使用多个视口），第二个参数是一个指向视口数组的指针。
    //    mCommandList->RSSetViewports(1, &vp);

    // --------- 设置裁剪矩形（scissor rectangle）

    // 我们可以相对于后台缓冲区定义一个裁剪矩形（scissor rectangle），在此矩形外的像素都将被剔除
    // （即这些图像部分将不会被光栅化（rasterize）至后台缓冲区）。这个方法能用于优化程序的性能。
    // 例如，假设已知有一个矩形的 UI（user interface，用户界面）元素覆于屏幕中某块区域的最上层，
    // 那么我们也就无须对 3D 空间中那些被它遮挡的像素进行处理了。

    // 第一个参数是要绑定的裁剪矩形数量（为了实现一些高级效果有时会采用多个裁剪矩形），第二个参数是指向一个裁剪矩形数组的指针。
    // 下面的示例将创建并设置一个覆盖后台缓冲区左上角 1/4 区域的裁剪矩形
    //    mScissorRect = {0, 0, mClientWidth / 2, mClientHeight / 2};
    //    mCommandList->RSSetScissorRects(1, &mScissorRect);
    mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 处理一些特定的消息。注意，在处理完一个消息之后，我们应当返回 0
    switch (msg)
    {
    // WM_ACTIVATE 当一个程序被激活（activate）或进入非活动状态（deactivate）时便会发送此消息。
    // We pause the game when the window is deactivated and unpause it when it becomes active.
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            mAppPaused = true;
            mTimer.Stop();
        }
        else
        {
            mAppPaused = false;
            mTimer.Start();
        }
        return 0;

    // WM_SIZE is sent when the user resizes the window.
    case WM_SIZE:
        // Save the new client area dimensions.
        mClientWidth = LOWORD(lParam);
        mClientHeight = HIWORD(lParam);
        if (md3dDevice)
        {
            if (wParam == SIZE_MINIMIZED)
            {
                mAppPaused = true;
                mMinimized = true;
                mMaximized = false;
            }
            else if (wParam == SIZE_MAXIMIZED)
            {
                mAppPaused = false;
                mMinimized = false;
                mMaximized = true;
                OnResize();
            }
            else if (wParam == SIZE_RESTORED)
            {

                // Restoring from minimized state?
                if (mMinimized)
                {
                    mAppPaused = false;
                    mMinimized = false;
                    OnResize();
                }

                // Restoring from maximized state?
                else if (mMaximized)
                {
                    mAppPaused = false;
                    mMaximized = false;
                    OnResize();
                }
                else if (mResizing)
                {
                    // If user is dragging the resize bars, we do not resize
                    // the buffers here because as the user continuously
                    // drags the resize bars, a stream of WM_SIZE messages are
                    // sent to the window, and it would be pointless (and slow)
                    // to resize for each WM_SIZE message received from dragging
                    // the resize bars.  So instead, we reset after the user is
                    // done resizing the window and releases the resize bars, which
                    // sends a WM_EXITSIZEMOVE message.
                }
                else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
                {
                    OnResize();
                }
            }
        }
        return 0;

    // 当用户抓取调整栏时发送 WM_ENTERSIZEMOVE 消息
    case WM_ENTERSIZEMOVE:
        mAppPaused = true;
        mResizing = true;
        mTimer.Stop();
        return 0;

    // 当用户释放调整栏时发送 WM_EXITSIZEMOVE 消息
    // 此处将根据新的窗口大小重置相关对象（如缓冲区、视图等）
    case WM_EXITSIZEMOVE:
        mAppPaused = false;
        mResizing = false;
        mTimer.Start();
        OnResize();
        return 0;

    // 当窗口被销毁时发送 WM_DESTROY 消息
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    // 当某一菜单处于激活状态，而且用户按下的既不是助记键（mnemonic key）也不是加速键
    // （acceleratorkey）时，就发送 WM_MENUCHAR 消息
    case WM_MENUCHAR:
        // 当按下组合键 alt-enter 时不发出 beep 蜂鸣声
        return MAKELRESULT(0, MNC_CLOSE);

    // 捕获此消息以防窗口变得过小
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lParam)->ptMinTrackSize.x = 200;
        ((MINMAXINFO *)lParam)->ptMinTrackSize.y = 200;
        return 0;

    // 为了在代码中调用我们自己编写的鼠标输入虚函数，要按以下方式来处理与鼠标有关的消息
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_KEYUP:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        else if ((int)wParam == VK_F2)
            Set4xMsaaState(!m4xMsaaState);

        return 0;
    }
    // 将上面没有处理的消息转发给默认的窗口过程。注意，我们自己所编写的窗口过程一定要返回
    // DefWindowProc 函数的返回值
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
