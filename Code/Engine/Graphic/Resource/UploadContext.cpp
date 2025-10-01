#include "UploadContext.hpp"
#include <cassert>
#include <algorithm>
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

namespace enigma::graphic
{
    /**
     * @brief 构造函数 - 创建Upload Heap
     *
     * 教学要点:
     * 1. Upload Heap使用D3D12_HEAP_TYPE_UPLOAD类型
     * 2. 大小需要对齐到64KB边界（DirectX 12要求）
     * 3. Upload Heap状态必须是D3D12_RESOURCE_STATE_GENERIC_READ
     * 4. 创建后立即Map，析构时自动Unmap
     */
    UploadContext::UploadContext(ID3D12Device* device, size_t uploadSize)
        : m_uploadBuffer(nullptr)
          , m_mappedData(nullptr)
          , m_uploadSize(uploadSize)
    {
        if (!device || uploadSize == 0)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UploadContext: Invalid parameters (device=%p, size=%zu)",
                           device, uploadSize);
            return;
        }

        // 1. 对齐到64KB边界
        const size_t alignment = 65536; // 64KB
        m_uploadSize           = (uploadSize + alignment - 1) & ~(alignment - 1);

        // 2. 创建Upload Heap属性
        D3D12_HEAP_PROPERTIES uploadHeapProps = {};
        uploadHeapProps.Type                  = D3D12_HEAP_TYPE_UPLOAD; // CPU可写，GPU可读
        uploadHeapProps.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeapProps.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;
        uploadHeapProps.CreationNodeMask      = 1;
        uploadHeapProps.VisibleNodeMask       = 1;

        // 3. 创建资源描述符（Buffer类型）
        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Alignment           = 0; // 默认对齐
        bufferDesc.Width               = m_uploadSize;
        bufferDesc.Height              = 1;
        bufferDesc.DepthOrArraySize    = 1;
        bufferDesc.MipLevels           = 1;
        bufferDesc.Format              = DXGI_FORMAT_UNKNOWN; // Buffer必须是UNKNOWN
        bufferDesc.SampleDesc.Count    = 1;
        bufferDesc.SampleDesc.Quality  = 0;
        bufferDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // Buffer必须是ROW_MAJOR
        bufferDesc.Flags               = D3D12_RESOURCE_FLAG_NONE;

        // 4. 创建Upload Heap资源
        HRESULT hr = device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, // Upload Heap固定状态
            nullptr,
            IID_PPV_ARGS(&m_uploadBuffer)
        );

        if (FAILED(hr))
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UploadContext: Failed to create upload buffer (size=%zu bytes, hr=0x%08X)",
                           m_uploadSize, hr);
            return;
        }

        // 5. Map Upload Heap到CPU地址空间
        D3D12_RANGE readRange = {0, 0}; // 不读取（CPU只写）
        hr                    = m_uploadBuffer->Map(0, &readRange, &m_mappedData);

        if (FAILED(hr))
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UploadContext: Failed to map upload buffer (hr=0x%08X)",
                           hr);
            m_uploadBuffer.Reset();
            m_mappedData = nullptr;
            return;
        }

        core::LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                       "UploadContext: Created upload buffer (%zu bytes aligned to %zu)",
                       uploadSize, m_uploadSize);
    }

    /**
     * @brief 上传纹理数据
     *
     * 教学要点:
     * 1. 纹理上传需要计算D3D12_PLACED_SUBRESOURCE_FOOTPRINT
     * 2. 使用CopyTextureRegion执行GPU端复制
     * 3. 数据首先复制到Upload Heap，然后GPU复制到Default Heap
     */
    bool UploadContext::UploadTextureData(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource*            destResource,
        const void*                srcData,
        size_t                     dataSize,
        uint32_t                   rowPitch,
        uint32_t                   slicePitch,
        uint32_t                   width,
        uint32_t                   height,
        DXGI_FORMAT                format
    )
    {
        UNUSED(slicePitch)
        if (!IsValid() || !commandList || !destResource || !srcData || dataSize == 0)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UploadTextureData: Invalid parameters");
            return false;
        }

        // 验证数据大小
        if (dataSize > m_uploadSize)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UploadTextureData: Data size (%zu) exceeds upload buffer size (%zu)",
                           dataSize, m_uploadSize);
            return false;
        }

        // 1. 复制数据到Upload Heap
        memcpy(m_mappedData, srcData, dataSize);

        // 2. 计算纹理布局（Footprint）
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        footprint.Offset                             = 0;
        footprint.Footprint.Format                   = format;
        footprint.Footprint.Width                    = width;
        footprint.Footprint.Height                   = height;
        footprint.Footprint.Depth                    = 1;
        footprint.Footprint.RowPitch                 = rowPitch;

        // 3. 配置源位置（Upload Heap）
        D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
        srcLocation.pResource                   = m_uploadBuffer.Get();
        srcLocation.Type                        = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint             = footprint;

        // 4. 配置目标位置（GPU Default Heap）
        D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
        dstLocation.pResource                   = destResource;
        dstLocation.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex            = 0; // 只上传第一个子资源（Mip 0）

        // 5. 执行GPU端复制
        commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

        core::LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                       "UploadTextureData: Uploaded %zu bytes (%ux%u, format=%d)",
                       dataSize, width, height, format);

        return true;
    }

    /**
     * @brief 上传缓冲区数据
     *
     * 教学要点:
     * 1. 缓冲区上传比纹理简单，直接使用CopyBufferRegion
     * 2. 支持部分更新（通过destOffset参数）
     */
    bool UploadContext::UploadBufferData(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource*            destResource,
        const void*                srcData,
        size_t                     dataSize,
        size_t                     destOffset
    )
    {
        if (!IsValid() || !commandList || !destResource || !srcData || dataSize == 0)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UploadBufferData: Invalid parameters");
            return false;
        }

        // 验证数据大小
        if (dataSize > m_uploadSize)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UploadBufferData: Data size (%zu) exceeds upload buffer size (%zu)",
                           dataSize, m_uploadSize);
            return false;
        }

        // 1. 复制数据到Upload Heap
        memcpy(m_mappedData, srcData, dataSize);

        // 2. 执行GPU端复制
        commandList->CopyBufferRegion(
            destResource, // 目标资源
            destOffset, // 目标偏移
            m_uploadBuffer.Get(), // 源资源（Upload Heap）
            0, // 源偏移
            dataSize // 复制大小
        );

        core::LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                       "UploadBufferData: Uploaded %zu bytes (offset=%zu)",
                       dataSize, destOffset);

        return true;
    }
} // namespace enigma::graphic
