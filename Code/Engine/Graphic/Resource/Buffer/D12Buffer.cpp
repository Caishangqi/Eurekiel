#include "D12Buffer.hpp"
#include <cassert>
#include <algorithm>

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Resource/GlobalDescriptorHeapManager.hpp"
#include "Engine/Graphic/Resource/UploadContext.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Renderer/API/DX12Renderer.hpp"

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
          , m_formattedDebugName() // 初始化格式化调试名称
          , m_byteStride(createInfo.byteStride)
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

        // 复制初始数据到CPU端（如果提供了）
        if (createInfo.initialData && createInfo.size > 0)
        {
            SetInitialData(createInfo.initialData, createInfo.size);
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
        // [NEW] 如果有持久映射，先取消持久映射
        UnmapPersistent();

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
          , m_formattedDebugName(std::move(other.m_formattedDebugName))
          , m_byteStride(other.m_byteStride)
          , m_persistentMappedData(other.m_persistentMappedData) // [NEW] 转移持久映射指针
          , m_isPersistentlyMapped(other.m_isPersistentlyMapped) // [NEW] 转移持久映射状态
    {
        // 清空源对象
        other.m_mappedData           = nullptr;
        other.m_persistentMappedData = nullptr; // [NEW] 清空源对象的持久映射指针
        other.m_isPersistentlyMapped = false; // [NEW] 清空源对象的持久映射状态
    }

    /**
     * 移动赋值操作符
     */
    D12Buffer& D12Buffer::operator=(D12Buffer&& other) noexcept
    {
        if (this != &other)
        {
            // [NEW] 释放当前的持久映射
            UnmapPersistent();

            // 释放当前资源
            if (m_mappedData)
            {
                Unmap();
            }

            // 调用基类移动赋值
            D12Resource::operator=(std::move(other));

            // 移动D12Buffer特有成员
            m_usage                = other.m_usage;
            m_memoryAccess         = other.m_memoryAccess;
            m_mappedData           = other.m_mappedData;
            m_formattedDebugName   = std::move(other.m_formattedDebugName);
            m_byteStride           = other.m_byteStride;
            m_persistentMappedData = other.m_persistentMappedData; // [NEW] 转移持久映射指针
            m_isPersistentlyMapped = other.m_isPersistentlyMapped; // [NEW] 转移持久映射状态

            // 清空源对象
            other.m_mappedData           = nullptr;
            other.m_persistentMappedData = nullptr; // [NEW] 清空源对象的持久映射指针
            other.m_isPersistentlyMapped = false; // [NEW] 清空源对象的持久映射状态
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
     * [NEW] 持久映射CPU内存（DirectX 12推荐模式）
     * 教学要点:
     * 1. 只有UPLOAD堆的缓冲区才能持久映射
     * 2. 使用CD3DX12_RANGE(0,0)表示CPU不读取，只写入
     * 3. 整个生命周期保持映射状态，避免反复Map/Unmap开销
     * 4. 重复调用返回同一指针，避免重复映射
     */
    void* D12Buffer::MapPersistent()
    {
        // 如果已经持久映射，直接返回缓存的指针
        if (m_isPersistentlyMapped)
        {
            return m_persistentMappedData;
        }

        auto* resource = GetResource();
        if (!resource)
        {
            core::LogError("D12Buffer", "MapPersistent: Resource is null");
            return nullptr;
        }

        // 只有CPU可访问的缓冲区才能持久映射
        if (m_memoryAccess != MemoryAccess::CPUToGPU && m_memoryAccess != MemoryAccess::CPUWritable)
        {
            core::LogError("D12Buffer",
                           "MapPersistent: Only UPLOAD heap buffers (CPUToGPU/CPUWritable) can be persistently mapped");
            return nullptr;
        }

        // 使用CD3DX12_RANGE(0,0)表示CPU不读取
        // 这是DirectX 12的最佳实践，避免缓存同步开销
        CD3DX12_RANGE readRange(0, 0);
        HRESULT       hr = resource->Map(0, &readRange, &m_persistentMappedData);

        if (FAILED(hr))
        {
            core::LogError("D12Buffer", "MapPersistent: Failed to persistent map buffer");
            m_persistentMappedData = nullptr;
            return nullptr;
        }

        m_isPersistentlyMapped = true;

        core::LogDebug("D12Buffer",
                       "MapPersistent: Successfully mapped buffer '%s' at address 0x%p",
                       GetDebugName().empty() ? "<unnamed>" : GetDebugName().c_str(),
                       m_persistentMappedData);

        return m_persistentMappedData;
    }

    /**
     * [NEW] 取消持久映射
     * 教学要点:
     * 1. 在析构函数中调用，确保资源正确释放
     * 2. 检查状态标志，避免重复Unmap
     * 3. 清空指针和状态标志，防止悬空指针
     */
    void D12Buffer::UnmapPersistent()
    {
        // 如果未持久映射，无需操作
        if (!m_isPersistentlyMapped)
        {
            return;
        }

        auto* resource = GetResource();
        if (resource)
        {
            // 取消映射（writtenRange=nullptr表示整个范围都被写入）
            resource->Unmap(0, nullptr);

            core::LogDebug("D12Buffer",
                           "UnmapPersistent: Unmapped buffer '%s'",
                           GetDebugName().empty() ? "<unnamed>" : GetDebugName().c_str());
        }

        // 清空状态
        m_persistentMappedData = nullptr;
        m_isPersistentlyMapped = false;
    }

    /**
     * @brief 重写虚函数：设置调试名称并添加缓冲区特定逻辑
     *
     * 教学要点: 重写基类虚函数，在设置名称时同时更新格式化名称
     * 这样GetDebugName()可以返回包含缓冲区详细信息的名称
     */
    void D12Buffer::SetDebugName(const std::string& name)
    {
        // 调用基类实现设置基本调试名称
        D12Resource::SetDebugName(name);

        // 清空格式化名称，让GetDebugName()重新生成
        m_formattedDebugName.clear();
    }

    /**
     * @brief 重写虚函数：获取包含缓冲区大小信息的调试名称
     *
     * 教学要点: 重写基类虚函数，返回格式化的缓冲区调试信息
     * 格式: "BufferName (Size: XXX bytes, Usage: YYY)"
     */
    const std::string& D12Buffer::GetDebugName() const
    {
        // 如果格式化名称为空，重新生成
        if (m_formattedDebugName.empty())
        {
            const std::string& baseName = D12Resource::GetDebugName();
            if (baseName.empty())
            {
                m_formattedDebugName = "[Unnamed Buffer]";
            }
            else
            {
                m_formattedDebugName = baseName;
            }

            // 添加缓冲区大小信息
            m_formattedDebugName += " (Size: " + std::to_string(GetSize()) + " bytes";

            // 添加用途信息
            if (HasFlag(m_usage, BufferUsage::VertexBuffer))
                m_formattedDebugName += ", Vertex";
            if (HasFlag(m_usage, BufferUsage::IndexBuffer))
                m_formattedDebugName += ", Index";
            if (HasFlag(m_usage, BufferUsage::ConstantBuffer))
                m_formattedDebugName += ", Constant";
            if (HasFlag(m_usage, BufferUsage::StructuredBuffer))
                m_formattedDebugName += ", Structured";
            if (HasFlag(m_usage, BufferUsage::StorageBuffer))
                m_formattedDebugName += ", Storage";
            if (HasFlag(m_usage, BufferUsage::IndirectBuffer))
                m_formattedDebugName += ", Indirect";

            m_formattedDebugName += ")";
        }

        return m_formattedDebugName;
    }

    /**
     * @brief 重写虚函数：获取缓冲区的详细调试信息
     *
     * 教学要点: 提供缓冲区特定的详细调试信息
     * 包括大小、用途、内存访问模式、映射状态等信息
     */
    std::string D12Buffer::GetDebugInfo() const
    {
        std::string info = "D12Buffer Debug Info:\n";
        info += "  Name: " + GetDebugName() + "\n";
        info += "  Size: " + std::to_string(GetSize()) + " bytes\n";
        info += "  GPU Address: 0x" + std::to_string(GetGPUVirtualAddress()) + "\n";

        // 内存访问模式
        info += "  Memory Access: ";
        switch (m_memoryAccess)
        {
        case MemoryAccess::GPUOnly:
            info += "GPU Only (DEFAULT heap)\n";
            break;
        case MemoryAccess::CPUToGPU:
            info += "CPU to GPU (UPLOAD heap)\n";
            break;
        case MemoryAccess::GPUToCPU:
            info += "GPU to CPU (READBACK heap)\n";
            break;
        case MemoryAccess::CPUWritable:
            info += "CPU Writable (UPLOAD heap)\n";
            break;
        }

        // 映射状态
        info += "  Mapped: " + std::string(m_mappedData ? "Yes" : "No") + "\n";

        // 资源状态
        info += "  Current State: " + std::to_string(static_cast<int>(GetCurrentState())) + "\n";
        info += "  Valid: " + std::string(IsValid() ? "Yes" : "No");

        return info;
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
        // [NEW] Buffer resources don't need ClearValue (DirectX 12 specification)
        HRESULT hr = D3D12RenderSystem::CreateCommittedResource(
            heapProps, // 堆属性
            resourceDesc, // 资源描述
            initialState, // 初始状态
            nullptr, // [NEW] Buffer resources always pass nullptr for pOptimizedClearValue
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

    /**
     * @brief 分配Buffer专属的Bindless索引
     *
     * 教学要点:
     * 1. Buffer索引范围: 1,000,000 - 1,999,999 (1M个Buffer容量)
     * 2. 调用BindlessIndexAllocator::AllocateBufferIndex()
     * 3. 返回值是全局唯一的索引,用于在全局描述符堆中定位
     */
    uint32_t D12Buffer::AllocateBindlessIndexInternal(BindlessIndexAllocator* allocator) const
    {
        if (!allocator)
        {
            return BindlessIndexAllocator::INVALID_INDEX;
        }

        // 调用Buffer索引分配器
        return allocator->AllocateBufferIndex();
    }

    /**
     * @brief 释放Buffer专属的Bindless索引
     *
     * 教学要点:
     * 1. 必须使用FreeBufferIndex()释放Buffer索引
     * 2. 不能使用FreeTextureIndex()释放Buffer索引(会导致索引泄漏)
     * 3. FreeList架构保证O(1)时间复杂度
     */
    bool D12Buffer::FreeBindlessIndexInternal(BindlessIndexAllocator* allocator, uint32_t index) const
    {
        if (!allocator)
        {
            return false;
        }

        // 调用Buffer索引释放器
        return allocator->FreeBufferIndex(index);
    }

    /**
     * @brief 在全局描述符堆中创建缓冲区描述符 (SM6.6 Bindless架构)
     *
     * 教学要点:
     * 1. SM6.6 Bindless架构要求在全局描述符堆的指定索引位置创建CBV或SRV
     * 2. ConstantBuffer使用CBV (Constant Buffer View)
     * 3. StructuredBuffer使用SRV (Structured Buffer View)
     * 4. 其他类型的Buffer根据用途选择合适的视图类型
     */
    void D12Buffer::CreateDescriptorInGlobalHeap(
        ID3D12Device*                device,
        GlobalDescriptorHeapManager* heapManager)
    {
        // 验证参数和状态
        if (!device || !heapManager || !IsValid() || !IsBindlessRegistered())
        {
            core::LogError("D12Buffer",
                           "CreateDescriptorInGlobalHeap: Invalid parameters or resource not registered");
            return;
        }

        // 获取缓冲区资源
        auto* resource = GetResource();
        if (!resource)
        {
            core::LogError("D12Buffer", "CreateDescriptorInGlobalHeap: Resource is null");
            return;
        }

        // 根据缓冲区用途选择合适的视图类型
        if (HasFlag(m_usage, BufferUsage::ConstantBuffer))
        {
            // 1. 创建Constant Buffer View (CBV)
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation                  = resource->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes                     = static_cast<UINT>((GetSize() + 255) & ~255); // CBV必须256字节对齐

            // 在全局描述符堆的Bindless索引位置创建CBV
            heapManager->CreateConstantBufferView(device, &cbvDesc, GetBindlessIndex());

            core::LogInfo("D12Buffer",
                          "CreateDescriptorInGlobalHeap: Created CBV at bindless index %u for buffer '%s'",
                          GetBindlessIndex(),
                          GetDebugName().c_str());
        }
        else if (HasFlag(m_usage, BufferUsage::StructuredBuffer) ||
            HasFlag(m_usage, BufferUsage::StorageBuffer))
        {
            // 2. 创建Structured Buffer View (SRV)
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format                          = DXGI_FORMAT_UNKNOWN; // Structured Buffer格式未知
            srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement             = 0;
            size_t stride                           = m_byteStride > 0 ? m_byteStride : 4;
            if (stride <= 0)
                LogWarn("D12Buffer", "Buffer stride is not setting which is set to default stride %zu", stride);
            srvDesc.Buffer.NumElements         = static_cast<UINT>(GetSize() / stride);
            srvDesc.Buffer.StructureByteStride = static_cast<UINT>(stride);
            srvDesc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;

            // 在全局描述符堆的Bindless索引位置创建SRV
            heapManager->CreateShaderResourceView(device, resource, &srvDesc, GetBindlessIndex());

            core::LogInfo("D12Buffer",
                          "CreateDescriptorInGlobalHeap: Created Structured Buffer SRV at bindless index %u for buffer '%s'",
                          GetBindlessIndex(),
                          GetDebugName().c_str());
        }
        else if (HasFlag(m_usage, BufferUsage::VertexBuffer) ||
            HasFlag(m_usage, BufferUsage::IndexBuffer))
        {
            // 3. Vertex/Index Buffer通常使用Raw Buffer View (SRV)
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format                          = DXGI_FORMAT_R32_TYPELESS; // Raw Buffer使用R32_TYPELESS
            srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement             = 0;
            srvDesc.Buffer.NumElements              = static_cast<UINT>(GetSize() / 4); // 每个元素4字节
            srvDesc.Buffer.StructureByteStride      = 0; // Raw Buffer的步长为0
            srvDesc.Buffer.Flags                    = D3D12_BUFFER_SRV_FLAG_RAW; // Raw Buffer标志

            // 在全局描述符堆的Bindless索引位置创建SRV
            heapManager->CreateShaderResourceView(device, resource, &srvDesc, GetBindlessIndex());

            core::LogInfo("D12Buffer",
                          "CreateDescriptorInGlobalHeap: Created Raw Buffer SRV at bindless index %u for buffer '%s'",
                          GetBindlessIndex(),
                          GetDebugName().c_str());
        }
        else
        {
            // 默认情况：创建Raw Buffer View
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format                          = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement             = 0;
            srvDesc.Buffer.NumElements              = static_cast<UINT>(GetSize() / 4);
            srvDesc.Buffer.StructureByteStride      = 0;
            srvDesc.Buffer.Flags                    = D3D12_BUFFER_SRV_FLAG_RAW;

            heapManager->CreateShaderResourceView(device, resource, &srvDesc, GetBindlessIndex());

            core::LogInfo("D12Buffer",
                          "CreateDescriptorInGlobalHeap: Created default SRV at bindless index %u for buffer '%s'",
                          GetBindlessIndex(),
                          GetDebugName().c_str());
        }
    }

    // ==================== GPU资源上传(Milestone 2.7) ====================

    /**
     * @brief 实现缓冲区特定的上传逻辑
     * 教学要点:
     * 1. 缓冲区上传比纹理简单,使用CopyBufferRegion即可
     * 2. destOffset设为0,完整替换整个缓冲区数据
     * 3. 使用UploadContext::UploadBufferData()执行GPU复制
     * 4. 资源状态转换由基类D12Resource::Upload()处理
     */
    bool D12Buffer::UploadToGPU(
        ID3D12GraphicsCommandList* commandList,
        UploadContext&             uploadContext
    )
    {
        if (!HasCPUData())
        {
            core::LogError(LogRenderer,
                           "D12Buffer::UploadToGPU: No CPU data available");
            return false;
        }

        // 调用UploadContext上传缓冲区数据(destOffset=0,完整替换)
        bool uploadSuccess = uploadContext.UploadBufferData(
            commandList,
            GetResource(), // GPU缓冲区资源
            GetCPUData(), // CPU数据指针
            GetCPUDataSize(), // 数据大小
            0 // destOffset=0(完整替换)
        );

        if (!uploadSuccess)
        {
            core::LogError(LogRenderer,
                           "D12Buffer::UploadToGPU: Failed to upload buffer '%s'",
                           GetDebugName().empty() ? "<unnamed>" : GetDebugName().c_str());
            return false;
        }

        core::LogDebug(LogRenderer,
                       "D12Buffer::UploadToGPU: Successfully uploaded buffer '%s' (%zu bytes)",
                       GetDebugName().empty() ? "<unnamed>" : GetDebugName().c_str(),
                       GetCPUDataSize());

        return true;
    }

    /**
     * @brief 获取缓冲区上传后的目标资源状态
     * 教学要点:
     * 1. 缓冲区通常作为各种着色器的输入(VS/PS/CS)
     * 2. D3D12_RESOURCE_STATE_GENERIC_READ是最通用的缓冲区状态
     * 3. GENERIC_READ包含多种读取状态,适用于Vertex/Index/Constant/Shader Resource
     * 4. 如果需要UAV访问,应在创建时指定BufferUsage::UnorderedAccess
     */
    D3D12_RESOURCE_STATES D12Buffer::GetUploadDestinationState() const
    {
        // 缓冲区默认转换到通用读取状态
        return D3D12_RESOURCE_STATE_GENERIC_READ;
    }
} // namespace enigma::graphic
