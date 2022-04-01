#include "d3dUtil.h"
#include "DXTrace.h"

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

    heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    // 为了将  CPU 端内存中的数据复制到默认缓冲区，我们还需要创建一个处于中介位置的上传堆
    ThrowIfFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                  IID_PPV_ARGS(uploadBuffer.GetAddressOf())));
    // 描述我们希望复制到默认缓冲区中的数据
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    // 指向某个系统内存块的指针，其中有初始化缓冲区所用的数据。如果欲初始化的缓冲
    // 区能够存储 n 个顶点数据，则该系统内存块必定可容纳至少 n 个顶点数据，以此来初始化整个缓冲区。
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
