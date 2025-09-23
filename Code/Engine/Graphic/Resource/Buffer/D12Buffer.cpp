#include "D12Buffer.hpp"
#include <cassert>
#include <algorithm>

#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"

namespace enigma::graphic
{
    // ===== D12Buffer类实现 =====

    /**
     * 构造函数实现
     * 参考Iris的ShaderStorageBuffer构造函数逻辑
     */
    D12Buffer::D12Buffer(const BufferCreateInfo& createInfo)
        : D12Resource() // 调用基类构造函数
          , m_usage(createInfo.usage)
          , m_memoryAccess(createInfo.memoryAccess)
          , m_mappedData(nullptr)
    {
        // 教学注释：验证输入参数的有效性
        assert(createInfo.size > 0 && "Buffer size must be greater than 0");

        // 创建DirectX 12资源
        if (!CreateD3D12Resource(createInfo))
        {
            // 创建失败，保持无效状态
            return;
        }

        // 设置调试名称（使用基类方法）
        if (createInfo.debugName)
        {
            SetDebugName(createInfo.debugName);
        }

        // 如果提供了初始数据且缓冲区支持CPU写入，则进行数据拷贝
        // 对应Iris的ShaderStorageBuffer中content的处理
        if (createInfo.initialData && (m_memoryAccess == MemoryAccess::CPUToGPU || m_memoryAccess == MemoryAccess::CPUWritable))
        {
            void* mappedPtr = Map();
            if (mappedPtr)
            {
                memcpy(mappedPtr, createInfo.initialData, GetSize());
                Unmap();
            }
        }
    }

    /**
     * 析构函数实现
     * 对应Iris的ShaderStorageBuffer.destroy()方法
     */
    D12Buffer::~D12Buffer()
    {
        // 如果当前有映射，先取消映射
        if (m_mappedData)
        {
            Unmap();
        }

        // ComPtr会自动释放DirectX 12资源
        // 这里不需要显式调用Release()
    }

    /**
     * 移动构造函数
     */
    D12Buffer::D12Buffer(D12Buffer&& other) noexcept
        : D12Resource(std::move(other)) // 调用基类移动构造函数
          , m_usage(other.m_usage)
          , m_memoryAccess(other.m_memoryAccess)
          , m_mappedData(other.m_mappedData)
    {
        // 清空源对象
        other.m_mappedData = nullptr;
    }

    /**
     * 移动赋值操作符
     */
    D12Buffer& D12Buffer::operator=(D12Buffer&& other) noexcept
    {
        if (this != &other)
        {
            // 释放当前资源
            if (m_mappedData)
            {
                Unmap();
            }

            // 调用基类移动赋值
            D12Resource::operator=(std::move(other));

            // 移动D12Buffer特有成员
            m_usage        = other.m_usage;
            m_memoryAccess = other.m_memoryAccess;
            m_mappedData   = other.m_mappedData;

            // 清空源对象
            other.m_mappedData = nullptr;
        }
        return *this;
    }

    /**
     * 映射CPU内存
     * 对应OpenGL的glMapBuffer功能
     */
    void* D12Buffer::Map(const D3D12_RANGE* readRange)
    {
        auto* resource = GetResource(); // 使用基类方法获取资源
        if (!resource || m_mappedData)
        {
            return nullptr;
        }

        // 只有CPU可访问的缓冲区才能映射
        if (m_memoryAccess == MemoryAccess::GPUOnly)
        {
            assert(false && "Cannot map GPU-only buffer");
            return nullptr;
        }

        // DirectX 12 API: ID3D12Resource::Map()
        // 映射GPU资源到CPU可访问的内存地址
        // readRange: 指定CPU将读取的内存范围（可为nullptr表示不读取）
        HRESULT hr = resource->Map(0, readRange, &m_mappedData);

        if (FAILED(hr))
        {
            m_mappedData = nullptr;
            assert(false && "Failed to map buffer memory");
        }

        return m_mappedData;
    }

    /**
     * 取消映射CPU内存
     * 对应OpenGL的glUnmapBuffer功能
     */
    void D12Buffer::Unmap(const D3D12_RANGE* writtenRange)
    {
        auto* resource = GetResource(); // 使用基类方法获取资源
        if (!resource || !m_mappedData)
        {
            return;
        }

        // DirectX 12 API: ID3D12Resource::Unmap()
        // 取消GPU资源的CPU内存映射
        // writtenRange: 指定CPU修改的内存范围（可为nullptr表示整个范围）
        resource->Unmap(0, writtenRange);
        m_mappedData = nullptr;
    }

    /**
     * 创建DirectX 12资源的核心实现
     * 对应Iris的IrisRenderSystem.createBuffers()逻辑
     */
    bool D12Buffer::CreateD3D12Resource(const BufferCreateInfo& createInfo)
    {
        // 1. 设置堆属性（决定内存类型和CPU/GPU访问性）
        // DirectX 12 API: D3D12_HEAP_PROPERTIES
        D3D12_HEAP_PROPERTIES heapProps = GetHeapProperties(m_memoryAccess);

        // 2. 创建资源描述符
        // DirectX 12 API: D3D12_RESOURCE_DESC
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER; // 缓冲区类型
        resourceDesc.Alignment           = 0; // 让DirectX自动选择对齐
        resourceDesc.Width               = createInfo.size; // 缓冲区大小
        resourceDesc.Height              = 1; // 缓冲区高度固定为1
        resourceDesc.DepthOrArraySize    = 1; // 深度固定为1
        resourceDesc.MipLevels           = 1; // Mip级别固定为1
        resourceDesc.Format              = DXGI_FORMAT_UNKNOWN; // 缓冲区格式未知
        resourceDesc.SampleDesc.Count    = 1; // 采样数量
        resourceDesc.SampleDesc.Quality  = 0; // 采样质量
        resourceDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // 行主序布局
        resourceDesc.Flags               = GetResourceFlags(m_usage); // 根据用途设置标志

        // 3. 确定初始资源状态
        D3D12_RESOURCE_STATES initialState;
        switch (m_memoryAccess)
        {
        case MemoryAccess::GPUOnly:
            initialState = D3D12_RESOURCE_STATE_COMMON; // GPU专用，通用状态
            break;
        case MemoryAccess::CPUToGPU:
        case MemoryAccess::CPUWritable:
            initialState = D3D12_RESOURCE_STATE_GENERIC_READ; // CPU可写，GPU读取状态
            break;
        case MemoryAccess::GPUToCPU:
            initialState = D3D12_RESOURCE_STATE_COPY_DEST; // GPU写入，拷贝目标状态
            break;
        default:
            initialState = D3D12_RESOURCE_STATE_COMMON;
            break;
        }

        // 4. 创建提交资源
        // 使用D3D12RenderSystem统一接口创建资源
        // 符合分层架构原则：资源层通过API封装层访问DirectX
        ID3D12Resource* resource = nullptr;
        HRESULT         hr       = D3D12RenderSystem::CreateCommittedResource(
            heapProps, // 堆属性
            resourceDesc, // 资源描述
            initialState, // 初始状态
            &resource // 输出接口
        );

        if (FAILED(hr))
        {
            assert(false && "Failed to create D3D12 buffer resource");
            return false;
        }

        // 使用基类方法设置资源
        SetResource(resource, initialState, createInfo.size);

        return true;
    }

    /**
     * 获取对应内存访问模式的堆属性
     */
    D3D12_HEAP_PROPERTIES D12Buffer::GetHeapProperties(MemoryAccess access)
    {
        D3D12_HEAP_PROPERTIES heapProps = {};

        switch (access)
        {
        case MemoryAccess::GPUOnly:
            // DEFAULT堆：GPU专用，最高性能
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
            heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            break;

        case MemoryAccess::CPUToGPU:
        case MemoryAccess::CPUWritable:
            // UPLOAD堆：CPU可写，GPU可读
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
            heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            break;

        case MemoryAccess::GPUToCPU:
            // READBACK堆：GPU可写，CPU可读
            heapProps.Type = D3D12_HEAP_TYPE_READBACK;
            heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            break;

        default:
            // 默认使用DEFAULT堆
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
            heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            break;
        }

        heapProps.CreationNodeMask = 1; // 单GPU系统
        heapProps.VisibleNodeMask  = 1; // 单GPU系统

        return heapProps;
    }

    /**
     * 根据用途获取资源标志
     */
    D3D12_RESOURCE_FLAGS D12Buffer::GetResourceFlags(BufferUsage usage)
    {
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

        // 如果包含可写存储缓冲区用途，添加UAV标志
        if (HasFlag(usage, BufferUsage::StorageBuffer))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        return flags;
    }
} // namespace enigma::graphic
