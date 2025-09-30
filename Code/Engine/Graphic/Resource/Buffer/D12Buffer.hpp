#pragma once

#include "../D12Resources.hpp"
#include "../BindlessResourceTypes.hpp"
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
     * Memory access mode enumeration
     * Determines the access mode of the buffer between GPU/CPU
     */
    enum class MemoryAccess
    {
        GPUOnly, // GPU-specific, highest performance (DEFAULT heap)
        CPUToGPU, // CPU write, GPU read (UPLOAD heap)
        GPUToCPU, // GPU write, CPU read (READBACK heap)
        CPUWritable // The CPU can write frequently (corresponding to Iris' dynamic buffer)
    };

    /**
     * Buffer creation information structure
     * Corresponding to Iris' BuiltShaderStorageInfo, but adapts to DirectX 12
     */
    struct BufferCreateInfo
    {
        size_t       size; // Buffer size (bytes)
        BufferUsage  usage; // Use flag
        MemoryAccess memoryAccess; // Memory access mode
        const void*  initialData; // Initial data pointer (can be nullptr)
        const char*  debugName; // Debug name (corresponding to Iris' GLDebug.nameObject)

        // 默认构造
        BufferCreateInfo() : size(0), usage(BufferUsage::Default), memoryAccess(MemoryAccess::GPUOnly), initialData(nullptr), debugName(nullptr)
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
         * @brief 获取缓冲区的默认Bindless资源类型
         * @return 根据BufferUsage确定的BindlessResourceType
         *
         * 实现指导:
         * - ConstantBuffer用途返回BindlessResourceType::ConstantBuffer
         * - StructuredBuffer用途返回BindlessResourceType::Buffer
         * - 其他类型根据实际情况返回合适的BindlessResourceType
         */
        BindlessResourceType GetDefaultBindlessResourceType() const override;
        void                 CreateDescriptorInGlobalHeap(ID3D12Device* device, GlobalDescriptorHeapManager* heapManager) override;

    private:
        // ==================== D12Buffer特有成员变量 ====================
        // 注意：m_resource, m_size, m_debugName, m_currentState 等已在基类D12Resource中定义

        BufferUsage         m_usage; // 缓冲区使用类型
        MemoryAccess        m_memoryAccess; // 内存访问模式
        void*               m_mappedData; // 映射的CPU内存指针
        mutable std::string m_formattedDebugName; // 格式化的调试名称（用于GetDebugName重写）

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
