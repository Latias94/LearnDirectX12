#pragma once

#include "d3dUtil.h"

// 实现了上传缓冲区资源的构造与析构函数、处理资源的映射和取消映射操作，
// 还提供了 CopyData 方法来更新缓冲区内的特定元素。在需要通过 CPU 修改上传缓冲区中数据的时候
// （例如，当观察矩阵有了变化），便可以使用 CopyData。
// 注意，此类可用于各种类型的上传缓冲区，而并非只针对常量缓冲区。
// 当用此类管理常量缓冲区时，我们就需要通过构造函数参数 isConstantBuffer 来对此加以描述。
// 另外，如果此类中存储的是常量缓冲区，那么其中的构造函数将自动填充内存，使每个常量缓冲区的大小都成为 256B 的整数倍。
template <typename T>
class UploadBuffer
{
  public:
    UploadBuffer(ID3D12Device *device, UINT elementCount, bool isConstantBuffer) : mIsConstantBuffer(isConstantBuffer)
    {
        mElementByteSize = sizeof(T);
        // 常量缓冲区的大小为 256B 的整数倍。这是因为硬件只能按 m*256B 的偏移量和 n*256B 的数据
        // 长度这两种规格来查看常量数据
        // typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
        // UINT64 OffsetInBytes; // 256 的整数倍
        // UINT SizeInBytes; // 256 的整数倍
        // } D3D12_CONSTANT_BUFFER_VIEW_DESC;
        if (isConstantBuffer)
        {
            mElementByteSize = d3dUtil::CalcConstantBufferByteSize(mElementByteSize);
        }
        // 由于常量缓冲区是用 D3D12_HEAP_TYPE_UPLOAD这种堆类型来创建的，所以我们就能通过 CPU 为常量缓冲区资源更新数据。
        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);
        ThrowIfFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                      IID_PPV_ARGS(&mUploadBuffer)));

        ThrowIfFailed(mUploadBuffer->Map(
            // 第一个参数是子资源（subresource）的索引(关于此处的子资源索引，请参考 12.3.4 节。)，指定了欲映射的子资源。
            // 对于缓冲区来说，它自身就是唯一的子资源，所以我们将此参数设置为 0。
            0,
            // 第二个参数是一个可选项，是个指向 D3D12_RANGE 结构体的指针，
            // 此结构体描述了内存的映射范围，若将该参数指定为空指针，则对整个资源进行映射。
            nullptr,
            // 第三个参数则借助双重指针，返回待映射资源数据的目标内存块。
            reinterpret_cast<void **>(&mMappedData)));
        // 只要还会修改当前的资源，我们就无须取消映射
        // 但是，在资源被 GPU 使用期间，我们千万不可向该资源进行写操作（所以必须借助于同步技术）
    }

    UploadBuffer(const UploadBuffer &rhs) = delete;
    UploadBuffer &operator=(const UploadBuffer &rhs) = delete;
    ~UploadBuffer()
    {
        // 当常量缓冲区更新完成后，我们应在释放映射内存之前对其进行 Unmap（取消映射）操作
        if (mUploadBuffer != nullptr)
            mUploadBuffer->Unmap(0, nullptr);

        mMappedData = nullptr;
    }

    ID3D12Resource *Resource() const
    {
        return mUploadBuffer.Get();
    }

    // 我们利用 memcpy 函数将数据从系统内存（system memory，也就是 CPU 端控制的内存）复制到常量缓冲区
    void CopyData(int dstElementIndex, const T &srcData)
    {
        memcpy(&mMappedData[dstElementIndex * mElementByteSize], &srcData, sizeof(T));
    }

  private:
    ComPtr<ID3D12Resource> mUploadBuffer;
    BYTE *mMappedData = nullptr;

    UINT mElementByteSize = 0;
    bool mIsConstantBuffer = false;
};
