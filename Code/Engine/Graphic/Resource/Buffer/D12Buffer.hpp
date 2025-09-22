#pragma once

#include "../D12Resources.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <cstdint>

namespace enigma::graphic
{
    /**
     * 教学目标：了解DirectX 12缓冲区管理的现代方法
     *
     * D12Buffer类设计说明：
     * - 对应Iris的ShaderStorageBuffer.java的DirectX 12实现
     * - 管理GPU缓冲区资源，包括顶点缓冲区、索引缓冲区、常量缓冲区等
     * - 使用RAII管理资源生命周期
     * - 支持CPU可见和GPU专用的不同用途缓冲区
     *
     * DirectX 12 API参考：
     * - ID3D12Resource: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12resource
     * - D3D12_RESOURCE_DESC: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_desc
     * - D3D12_HEAP_PROPERTIES: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_heap_properties
     */

    /**
     * 缓冲区使用标志枚举
     * 对应DirectX 12的D3D12_RESOURCE_USAGE和D3D12_RESOURCE_FLAGS
     */
    enum class BufferUsage : uint32_t
    {
        VertexBuffer = 0x1, // 顶点缓冲区
        IndexBuffer = 0x2, // 索引缓冲区
        ConstantBuffer = 0x4, // 常量缓冲区 (对应Iris的Uniform Buffer)
        StructuredBuffer = 0x8, // 结构化缓冲区 (对应Iris的SSBO)
        StorageBuffer = 0x10, // 可读写存储缓冲区
        IndirectBuffer = 0x20, // 间接绘制参数缓冲区

        // 组合标志
        Default = VertexBuffer | IndexBuffer,
        AllBufferTypes = VertexBuffer | IndexBuffer | ConstantBuffer | StructuredBuffer | StorageBuffer | IndirectBuffer
    };

    /**
     * 内存访问模式枚举
     * 决定缓冲区在GPU/CPU间的访问模式
     */
    enum class MemoryAccess
    {
        GPUOnly, // GPU专用，最高性能 (DEFAULT heap)
        CPUToGPU, // CPU写入，GPU读取 (UPLOAD heap)
        GPUToCPU, // GPU写入，CPU读取 (READBACK heap)
        CPUWritable // CPU可频繁写入 (对应Iris的dynamic buffer)
    };

    /**
     * 缓冲区创建信息结构
     * 对应Iris的BuiltShaderStorageInfo，但适配DirectX 12
     */
    struct BufferCreateInfo
    {
        size_t       size; // 缓冲区大小（字节）
        BufferUsage  usage; // 使用标志
        MemoryAccess memoryAccess; // 内存访问模式
        const void*  initialData; // 初始数据指针（可为nullptr）
        const char*  debugName; // 调试名称（对应Iris的GLDebug.nameObject）

        // 默认构造
        BufferCreateInfo()
            : size(0)
              , usage(BufferUsage::Default)
              , memoryAccess(MemoryAccess::GPUOnly)
              , initialData(nullptr)
              , debugName(nullptr)
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
        ~D12Buffer();

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
         * 检查缓冲区是否有效（继承自D12Resource）
         * @return true表示资源已创建且有效
         */
        // IsValid() 已在基类D12Resource中定义

        /**
         * 获取调试名称（继承自D12Resource）
         * @return 调试名称字符串
         */
        // GetDebugName() 已在基类D12Resource中定义

    private:
        // ==================== D12Buffer特有成员变量 ====================
        // 注意：m_resource, m_size, m_debugName, m_currentState 等已在基类D12Resource中定义

        BufferUsage  m_usage; // 缓冲区使用类型
        MemoryAccess m_memoryAccess; // 内存访问模式
        void*        m_mappedData; // 映射的CPU内存指针

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
