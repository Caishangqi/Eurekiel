#pragma once
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <d3d12.h>
#include <cassert>

namespace enigma::graphic
{
    /**
     * @brief D12VertexBuffer类 - DirectX 12顶点缓冲区封装
     *
     * 教学要点:
     * 1. 继承自D12Buffer，利用基类的资源管理能力
     * 2. 添加顶点缓冲区特有的stride和view管理
     * 3. 提供D3D12_VERTEX_BUFFER_VIEW用于Input Assembler绑定
     * 4. 不需要Bindless支持（通过IA阶段直接绑定）
     *
     * 架构设计:
     * - 职责单一: 只管理顶点缓冲区特有逻辑（stride + view）
     * - 资源管理: 完全继承D12Buffer的RAII和状态管理
     * - 绑定机制: 使用IASetVertexBuffers()而非描述符堆
     * - 类型安全: 编译时确保stride和size的正确关系
     *
     * DirectX 12 API参考:
     * - D3D12_VERTEX_BUFFER_VIEW: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_vertex_buffer_view
     * - IASetVertexBuffers: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-iasetvertexbuffers
     *
     * ============================================================================
     * Bindless Architecture Interaction (IMPORTANT)
     * ============================================================================
     *
     * VertexBuffers DO NOT require Bindless support because:
     *
     * 1. Binding Mechanism Difference:
     *    - VertexBuffer: Bound via Input Assembler (IA) stage using IASetVertexBuffers()
     *    - Textures/StructuredBuffers: Bound via Descriptor Heaps (require Bindless indices)
     *
     * 2. DirectX 12 Pipeline:
     *    - VertexBuffer → Fixed-function IA stage → Vertex Shader
     *    - No descriptor heap involved, no Bindless index needed
     *
     * 3. GPU Access Pattern:
     *    - VertexBuffer: Sequential access by IA stage (fixed pipeline)
     *    - Textures: Random access by shaders (requires descriptor table)
     *
     * 4. Exception - Mesh Shaders (Future):
     *    - If using Mesh Shaders, VertexBuffer needs to be accessed as StructuredBuffer
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
    class D12VertexBuffer : public D12Buffer
    {
    public:
        /**
         * @brief 构造函数：创建顶点缓冲区
         * @param size 缓冲区总大小（字节），必须是stride的整数倍
         * @param stride 单个顶点的大小（字节）
         * @param initialData 初始顶点数据（可为nullptr）
         * @param debugName 调试名称
         *
         * 教学要点:
         * 1. 使用BufferUsage::VertexBuffer标志
         * 2. 根据initialData选择MemoryAccess模式:
         *    - 有数据: CPUToGPU (需要上传)
         *    - 无数据: GPUOnly (最高性能)
         * 3. 自动创建D3D12_VERTEX_BUFFER_VIEW
         * 4. stride必须 > 0，size必须是stride的整数倍
         */
        D12VertexBuffer(size_t size, size_t stride, const void* initialData = nullptr, const char* debugName = "VertexBuffer");

        /**
         * @brief 析构函数：自动释放资源
         *
         * 教学要点: 基类D12Buffer已处理资源释放，这里无需额外操作
         */
        ~D12VertexBuffer() override = default;

        // ========================================================================
        // 顶点缓冲区特有接口
        // ========================================================================

        /**
         * @brief 获取顶点stride（单个顶点大小）
         * @return stride（字节）
         */
        size_t GetStride() const { return m_stride; }

        /**
         * @brief 获取顶点数量
         * @return 顶点数量（size / stride）
         */
        size_t GetVertexCount() const { return GetSize() / m_stride; }

        /**
         * @brief 获取D3D12_VERTEX_BUFFER_VIEW（用于绑定到IA阶段）
         * @return VertexBufferView引用
         *
         * 教学要点:
         * 1. 这是DirectX 12绑定顶点缓冲区的核心结构
         * 2. 包含GPU地址、大小和stride
         * 3. 传递给IASetVertexBuffers()使用
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
        // ========================================================================
        // VertexBuffer特有成员变量
        // ========================================================================

        size_t                   m_stride; // 单个顶点大小（字节）
        D3D12_VERTEX_BUFFER_VIEW m_view; // DirectX 12 VertexBufferView

        /**
         * @brief 更新VertexBufferView
         *
         * 教学要点:
         * 1. 根据m_resource更新m_view
         * 2. 在构造函数和资源创建后调用
         */
        void UpdateView();
    };
} // namespace enigma::graphic
