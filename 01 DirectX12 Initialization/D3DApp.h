#pragma once

#include "DXTrace.h"
#include "GameTimer.h"
#include "d3dx12.h"
#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <string>
#include <wrl/client.h>

class D3DApp
{
  protected:
    D3DApp(HINSTANCE hInstance); // 在构造函数的初始化列表应当设置好初始参数
    D3DApp(const D3DApp &rhs) = delete;
    D3DApp &operator=(const D3DApp &rhs) = delete;

    // 这个析构函数用于释放 D3DApp 中所用的 COM 接口对象并刷新命令队列。
    // 在析构函数中刷新命令队列的原因是：在销毁 GPU 引用的资源以前，必须等待 GPU 处理完队列中的
    // 所有命令。否则，可能造成应用程序在退出时崩溃。
    virtual ~D3DApp();

  public:
    static D3DApp *GetApp();

    HINSTANCE AppInst() const; // 获取应用实例的句柄
    HWND MainWnd() const;      // 获取主窗口句柄
    float AspectRatio() const; // 获取后台缓冲区的宽度与高度之比

    bool Get4xMsaaState() const;     // 获取是否开启 4X MSAA
    void Set4xMsaaState(bool value); // 开启或禁用 4X MSAA 功能

    // 这个方法封装了应用程序的消息循环。
    // 它使用的是 Win32 的 PeekMessage 函数，当没有窗口消息到来时就会处理我们的游戏逻辑部分。
    int Run(); // 运行程序，进行游戏主循环

    virtual bool Initialize();
    // 该方法用于实现应用程序主窗口的窗口过程函数（procedure function）。一般来说，
    //如 果需要处理在 D3DApp::MsgProc 中没有得到处理（或者不能如我们所愿进行处理）的消息，
    // 只要重写此方法即可。该方法的实现在 4.5.5 节中有相应的讲解。此外，如果对该方法进行了重写，
    //那么其中并未处理的消息都应当转交至 D3DApp::MsgProc。
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  protected:
    // 此虚函数用于创建应用程序所需的 RTV 和 DSV 描述符堆。
    // 默认的实现是创建一个含有 SwapChainBufferCount 个 RTV 描述符的 RTV 堆（为交换链中的缓冲区而创建），
    // 以及具有一个 DSV 描述符的 DSV 堆（为深度/模板缓冲区而创建）。
    virtual void CreateRtvAndDsvDescriptorHeaps();

    // 当 D3DApp::MsgProc 函数接收到 WM_SIZE 消息时便会调用此方法。若窗口的大小发生了改变，
    // 一些与工作区大小有关的 Direct3D 属性也需要随之调整。特别是后台缓冲区以及深度/模板缓冲区，
    // 为了匹配窗口工作区调整后的大小需要对其重新创建。我们可以通过调用 IDXGISwapChain::ResizeBuffers
    // 方法来调整后台缓冲区的尺寸。
    // 对于深度/模板缓冲区而言，则需要在销毁后根据新的工作区大小进行重建。另外，渲染目标和深度/模板的视图也应重新创建。
    // D3DApp 类中 OnResize 方法实现的功能即为调整后台缓冲区和深度/模板缓冲区的尺寸，
    // 我们可直接查阅其源代码来研究相关细节。除了这些缓冲区以外，依赖于工作区大小的其他属性
    // （如投影矩阵，projection matrix）也要在此做相应的修改。
    // 由于在调整窗口大小时，客户端代码可能还需执行一些它自己的逻辑代码，因此该方法亦属于框架的一部分。
    virtual void OnResize();
    // 在绘制每一帧时都会调用该抽象方法，我们通过它来随着时间的推移而更新 3D 应用
    // 程序（如呈现动画、移动摄像机、做碰撞检测以及检查用户的输入等）。
    virtual void Update(const GameTimer &gt) = 0;
    // 在绘制每一帧时都会调用的抽象方法。我们在该方法中发出渲染命令，将当前帧真正地绘制到后台缓冲区中。
    // 当完成帧的绘制后，再调用 IDXGISwapChain::Present 方法将后台缓冲区的内容显示在屏幕上。
    virtual void Draw(const GameTimer &gt) = 0;

    // 便于重写鼠标输入消息的处理流程
    // 若希望处理鼠标消息，我们只需重写这几种方法，而不必重写 MsgProc 方法。
    virtual void OnMouseDown(WPARAM btnState, int x, int y)
    {
    }

    virtual void OnMouseUp(WPARAM btnState, int x, int y)
    {
    }

    virtual void OnMouseMove(WPARAM btnState, int x, int y)
    {
    }

  protected:
    bool InitMainWindow(); // 该父类方法需要初始化窗口
    bool InitDirect3D();   // 该父类方法需要初始化 Direct3D
    // 依 4.3.4 节中所述的流程创建命令队列、命令列表分配器和命令列表。
    void CreateCommandObjects();
    void CreateSwapChain(); // 创建交换链（参见 4.3.5 节）。

    // 强制 CPU 等待 GPU，直到 GPU 处理完队列中所有的命令（详见 4.2.2 节）
    void FlushCommandQueue();

    // 返回交换链中当前后台缓冲区的 ID3D12Resource
    ID3D12Resource *CurrentBackBuffer() const
    {
        return mSwapChainBuffer[mCurrBackBuffer].Get();
    }

    // 返回当前后台缓冲区的 RTV（渲染目标视图，render target view）
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const
    {
        // CD3DX12 构造函数根据给定的偏移量找到当前后台缓冲区的 RTV
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), // 堆中的首个句柄
                                             mCurrBackBuffer,     // 偏移至后台缓冲区描述符句柄的索引
                                             mRtvDescriptorSize); // 描述符所占字节的大小
    }

    // 返回主深度/模板缓冲区的 DSV（深度/模板视图，depth/stencil view）
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const
    {
        return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
    }

    // 计算每秒的平均帧数以及每帧平均的毫秒时长。实现方法将在 4.5.4 节中讨论。
    void CalculateFrameStats();

    void LogAdapters(); // 枚举系统中所有的适配器（参见 4.1.10 节）
    // 枚举指定适配器的全部显示输出（参见 4.1.10 节）
    void LogAdapterOutputs(IDXGIAdapter *adapter);
    // 枚举某个显示输出对特定格式支持的所有显示模式（参见 4.1.10 节）
    void LogOutputDisplayModes(IDXGIOutput *output, DXGI_FORMAT format);

  protected:
    static D3DApp *mApp;

    HINSTANCE mhAppInst = nullptr; // 应用程序实例句柄
    HWND mhMainWnd = nullptr;      // 主窗口句柄
    bool mAppPaused = false;       // 应用程序是否暂停
    bool mMinimized = false;       // 应用程序是否最小化
    bool mMaximized = false;       // 应用程序是否最大化
    bool mResizing = false;        // 大小调整栏是否受到拖拽
    bool mFullscreenState = false; // 是否开启全屏模式

    // 若将该选项设置为 true，则使用 4X MSAA 技术(参见 4.1.8 节)。默认值为 false
    bool m4xMsaaState = false; // 是否开启 4X MSAA
    UINT m4xMsaaQuality = 0;   // 4X MSAA 的质量级别

    // 用于记录"delta-time"（帧之间的时间间隔）和游戏总时间(参见 4.4 节)
    GameTimer mTimer;

    // 使用模板别名(C++11)简化类型名
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<IDXGIFactory4> mdxgiFactory;
    ComPtr<IDXGISwapChain> mSwapChain;
    ComPtr<ID3D12Device> md3dDevice;

    ComPtr<ID3D12Fence> mFence;
    UINT64 mCurrentFence = 0;

    ComPtr<ID3D12CommandQueue> mCommandQueue;
    ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
    ComPtr<ID3D12GraphicsCommandList> mCommandList;

    static const int SwapChainBufferCount = 2;
    // 用来记录当前后台缓冲区的索引（由于利用页面翻转技术来交换前台缓冲区和后台缓冲区，
    // 所以我们需要对其进行记录，以便搞清楚哪个缓冲区才是当前正在用于渲染数据的后台缓冲区）。
    int mCurrBackBuffer = 0;
    // ID3D12Resource 接口将物理内存与堆资源抽象组织为可处理的数据数组与多维数据，从而使 CPU 与 GPU
    // 可以对这些资源进行读写。
    ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
    ComPtr<ID3D12Resource> mDepthStencilBuffer;

    ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    ComPtr<ID3D12DescriptorHeap> mDsvHeap;

    D3D12_VIEWPORT mScreenViewport;

    D3D12_RECT mScissorRect;

    // RTV 描述符表示的是渲染目标视图资源（render target view）
    UINT mRtvDescriptorSize = 0;
    // DSV 描述符表示的是深度/模板视图资源（depth/stencil view）
    UINT mDsvDescriptorSize = 0;
    // CBV/SRV/UAV  描述符分别表示的是常量缓冲区视图（constant buffer view）、着色器资源视图
    //（shader resource view）和无序访问视图（unordered access view）这 3 种资源。
    UINT mCbvSrvUavDescriptorSize = 0;

    // 用户应该在派生类的派生构造函数中自定义这些初始值
    std::wstring mMainWndCaption = L"d3d App";
    D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    int mClientWidth = 800;
    int mClientHeight = 600;
};
