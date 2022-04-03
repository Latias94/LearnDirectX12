#include "d3dUtil.h"

bool d3dUtil::IsKeyDown(int vkeyCode)
{
    return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0;
}

ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(ID3D12Device *device, ID3D12GraphicsCommandList *cmdList,
                                                    const void *initData, UINT64 byteSize,
                                                    ComPtr<ID3D12Resource> &uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    // 创建实际的默认缓冲区资源
    ThrowIfFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                  D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                  IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    // 为了将 CPU 端内存中的数据复制到默认缓冲区，我们还需要创建一个处于中介位置的上传堆
    ThrowIfFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                  IID_PPV_ARGS(uploadBuffer.GetAddressOf())));
    // 描述我们希望复制到默认缓冲区中的数据
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    // 指向某个系统内存块的指针，其中有初始化缓冲区所用的数据。如果欲初始化的缓冲区能够存储 n 个顶点数据，
    // 则该系统内存块必定可容纳至少 n 个顶点数据，以此来初始化整个缓冲区。
    subResourceData.pData = initData;
    // 对于缓冲区而言，此参数为欲复制数据的字节数。
    subResourceData.RowPitch = byteSize;
    // 对于缓冲区而言，此参数亦为欲复制数据的字节数
    subResourceData.SlicePitch = subResourceData.RowPitch;
    // 将数据复制到默认缓冲区资源的流程
    // UpdateSubresources 辅助函数会先将数据从 CPU 端的内存中复制到位于中介位置的上传堆里接着，
    // 再通过调用 ID3D12CommandList::CopySubresourceRegion 函数，把上传堆内的数据复制到 mBuffer 中
    auto resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
                                                                D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &resourceBarrier);

    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

    resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                                           D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &resourceBarrier);
    // 注意：在调用上述函数后，必须保证 uploadBuffer 依然存在，而不能对它立即进行销毁。这是因为
    // 命令列表中的复制操作可能尚未执行。待调用者得知复制完成的消息后，方可释放  uploadBuffer
    return defaultBuffer;
}

// 加载离线编译的 Shader 字节码（.cso）
ComPtr<ID3DBlob> d3dUtil::LoadBinary(const std::wstring &filename)
{
    std::ifstream fin(filename, std::ios::binary);

    fin.seekg(0, std::ios_base::end);
    std::ifstream::pos_type size = (int)fin.tellg();
    fin.seekg(0, std::ios_base::beg);

    ComPtr<ID3DBlob> blob;
    ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

    fin.read((char *)blob->GetBufferPointer(), size);
    fin.close();

    return blob;
}

// 运行时对着色器进行编译
ComPtr<ID3DBlob> d3dUtil::CompileShader(const std::wstring &filename, const D3D_SHADER_MACRO *defines,
                                        const std::string &entrypoint, const std::string &target)
{
    // 若处于调试模式,则使用调试标志
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(
        // 我们希望编译的以.hlsl 作为扩展名的 HLSL 源代码文件。
        filename.c_str(),
        // 在本书中，我们并不使用这个高级选项，因此总是将它指定为空指针。关于此参数的详细信息可参见 SDK 文档。
        defines,
        // 在本书中，我们并不使用这个高级选项，因此总是将它指定为空指针。关于此参数的详细信息可参见 SDK 文档。
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        // 着色器的入口点函数名。一个.hlsl 文件可能存有多个着色器程序
        // （例如，一个顶点着色器和一个像素着色器），所以我们需要为待编译的着色器指定入口点。
        entrypoint.c_str(),
        // 指定所用着色器类型和版本的字符串。在本书中，我们采用的着色器模型版本是 5.0 和 5.1。
        // 可以从 msdn 与微软 GitHub 上 DirectXShaderCompiler 项目的示例及 wiki
        // 文档中获得更多相关信息（使用该版本着色器模型有 SDK 版本与特性级别等限制）。
        target.c_str(),
        // 指示对着色器代码应当如何编译的标志。在 SDK 文档里，这些标志列出得不少，但是此书中我们仅用两种。
        // a）D3DCOMPILE_DEBUG：用调试模式来编译着色器。
        // b）D3DCOMPILE_SKIP_OPTIMIZATION：指示编译器跳过优化阶段（对调试很有用处）。
        compileFlags,
        // 我们不会用到处理效果文件的高级编译选项，关于它的信息请参见 SDK 文档。
        0,
        // 返回一个指向 ID3DBlob 数据结构的指针，它存储着编译好的着色器对象字节码。
        &byteCode,
        // 返回一个指向 ID3DBlob 数据结构的指针。如果在编译过程中发生了错误，它便会储存报错的字符串。
        &errors);

    // ID3DBlob 类型描述的其实就是一段普通的内存块，这是该接口的两个方法：
    // a）LPVOID GetBufferPointer：返回指向 ID3DBlob 对象中数据的 void* 类型的指针。由此
    // 可见，在使用此数据之前务必先要将它转换为适当的类型（参考下面的示例）。
    // b）SIZE_T GetBufferSize：返回缓冲区的字节大小（即该对象中的数据大小）。

    // 将错误信息输出到调试窗口
    if (errors != nullptr)
    {
        OutputDebugStringA((char *)errors->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    return byteCode;
}
