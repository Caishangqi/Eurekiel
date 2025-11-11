#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

namespace enigma::graphic
{
    /**
     * @brief UploadContext - GPU资源上传辅助类
     *
     * 教学要点:
     * 1. DirectX 12资源上传需要Upload Heap作为中间缓冲区
     * 2. 上传流程: CPU数据 → Upload Heap → GPU Default Heap
     * 3. Upload Heap是CPU可写、GPU可读的特殊堆类型
     * 4. 使用RAII管理Upload Heap生命周期
     *
     * DirectX 12 Upload流程:
     * ```
     * 1. 创建Upload Heap (D3D12_HEAP_TYPE_UPLOAD)
     * 2. Map Upload Heap，写入CPU数据
     * 3. Unmap Upload Heap
     * 4. 使用CopyTextureRegion/CopyBufferRegion复制到GPU Default Heap
     * 5. 添加资源屏障，转换资源状态
     * 6. 执行命令列表
     * 7. 等待GPU完成（或使用Fence异步等待）
     * 8. 释放Upload Heap（可选：保留复用）
     * ```
     *
     * Milestone 2.7设计:
     * - 简化设计：每次上传创建临时Upload Heap
     * - KISS原则：不实现Upload Heap池化（避免过早优化）
     * - 职责单一：只负责数据上传，不管理资源状态
     */
    class UploadContext
    {
    public:
        /**
         * @brief 构造函数 - 创建Upload Heap
         * @param device DirectX 12设备
         * @param uploadSize 上传数据大小（字节）
         *
         * 教学要点:
         * 1. Upload Heap必须对齐到64KB边界
         * 2. D3D12_HEAP_TYPE_UPLOAD堆类型允许CPU写入
         * 3. 创建失败时m_uploadBuffer为nullptr
         */
        UploadContext(ID3D12Device* device, size_t uploadSize);

        /**
         * @brief 析构函数 - 自动释放Upload Heap
         *
         * 教学要点: ComPtr自动管理生命周期，无需手动释放
         */
        ~UploadContext() = default;

        /**
         * @brief 上传纹理数据
         * @param commandList 图形命令列表
         * @param destResource 目标GPU资源（纹理）
         * @param srcData CPU源数据指针
         * @param dataSize 数据大小（字节）
         * @param rowPitch 行间距（字节）
         * @param slicePitch 切片间距（字节）
         * @param width 纹理宽度
         * @param height 纹理高度
         * @param format 纹理格式
         * @return 成功返回true
         *
         * 教学要点:
         * 1. 使用D3D12_PLACED_SUBRESOURCE_FOOTPRINT计算布局
         * 2. CopyTextureRegion执行GPU端复制
         * 3. 资源状态转换由调用者负责（职责分离）
         */
        bool UploadTextureData(
            ID3D12GraphicsCommandList* commandList,
            ID3D12Resource*            destResource,
            const void*                srcData,
            size_t                     dataSize,
            uint32_t                   rowPitch,
            uint32_t                   slicePitch,
            uint32_t                   width,
            uint32_t                   height,
            DXGI_FORMAT                format
        );

        /**
         * @brief 上传缓冲区数据
         * @param commandList 图形命令列表
         * @param destResource 目标GPU资源（缓冲区）
         * @param srcData CPU源数据指针
         * @param dataSize 数据大小（字节）
         * @param destOffset 目标缓冲区偏移（字节）
         * @return 成功返回true
         *
         * 教学要点:
         * 1. 缓冲区上传比纹理简单，只需CopyBufferRegion
         * 2. 支持部分更新（通过destOffset参数）
         * 3. 资源状态转换由调用者负责
         */
        bool UploadBufferData(
            ID3D12GraphicsCommandList* commandList,
            ID3D12Resource*            destResource,
            const void*                srcData,
            size_t                     dataSize,
            size_t                     destOffset = 0
        );

        /**
         * @brief 检查UploadContext是否有效
         * @return Upload Heap创建成功返回true
         */
        bool IsValid() const { return m_uploadBuffer != nullptr; }

        /**
         * @brief 获取Upload Heap指针（调试用）
         * @return Upload Heap资源指针
         */
        ID3D12Resource* GetUploadBuffer() const { return m_uploadBuffer.Get(); }

    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer; // Upload Heap资源
        void*                                  m_mappedData; // CPU映射指针
        size_t                                 m_uploadSize; // Upload Heap大小

        // 禁用拷贝和赋值（Upload Heap不应被拷贝）
        UploadContext(const UploadContext&)            = delete;
        UploadContext& operator=(const UploadContext&) = delete;

        // 支持移动语义（允许转移所有权）
        UploadContext(UploadContext&&)            = default;
        UploadContext& operator=(UploadContext&&) = default;
    };
} // namespace enigma::graphic
