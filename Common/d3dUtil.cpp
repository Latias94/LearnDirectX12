#include "d3dUtil.h"
#include "DXTrace.h"

Microsoft::WRL::ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(
    ID3D12Device *device, ID3D12GraphicsCommandList *cmdList, const void *initData, UINT64 byteSize,
    Microsoft::WRL::ComPtr<ID3D12Resource> &uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;
    // 创建实际的默认缓冲区资源
    ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                  D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
                                                  D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                  IID_PPV_ARGS(defaultBuffer.GetAddressOf())));
    // 为了将  CPU 端内存中的数据复制到默认缓冲区，我们还需要创建一个处于中介位置的上传堆
    ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                  D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                  IID_PPV_ARGS(uploadBuffer.GetAddressOf())));
    // 描述我们希望复制到默认缓冲区中的数据
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;
    // 将数据复制到默认缓冲区资源的流程
    // UpdateSubresources 辅助函数会先将数据从 CPU 端的内存中复制到位于中介位置的上传堆里接着，
    // 再通过调用 ID3D12CommandList::CopySubresourceRegion 函数，把上传堆内的数据复制到
    // mBuffer 中①
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
                                                                      D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
    cmdList->ResourceBarrier(1,
                             &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                                                   D3D12_RESOURCE_STATE_GENERIC_READ));
    // 注意：在调用上述函数后，必须保证  uploadBuffer 依然存在，而不能对它立即进行销毁。这是因为
    // 命令列表中的复制操作可能尚未执行。待调用者得知复制完成的消息后，方可释放  uploadBuffer
    return defaultBuffer;
}
