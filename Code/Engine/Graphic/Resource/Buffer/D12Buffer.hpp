#pragma once

#include "../D12Resources.hpp"
#include <d3d12.h>
#include <memory>
#include <cstdint>

#include "Engine/Graphic/Resource/BindlessIndexAllocator.hpp"

namespace enigma::graphic
{
    /**
     * Teaching Objective: Understand the Modern Methods of DirectX 12 Buffer Management
     *
     * D12Buffer class design description:
     * - DirectX 12 implementation corresponding to Iris' ShaderStorageBuffer.java
     * - Manage GPU buffer resources, including vertex buffers, index buffers, constant buffers, etc.
     * - Manage resource life cycles with RAII
     * - Supports different purpose buffers that are visible to CPU and GPU-specific
     *
     * DirectX 12 API Reference:
     * - ID3D12Resource: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12resource
     * - D3D12_RESOURCE_DESC: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_desc
     * - D3D12_HEAP_PROPERTIES: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_heap_properties
     */

    /**
     * Buffer usage flag enumeration
     * D3D12_RESOURCE_USAGE and D3D12_RESOURCE_FLAGS corresponding to DirectX 12
     */
    enum class BufferUsage : uint32_t
    {
        VertexBuffer = 0x1, // Vertex Buffer
        IndexBuffer = 0x2, // Index buffer
        ConstantBuffer = 0x4, // Constant buffer (corresponding to Iris' Uniform Buffer)
        StructuredBuffer = 0x8, // Structured Buffer (corresponding to Iris' SSBO)
        StorageBuffer = 0x10, // Read and write storage buffer
        IndirectBuffer = 0x20, // Indirectly draw the parameter buffer

        // Combination flag
        Default = VertexBuffer | IndexBuffer,
        AllBufferTypes = VertexBuffer | IndexBuffer | ConstantBuffer | StructuredBuffer | StorageBuffer | IndirectBuffer
    };

    /**
     * Controls CPU/GPU visibility for the backing resource.
     */
    enum class MemoryAccess
    {
        GPUOnly, // GPU-specific, highest performance (DEFAULT heap)
        CPUToGPU, // CPU write, GPU read (UPLOAD heap)
        GPUToCPU, // GPU write, CPU read (READBACK heap)
        CPUWritable // The CPU can write frequently (corresponding to Iris' dynamic buffer)
    };

    /**
     * Declares the intended lifetime and upload model of a buffer.
     */
    enum class BufferUsagePolicy : uint8_t
    {
        DynamicUpload,
        GpuOnlyCopyReady
    };

    /**
     * Generic buffer creation contract.
     */
    struct BufferCreateInfo
    {
        size_t            size; // Buffer size in bytes
        BufferUsage       usage; // Usage flags
        MemoryAccess      memoryAccess; // Requested memory access mode
        const void*       initialData; // Optional initial data
        const char*       debugName; // Optional debug name
        size_t            byteStride; // Structured-buffer element size in bytes
        BufferUsagePolicy usagePolicy; // Upload/update policy

        BufferCreateInfo()
            : size(0)
            , usage(BufferUsage::Default)
            , memoryAccess(MemoryAccess::GPUOnly)
            , initialData(nullptr)
            , debugName(nullptr)
            , byteStride(0)
            , usagePolicy(BufferUsagePolicy::DynamicUpload)
        {
        }
    };

    /**
     * D12Buffer类：DirectX 12缓冲区的封装，继承D12Resource
     * 设计理念：对应Iris的ShaderStorageBuffer，但适配DirectX 12的资源模型
     */
    class D12Buffer : public D12Resource
    {
    public:
        /**
         * 构造函数：根据创建信息初始化缓冲区
         * @param createInfo 缓冲区创建参数
         */
        explicit D12Buffer(const BufferCreateInfo& createInfo);

        /**
         * 析构函数：自动释放DirectX 12资源
         */
        ~D12Buffer() override;

        // 禁用拷贝构造和赋值（RAII原则）F:\p4\Personal\SD\Engine\Code\Engine\Graphic
        D12Buffer(const D12Buffer&)            = delete;
        D12Buffer& operator=(const D12Buffer&) = delete;

        // 支持移动语义
        D12Buffer(D12Buffer&& other) noexcept;
        D12Buffer& operator=(D12Buffer&& other) noexcept;

        /**
         * 获取DirectX 12资源接口（继承自D12Resource）
         * @return ID3D12Resource指针，用于绑定到渲染管线
         */
        // GetResource() 已在基类D12Resource中定义

        /**
         * 获取缓冲区大小（继承自D12Resource）
         * @return 缓冲区字节大小
         */
        // GetSize() 已在基类D12Resource中定义

        /**
         * 获取缓冲区使用标志
         * @return BufferUsage枚举值
         */
        BufferUsage GetUsage() const { return m_usage; }

        /**
         * Returns the declared upload/update policy.
         */
        BufferUsagePolicy GetUsagePolicy() const { return m_usagePolicy; }

        /**
         * Returns the resolved memory access mode.
         */
        MemoryAccess GetMemoryAccess() const { return m_memoryAccess; }

        /**
         * Returns whether direct CPU mapping is allowed for this buffer.
         */
        bool SupportsCpuMapping() const;

        /**
         * Returns whether persistent mapping is allowed for this buffer.
         */
        bool SupportsPersistentMapping() const;

        /**
         * 获取GPU虚拟地址（继承自D12Resource）
         * DirectX 12 API: ID3D12Resource::GetGPUVirtualAddress()
         * @return 用于bindless资源绑定的GPU地址
         */
        // GetGPUVirtualAddress() 已在基类D12Resource中定义

        /**
         * 映射CPU内存（仅适用于CPU可访问的缓冲区）
         * DirectX 12 API: ID3D12Resource::Map()
         * @param readRange 读取范围（可为nullptr）
         * @return 映射的CPU内存指针
         */
        void* Map(const D3D12_RANGE* readRange = nullptr);

        /**
         * 取消映射CPU内存
         * DirectX 12 API: ID3D12Resource::Unmap()
         * @param writtenRange 写入范围（可为nullptr）
         */
        void Unmap(const D3D12_RANGE* writtenRange = nullptr);

        /**
         * 持久映射CPU内存（DirectX 12推荐模式）
         * 在缓冲区生命周期内保持映射状态，避免每次draw都Map/Unmap的开销
         * 只适用于HEAP_TYPE_UPLOAD的缓冲区（CPUToGPU或CPUWritable）
         * @return 持久映射的CPU内存指针，失败返回nullptr
         */
        void* MapPersistent();

        /**
         * 取消持久映射
         * 释放持久映射的CPU内存，通常在析构时调用
         */
        void UnmapPersistent();

        /**
         * 获取持久映射的CPU内存指针
         * @return 如果已持久映射返回有效指针，否则返回nullptr
         */
        void* GetPersistentMappedData() const;

        /**
         * 检查缓冲区是否处于持久映射状态
         * @return 已持久映射返回true，否则返回false
         */
        bool IsPersistentlyMapped() const { return m_isPersistentlyMapped; }

        /**
         * @brief 重写虚函数：设置调试名称并添加缓冲区特定逻辑
         * @param name 调试名称
         *
         * 教学要点: 重写基类虚函数，添加缓冲区特定的调试信息设置
         * 可以在设置名称的同时更新缓冲区相关的调试标记
         */
        void SetDebugName(const std::string& name) override;

        /**
         * @brief 重写虚函数：获取包含缓冲区大小信息的调试名称
         * @return 格式化的调试名称字符串
         *
         * 教学要点: 重写基类虚函数，返回包含缓冲区特定信息的名称
         * 格式: "BufferName (Size: XXX bytes, Usage: YYY)"
         */
        const std::string& GetDebugName() const override;

        /**
         * @brief 重写虚函数：获取缓冲区的详细调试信息
         * @return 详细调试信息字符串
         *
         * 教学要点: 提供缓冲区特定的调试信息，包括大小、用途、内存访问模式等
         * 对应Iris的调试信息输出模式
         */
        std::string GetDebugInfo() const override;

    protected:
        /**
         * @brief 分配Bindless索引（Buffer子类实现）
         * @param allocator Bindless索引分配器指针
         * @return 成功返回有效索引（1,000,000-1,999,999），失败返回INVALID_INDEX
         *
         * 教学要点:
         * 1. Buffer使用Buffer索引范围（1,000,000-1,999,999）
         * 2. 调用allocator->AllocateBufferIndex()获取Buffer专属索引
         * 3. FreeList O(1)分配策略
         */
        uint32_t AllocateBindlessIndexInternal(BindlessIndexAllocator* allocator) const override;

        /**
         * @brief 释放Bindless索引（Buffer子类实现）
         * @param allocator Bindless索引分配器指针
         * @param index 要释放的索引值
         * @return 成功返回true，失败返回false
         *
         * 教学要点:
         * 1. 调用allocator->FreeBufferIndex(index)释放Buffer专属索引
         * 2. 必须使用与AllocateBufferIndex对应的释放函数
         */
        bool FreeBindlessIndexInternal(BindlessIndexAllocator* allocator, uint32_t index) const override;

        void CreateDescriptorInGlobalHeap(ID3D12Device* device, GlobalDescriptorHeapManager* heapManager) override;

        // ==================== GPU资源上传(Milestone 2.7) ====================

        /**
         * Records the buffer copy commands for the selected upload path.
         */
        bool UploadToGPU(
            ID3D12GraphicsCommandList* commandList,
            class UploadContext&       uploadContext
        ) override;

        /**
         * Returns the steady-state buffer usage after upload completion.
         */
        D3D12_RESOURCE_STATES GetUploadDestinationState() const override;

        /**
         * Routes upload-heap buffers to the explicit no-copy path and keeps
         * default-heap initialization uploads eligible for copy-queue routing
         * when the current state is copy-safe.
         */
        UploadContract GetUploadContract() const override
        {
            if (m_memoryAccess == MemoryAccess::CPUToGPU ||
                m_memoryAccess == MemoryAccess::CPUWritable ||
                m_memoryAccess == MemoryAccess::GPUToCPU)
            {
                return UploadContract::NoGpuUpload;
            }

            return HasCPUData() && IsCopyQueueStateCompatible()
                       ? UploadContract::CopyReadyUpload
                       : UploadContract::GraphicsStatefulUpload;
        }

    private:
        // ==================== D12Buffer特有成员变量 ====================
        // 注意：m_resource, m_size, m_debugName, m_currentState 等已在基类D12Resource中定义

        BufferUsage         m_usage; // Buffer usage flags
        BufferUsagePolicy   m_usagePolicy = BufferUsagePolicy::DynamicUpload; // Declared upload policy
        MemoryAccess        m_memoryAccess; // Resolved memory access mode
        void*               m_mappedData; // Temporary mapped CPU pointer
        mutable std::string m_formattedDebugName; // Cached formatted debug name
        size_t              m_byteStride; // Structured-buffer element size in bytes

        void* m_persistentMappedData = nullptr; // Persistent CPU pointer
        bool  m_isPersistentlyMapped = false; // Persistent mapping state

        /**
         * 内部方法：创建DirectX 12资源
         * @param createInfo 创建参数
         * @return 是否创建成功
         */
        bool CreateD3D12Resource(const BufferCreateInfo& createInfo);

        // 注意：SetDebugName() 已在基类D12Resource中定义

        /**
         * 内部方法：获取堆属性
         * @param access 内存访问模式
         * @return 对应的D3D12_HEAP_PROPERTIES
         */
        static D3D12_HEAP_PROPERTIES GetHeapProperties(MemoryAccess access);

        /**
         * 内部方法：获取资源标志
         * @param usage 缓冲区用途
         * @return 对应的D3D12_RESOURCE_FLAGS
         */
        static D3D12_RESOURCE_FLAGS GetResourceFlags(BufferUsage usage);
    };

    // 位运算操作符重载（支持BufferUsage组合）
    inline BufferUsage operator|(BufferUsage a, BufferUsage b)
    {
        return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline BufferUsage operator&(BufferUsage a, BufferUsage b)
    {
        return static_cast<BufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline bool HasFlag(BufferUsage value, BufferUsage flag)
    {
        return (value & flag) == flag;
    }
} // namespace enigma::graphic
