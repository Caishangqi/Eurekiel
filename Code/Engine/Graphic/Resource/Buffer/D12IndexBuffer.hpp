#pragma once
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <d3d12.h>
#include <cassert>

namespace enigma::graphic
{
    /**
     * Typed index-buffer wrapper with cached IA view metadata.
     */
    class D12IndexBuffer : public D12Buffer
    {
    public:
        // ========================================================================
        // [SIMPLIFIED] Index format is always Uint32 (4 bytes per index)
        // 
        // Design Decision:
        //   - Modern GPUs have abundant memory, Uint32 is standard
        //   - Simplifies Ring Buffer and immediate mode rendering
        //   - Eliminates format mismatch errors
        //   - Consistent with sizeof(unsigned) = 4 bytes
        // ========================================================================

        static constexpr size_t INDEX_SIZE = sizeof(uint32_t); // 4 bytes

        /**
         * Legacy convenience constructor.
         */
        D12IndexBuffer(size_t size, const void* initialData = nullptr, const char* debugName = "IndexBuffer");

        /**
         * Policy-aware creation contract used by the refactored render system.
         */
        explicit D12IndexBuffer(const BufferCreateInfo& createInfo);

        ~D12IndexBuffer() override = default;

        // ========================================================================
        // Index Buffer Specific Interface
        // ========================================================================

        /**
         * @brief Get index count
         * @return Number of indices (size / 4)
         */
        size_t GetIndexCount() const
        {
            return GetSize() / INDEX_SIZE;
        }

        /**
         * Returns the cached IA view.
         */
        const D3D12_INDEX_BUFFER_VIEW& GetView() const { return m_view; }

        // ========================================================================
        // 重写基类虚函数 - Debug接口
        // ========================================================================

        void SetDebugName(const std::string& name) override;

        const std::string& GetDebugName() const override
        {
            return D12Buffer::GetDebugName();
        }

        std::string GetDebugInfo() const override;

    protected:
        // ========================================================================
        // 重写基类虚函数 - Bindless支持（IndexBuffer不需要）
        // ========================================================================

        uint32_t AllocateBindlessIndexInternal(BindlessIndexAllocator* allocator) const override
        {
            UNUSED(allocator);
            // IndexBuffer不需要Bindless索引
            return D12Resource::INVALID_BINDLESS_INDEX;
        }

        bool FreeBindlessIndexInternal(BindlessIndexAllocator* allocator, uint32_t index) const override
        {
            UNUSED(allocator);
            UNUSED(index);
            return true;
        }

        void CreateDescriptorInGlobalHeap(ID3D12Device* device, GlobalDescriptorHeapManager* heapManager) override
        {
            UNUSED(device);
            UNUSED(heapManager);
            // IndexBuffer不需要描述符
        }

        // ========================================================================
        // 重写基类虚函数 - GPU上传支持
        // ========================================================================

        bool UploadToGPU(ID3D12GraphicsCommandList* commandList, UploadContext& uploadContext) override
        {
            return D12Buffer::UploadToGPU(commandList, uploadContext);
        }

        D3D12_RESOURCE_STATES GetUploadDestinationState() const override
        {
            return D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        void ReleaseResource() override;

    private:
        // ========================================================================
        // IndexBuffer Member Variables
        // ========================================================================

        D3D12_INDEX_BUFFER_VIEW m_view; // DirectX 12 IndexBufferView

        /**
         * @brief Update IndexBufferView
         */
        void UpdateView();
    };
} // namespace enigma::graphic
