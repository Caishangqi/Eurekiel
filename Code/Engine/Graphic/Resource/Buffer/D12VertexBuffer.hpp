#pragma once
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <d3d12.h>
#include <cassert>

namespace enigma::graphic
{
    /**
     * Typed vertex-buffer wrapper with cached IA view metadata.
     */
    class D12VertexBuffer : public D12Buffer
    {
    public:
        /**
         * Legacy convenience constructor.
         */
        D12VertexBuffer(size_t size, size_t stride, const void* initialData = nullptr, const char* debugName = "VertexBuffer");

        /**
         * Policy-aware creation contract used by the refactored render system.
         */
        explicit D12VertexBuffer(const BufferCreateInfo& createInfo);

        ~D12VertexBuffer() override = default;

        /**
         * Returns the vertex stride in bytes.
         */
        size_t GetStride() const { return m_stride; }

        /**
         * Returns the vertex count derived from size and stride.
         */
        size_t GetVertexCount() const { return GetSize() / m_stride; }

        /**
         * Returns the cached IA view.
         */
        const D3D12_VERTEX_BUFFER_VIEW& GetView() const { return m_view; }

        // ========================================================================
        // 重写基类虚函数 - Debug接口
        // ========================================================================

        /**
         * @brief 设置调试名称
         * @param name 调试名称
         *
         * 教学要点: 调用基类实现，添加顶点缓冲区特有的逻辑（如果需要）
         */
        void SetDebugName(const std::string& name) override;

        /**
         * @brief 获取调试名称
         * @return 调试名称
         */
        const std::string& GetDebugName() const override
        {
            return D12Buffer::GetDebugName();
        }

        /**
         * @brief 获取详细调试信息
         * @return 包含stride和顶点数量的调试信息
         *
         * 教学要点: 返回顶点缓冲区特有的信息
         */
        std::string GetDebugInfo() const override;

    protected:
        // ========================================================================
        // 重写基类虚函数 - Bindless支持（VertexBuffer不需要）
        // ========================================================================

        /**
         * @brief 分配Bindless索引（VertexBuffer不需要）
         * @return INVALID_BINDLESS_INDEX
         *
         * 教学要点:
         * 1. VertexBuffer通过IASetVertexBuffers()绑定，不经过描述符堆
         * 2. 只有作为StructuredBuffer访问时才需要Bindless（Mesh Shader场景）
         * 3. 返回INVALID_BINDLESS_INDEX表示不支持Bindless
         */
        uint32_t AllocateBindlessIndexInternal(BindlessIndexAllocator* allocator) const override
        {
            UNUSED(allocator);
            // VertexBuffer不需要Bindless索引
            return D12Resource::INVALID_BINDLESS_INDEX;
        }

        /**
         * @brief 释放Bindless索引（VertexBuffer不需要）
         * @return true（空操作）
         */
        bool FreeBindlessIndexInternal(BindlessIndexAllocator* allocator, uint32_t index) const override
        {
            UNUSED(allocator);
            UNUSED(index);
            // VertexBuffer不需要释放Bindless索引
            return true;
        }

        /**
         * @brief 在全局描述符堆中创建描述符（VertexBuffer不需要）
         *
         * 教学要点: VertexBuffer不使用描述符堆，空实现即可
         */
        void CreateDescriptorInGlobalHeap(ID3D12Device* device, GlobalDescriptorHeapManager* heapManager) override
        {
            UNUSED(device);
            UNUSED(heapManager);
            // VertexBuffer不需要描述符
        }

        // ========================================================================
        // 重写基类虚函数 - GPU上传支持
        // ========================================================================

        /**
         * @brief 上传顶点数据到GPU
         * @param commandList Copy队列命令列表
         * @param uploadContext Upload Heap上下文
         * @return 成功返回true
         *
         * 教学要点:
         * 1. 调用基类D12Buffer::UploadToGPU()实现
         * 2. 基类已处理缓冲区上传逻辑
         */
        bool UploadToGPU(ID3D12GraphicsCommandList* commandList, UploadContext& uploadContext) override
        {
            return D12Buffer::UploadToGPU(commandList, uploadContext);
        }

        /**
         * @brief 获取上传后的目标状态
         * @return D3D12_RESOURCE_STATE_GENERIC_READ
         *
         * 教学要点:
         * 1. VertexBuffer上传后处于GENERIC_READ状态
         * 2. 适用于顶点着色器读取
         */
        D3D12_RESOURCE_STATES GetUploadDestinationState() const override
        {
            return D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        /**
         * @brief 释放资源
         *
         * 教学要点: 调用基类实现，清理VertexBufferView
         */
        void ReleaseResource() override;

    private:
        size_t                   m_stride; // Vertex stride in bytes
        D3D12_VERTEX_BUFFER_VIEW m_view; // Cached IA view

        /**
         * Rebuilds the cached IA view after resource changes.
         */
        void UpdateView();
    };
} // namespace enigma::graphic
