#pragma once
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <d3d12.h>
#include <cassert>

namespace enigma::graphic
{
    /**
     * @brief D12IndexBuffer类 - DirectX 12索引缓冲区封装
     *
     * 教学要点:
     * 1. 继承自D12Buffer，利用基类的资源管理能力
     * 2. 添加索引缓冲区特有的格式（Uint16/Uint32）和view管理
     * 3. 提供D3D12_INDEX_BUFFER_VIEW用于Input Assembler绑定
     * 4. 不需要Bindless支持（通过IA阶段直接绑定）
     *
     * 架构设计:
     * - 职责单一: 只管理索引缓冲区特有逻辑（format + view）
     * - 资源管理: 完全继承D12Buffer的RAII和状态管理
     * - 绑定机制: 使用IASetIndexBuffer()而非描述符堆
     * - 类型安全: 编译时确保format和size的正确关系
     *
     * DirectX 12 API参考:
     * - D3D12_INDEX_BUFFER_VIEW: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_index_buffer_view
     * - IASetIndexBuffer: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-iasetindexbuffer
     *
     * ============================================================================
     * Bindless Architecture Interaction (IMPORTANT)
     * ============================================================================
     *
     * IndexBuffers DO NOT require Bindless support because:
     *
     * 1. Binding Mechanism Difference:
     *    - IndexBuffer: Bound via Input Assembler (IA) stage using IASetIndexBuffer()
     *    - Textures/StructuredBuffers: Bound via Descriptor Heaps (require Bindless indices)
     *
     * 2. DirectX 12 Pipeline:
     *    - IndexBuffer → Fixed-function IA stage → Vertex Shader (index fetching)
     *    - No descriptor heap involved, no Bindless index needed
     *
     * 3. GPU Access Pattern:
     *    - IndexBuffer: Sequential/indexed access by IA stage (fixed pipeline)
     *    - Textures: Random access by shaders (requires descriptor table)
     *
     * 4. Exception - Mesh Shaders (Future):
     *    - If using Mesh Shaders, IndexBuffer might be accessed as StructuredBuffer
     *    - In that case, Bindless support would be required
     *    - Current implementation: Traditional graphics pipeline (no Mesh Shader)
     *
     * Therefore, this class does NOT implement:
     * - AllocateBindlessIndexInternal() (returns INVALID_BINDLESS_INDEX)
     * - CreateDescriptorInGlobalHeap() (empty implementation)
     *
     * @see D12Texture for comparison (requires Bindless support)
     * ============================================================================
     */
    class D12IndexBuffer : public D12Buffer
    {
    public:
        /**
         * @brief 索引格式枚举
         *
         * 教学要点:
         * 1. Uint16: 16位索引（0-65535），节省内存
         * 2. Uint32: 32位索引（0-4294967295），支持大模型
         * 3. 对应DXGI_FORMAT_R16_UINT和DXGI_FORMAT_R32_UINT
         */
        enum class IndexFormat
        {
            Uint16, // DXGI_FORMAT_R16_UINT (2字节/索引)
            Uint32 // DXGI_FORMAT_R32_UINT (4字节/索引)
        };

        /**
         * @brief 构造函数：创建索引缓冲区
         * @param size 缓冲区总大小（字节），必须是索引大小的整数倍
         * @param format 索引格式（Uint16或Uint32）
         * @param initialData 初始索引数据（可为nullptr）
         * @param debugName 调试名称
         *
         * 教学要点:
         * 1. 使用BufferUsage::IndexBuffer标志
         * 2. 根据initialData选择MemoryAccess模式
         * 3. 自动创建D3D12_INDEX_BUFFER_VIEW
         * 4. size必须是索引大小的整数倍（Uint16=2字节，Uint32=4字节）
         */
        D12IndexBuffer(size_t size, IndexFormat format, const void* initialData = nullptr, const char* debugName = "IndexBuffer");

        /**
         * @brief 析构函数：自动释放资源
         */
        ~D12IndexBuffer() override = default;

        // ========================================================================
        // 索引缓冲区特有接口
        // ========================================================================

        /**
         * @brief 获取索引格式
         * @return IndexFormat枚举值
         */
        IndexFormat GetFormat() const { return m_format; }

        /**
         * @brief 获取索引数量
         * @return 索引数量（size / 索引大小）
         *
         * 教学要点:
         * - Uint16: size / 2
         * - Uint32: size / 4
         */
        size_t GetIndexCount() const
        {
            return GetSize() / (m_format == IndexFormat::Uint16 ? 2 : 4);
        }

        /**
         * @brief 获取D3D12_INDEX_BUFFER_VIEW（用于绑定到IA阶段）
         * @return IndexBufferView引用
         *
         * 教学要点:
         * 1. 这是DirectX 12绑定索引缓冲区的核心结构
         * 2. 包含GPU地址、大小和格式
         * 3. 传递给IASetIndexBuffer()使用
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
        // IndexBuffer特有成员变量
        // ========================================================================

        IndexFormat             m_format; // 索引格式（Uint16/Uint32）
        D3D12_INDEX_BUFFER_VIEW m_view; // DirectX 12 IndexBufferView

        /**
         * @brief 更新IndexBufferView
         */
        void UpdateView();

        /**
         * @brief IndexFormat转DXGI_FORMAT
         * @param format IndexFormat枚举
         * @return 对应的DXGI_FORMAT
         */
        static DXGI_FORMAT IndexFormatToDXGI(IndexFormat format)
        {
            return format == IndexFormat::Uint16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        }
    };
} // namespace enigma::graphic
