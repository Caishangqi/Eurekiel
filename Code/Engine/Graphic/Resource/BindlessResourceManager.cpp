#include "BindlessResourceManager.hpp"

#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Texture/D12Texture.hpp"
#include "Buffer/D12Buffer.hpp"
#include "DescriptorHandle.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // BindlessResourceManager 主要实现
    // ========================================================================

    BindlessResourceManager::BindlessResourceManager()
        : m_initialCapacity(DEFAULT_INITIAL_CAPACITY)
          , m_growthFactor(DEFAULT_GROWTH_FACTOR)
          , m_maxCapacity(DEFAULT_MAX_CAPACITY)
          , m_totalAllocated(0)
          , m_currentUsed(0)
          , m_peakUsed(0)
          , m_initialized(false)
    {
        enigma::core::LogInfo("BindlessResourceManager", "Constructed");
    }

    BindlessResourceManager::~BindlessResourceManager()
    {
        Shutdown();
        enigma::core::LogInfo("BindlessResourceManager", "Destroyed");
    }

    bool BindlessResourceManager::Initialize(uint32_t initialCapacity, uint32_t maxCapacity, uint32_t growthFactor)
    {
        if (m_initialized)
        {
            core::LogWarn("BindlessResourceManager", "Already initialized");
            return true;
        }

        // 存储配置参数
        m_initialCapacity = initialCapacity;
        m_maxCapacity     = maxCapacity;
        m_growthFactor    = growthFactor;

        // 创建并初始化DescriptorHeapManager
        m_heapManager = std::make_shared<DescriptorHeapManager>();

        // 设置描述符堆大小：
        // - CBV/SRV/UAV: 使用initialCapacity作为主要资源容量
        // - RTV: 1000个渲染目标描述符（包括SwapChain）
        // - DSV: 100个深度模板描述符
        // - Sampler: 2048个采样器描述符（预留）
        if (!m_heapManager->Initialize(initialCapacity, 1000, 100, 2048))
        {
            core::LogError("BindlessResourceManager", "Failed to initialize DescriptorHeapManager");
            m_heapManager.reset();
            return false;
        }

        // 预分配资源数组
        m_registeredResources.resize(initialCapacity);

        // 初始化空闲索引队列
        for (uint32_t i = 0; i < initialCapacity; ++i)
        {
            m_freeIndices.push(i);
        }

        // 初始化统计数据
        m_totalAllocated = 0;
        m_currentUsed    = 0;
        m_peakUsed       = 0;

        m_initialized = true;

        core::LogInfo("BindlessResourceManager",
                      "Initialized successfully with capacity: %u, max: %u, growth: %u",
                      initialCapacity, maxCapacity, growthFactor);

        return true;
    }

    void BindlessResourceManager::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        // 清理所有已注册的资源
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // 清空资源数组
            m_registeredResources.clear();

            // 清空映射表
            m_resourceToIndex.clear();
            m_indexToType.clear();

            // 清空空闲索引队列
            std::queue<uint32_t> empty;
            m_freeIndices.swap(empty);

            // 重置统计数据
            m_totalAllocated = 0;
            m_currentUsed    = 0;
            m_peakUsed       = 0;
        }

        // 释放描述符堆管理器
        if (m_heapManager)
        {
            m_heapManager.reset();
        }

        m_initialized = false;

        core::LogInfo("BindlessResourceManager", "Shutdown completed");
    }

    // ========================================================================
    // Bindless资源注册实现 (基于Iris直接组合模式)
    // ========================================================================

    /**
     * @brief 注册纹理到Bindless系统 - 核心实现
     *
     * 教学要点:
     * 1. 基于Iris源码分析，采用直接组合模式，无中间抽象层
     * 2. 使用ResourceBindingTraits组合特性提供Bindless支持
     * 3. DirectX 12 SRV (Shader Resource View) 创建和绑定
     * 4. 线程安全的资源注册和索引分配
     *
     * API来源:
     * - ID3D12Device::CreateShaderResourceView: DirectX 12核心API
     * - DescriptorHeapManager::AllocateCbvSrvUav: 自定义描述符分配
     * - ResourceBindingTraits::SetBindlessBinding: 组合特性设置
     *
     * Iris架构对应:
     * - 类似Iris Program.java中ProgramSamplers的直接组合模式
     * - 无中间抽象层，直接管理资源绑定
     */
    std::optional<uint32_t> BindlessResourceManager::RegisterTexture2D(std::shared_ptr<D12Texture> texture, BindlessResourceType type)
    {
        // 教学注释: 参数验证 - 防御性编程的重要实践
        if (!texture)
        {
            core::LogError("BindlessResourceManager", "RegisterTexture2D: texture is null");
            return std::nullopt;
        }

        if (!m_initialized || !m_heapManager)
        {
            core::LogError("BindlessResourceManager", "RegisterTexture2D: not initialized");
            return std::nullopt;
        }

        // 教学注释: 线程安全保护 - 多线程环境下的资源注册
        std::lock_guard<std::mutex> lock(m_mutex);

        // 教学注释: 检查是否已经注册，避免重复注册
        // 这是基于ResourceBindingTraits组合特性的状态检查
        if (texture->IsBindlessRegistered())
        {
            core::LogWarn("BindlessResourceManager", "RegisterTexture2D: texture already registered");
            auto existingIndex = texture->GetBindlessIndex();
            return existingIndex ? existingIndex : std::nullopt;
        }

        // 教学注释: 从描述符堆分配CBV/SRV/UAV描述符
        // 这是DirectX 12 Bindless渲染的核心 - 获取GPU可见的描述符
        auto allocation = m_heapManager->AllocateCbvSrvUav();
        if (!allocation.isValid)
        {
            core::LogError("BindlessResourceManager", "RegisterTexture2D: failed to allocate descriptor");
            return std::nullopt;
        }

        try
        {
            // 教学注释: 创建SRV (Shader Resource View)
            // API来源: DirectX 12官方文档和最佳实践
            auto device = D3D12RenderSystem::GetDevice();
            if (!device)
            {
                // 释放已分配的描述符
                m_heapManager->FreeCbvSrvUav(allocation);
                core::LogError("BindlessResourceManager", "RegisterTexture2D: D3D12 device not available");
                return std::nullopt;
            }

            // 教学注释: 配置SRV描述符
            // 基于BindlessResourceType确定纹理格式和维度
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format                          = texture->GetFormat(); // 获取纹理格式
            srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D; // 2D纹理
            srvDesc.Texture2D.MipLevels             = texture->GetMipLevels(); // Mip层级
            srvDesc.Texture2D.MostDetailedMip       = 0; // 最详细Mip级别
            srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 默认组件映射

            // 教学注释: 创建SRV到分配的描述符位置
            // 这是Bindless渲染的关键步骤 - 将资源绑定到描述符堆中的特定位置
            device->CreateShaderResourceView(
                texture->GetResource(), // 纹理资源
                &srvDesc, // SRV描述符配置
                allocation.cpuHandle // CPU描述符句柄
            );

            // 教学注释: 创建DescriptorHandle进行RAII管理
            // 使用现有架构的智能句柄管理，确保资源正确释放
            DescriptorHandle descriptorHandle(allocation, m_heapManager);

            // 教学注释: 设置资源的Bindless绑定信息
            // 使用ResourceBindingTraits组合特性，遵循组合优于继承原则
            texture->SetBindlessBinding(std::move(descriptorHandle), allocation.heapIndex);

            // 教学注释: 更新内部状态和统计
            uint32_t bindlessIndex = allocation.heapIndex;

            // 存储资源到已注册列表
            if (bindlessIndex >= m_registeredResources.size())
            {
                m_registeredResources.resize(bindlessIndex + 1);
            }
            m_registeredResources[bindlessIndex] = texture;

            // 更新映射表
            m_resourceToIndex[texture->GetResource()] = bindlessIndex;
            m_indexToType[bindlessIndex]              = type;

            // 更新统计信息
            m_currentUsed++;
            m_totalAllocated++;
#undef max
            m_peakUsed = std::max(m_peakUsed, m_currentUsed);

            core::LogInfo("BindlessResourceManager",
                          "RegisterTexture2D: successfully registered texture at index %u, current used: %u",
                          bindlessIndex, m_currentUsed);

            return bindlessIndex;
        }
        catch (const std::exception& e)
        {
            // 教学注释: 异常安全 - 清理已分配的资源
            m_heapManager->FreeCbvSrvUav(allocation);
            core::LogError("BindlessResourceManager", "RegisterTexture2D: exception occurred: %s", e.what());
            return std::nullopt;
        }
        catch (...)
        {
            // 教学注释: 处理未知异常
            m_heapManager->FreeCbvSrvUav(allocation);
            core::LogError("BindlessResourceManager", "RegisterTexture2D: unknown exception occurred");
            return std::nullopt;
        }
    }

    /**
     * @brief 注册缓冲区到Bindless系统 - 核心实现
     *
     * 教学要点:
     * 1. 与RegisterTexture2D类似,采用直接组合模式
     * 2. 使用ResourceBindingTraits组合特性提供Bindless支持
     * 3. DirectX 12 CBV/SRV/UAV创建(根据缓冲区类型)
     * 4. 线程安全的资源注册和索引分配
     *
     * API来源:
     * - ID3D12Device::CreateConstantBufferView: DirectX 12常量缓冲区视图
     * - ID3D12Device::CreateShaderResourceView: DirectX 12 SRV
     * - ID3D12Device::CreateUnorderedAccessView: DirectX 12 UAV
     *
     * Iris架构对应:
     * - 类似Iris的直接组合模式,无中间抽象层
     */
    std::optional<uint32_t> BindlessResourceManager::RegisterBuffer(std::shared_ptr<D12Buffer> buffer, BindlessResourceType type)
    {
        // 教学注释: 参数验证
        if (!buffer)
        {
            core::LogError("BindlessResourceManager", "RegisterBuffer: buffer is null");
            return std::nullopt;
        }

        if (!m_initialized || !m_heapManager)
        {
            core::LogError("BindlessResourceManager", "RegisterBuffer: not initialized");
            return std::nullopt;
        }

        // 教学注释: 线程安全保护
        std::lock_guard<std::mutex> lock(m_mutex);

        // 教学注释: 检查是否已经注册
        if (buffer->IsBindlessRegistered())
        {
            core::LogWarn("BindlessResourceManager", "RegisterBuffer: buffer already registered");
            auto existingIndex = buffer->GetBindlessIndex();
            return existingIndex ? existingIndex : std::nullopt;
        }

        // 教学注释: 从描述符堆分配CBV/SRV/UAV描述符
        auto allocation = m_heapManager->AllocateCbvSrvUav();
        if (!allocation.isValid)
        {
            core::LogError("BindlessResourceManager", "RegisterBuffer: failed to allocate descriptor");
            return std::nullopt;
        }

        try
        {
            // 教学注释: 获取D3D12设备
            auto device = D3D12RenderSystem::GetDevice();
            if (!device)
            {
                m_heapManager->FreeCbvSrvUav(allocation);
                core::LogError("BindlessResourceManager", "RegisterBuffer: D3D12 device not available");
                return std::nullopt;
            }

            // 教学注释: 根据缓冲区类型创建不同的视图
            // 支持Constant Buffer View (CBV), Shader Resource View (SRV), Unordered Access View (UAV)
            auto bufferResource = buffer->GetResource();

            if (type == BindlessResourceType::ConstantBuffer)
            {
                // 教学注释: 创建CBV
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                cbvDesc.BufferLocation                  = bufferResource->GetGPUVirtualAddress();
                cbvDesc.SizeInBytes                     = static_cast<UINT>(buffer->GetSize());

                device->CreateConstantBufferView(&cbvDesc, allocation.cpuHandle);
            }
            else if (type == BindlessResourceType::StructuredBuffer || type == BindlessResourceType::RawBuffer)
            {
                // 教学注释: 创建SRV用于结构化缓冲区或原始缓冲区
                // 使用合理的默认值：StructuredBuffer假设16字节元素(float4)，RawBuffer假设4字节元素(DWORD)
                UINT elementStride = (type == BindlessResourceType::StructuredBuffer) ? 16 : 4;
                UINT elementCount  = static_cast<UINT>(buffer->GetSize()) / elementStride;

                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format                          = DXGI_FORMAT_UNKNOWN;
                srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.Buffer.FirstElement             = 0;
                srvDesc.Buffer.NumElements              = elementCount;
                srvDesc.Buffer.StructureByteStride      = elementStride;
                srvDesc.Buffer.Flags                    = (type == BindlessResourceType::RawBuffer)
                                           ? D3D12_BUFFER_SRV_FLAG_RAW
                                           : D3D12_BUFFER_SRV_FLAG_NONE;

                device->CreateShaderResourceView(bufferResource, &srvDesc, allocation.cpuHandle);
            }
            else if (type == BindlessResourceType::RWStructuredBuffer || type == BindlessResourceType::RWRawBuffer)
            {
                // 教学注释: 创建UAV用于可读写缓冲区
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                uavDesc.Format                           = DXGI_FORMAT_UNKNOWN;
                uavDesc.ViewDimension                    = D3D12_UAV_DIMENSION_BUFFER;
                uavDesc.Buffer.FirstElement              = 0;
                UINT elementStride = (type == BindlessResourceType::RWStructuredBuffer) ? 16 : 4;
                UINT elementCount = static_cast<UINT>(buffer->GetSize()) / elementStride;

                uavDesc.Buffer.NumElements = elementCount;
                uavDesc.Buffer.StructureByteStride       = elementStride;
                uavDesc.Buffer.CounterOffsetInBytes      = 0;
                uavDesc.Buffer.Flags                     = (type == BindlessResourceType::RWRawBuffer)
                                           ? D3D12_BUFFER_UAV_FLAG_RAW
                                           : D3D12_BUFFER_UAV_FLAG_NONE;

                device->CreateUnorderedAccessView(bufferResource, nullptr, &uavDesc, allocation.cpuHandle);
            }
            else
            {
                // 教学注释: 不支持的缓冲区类型
                m_heapManager->FreeCbvSrvUav(allocation);
                core::LogError("BindlessResourceManager", "RegisterBuffer: unsupported buffer type");
                return std::nullopt;
            }

            // 教学注释: 创建DescriptorHandle进行RAII管理
            DescriptorHandle descriptorHandle(allocation, m_heapManager);

            // 教学注释: 设置资源的Bindless绑定信息
            buffer->SetBindlessBinding(std::move(descriptorHandle), allocation.heapIndex);

            // 教学注释: 更新内部状态和统计
            uint32_t bindlessIndex = allocation.heapIndex;

            // 存储资源到已注册列表
            if (bindlessIndex >= m_registeredResources.size())
            {
                m_registeredResources.resize(bindlessIndex + 1);
            }
            m_registeredResources[bindlessIndex] = buffer;

            // 更新映射表
            m_resourceToIndex[buffer->GetResource()] = bindlessIndex;
            m_indexToType[bindlessIndex]             = type;

            // 更新统计信息
            m_currentUsed++;
            m_totalAllocated++;
#undef max
            m_peakUsed = std::max(m_peakUsed, m_currentUsed);

            core::LogInfo("BindlessResourceManager",
                          "RegisterBuffer: successfully registered buffer at index %u, current used: %u",
                          bindlessIndex, m_currentUsed);

            return bindlessIndex;
        }
        catch (const std::exception& e)
        {
            // 教学注释: 异常安全 - 清理已分配的资源
            m_heapManager->FreeCbvSrvUav(allocation);
            core::LogError("BindlessResourceManager", "RegisterBuffer: exception occurred: %s", e.what());
            return std::nullopt;
        }
        catch (...)
        {
            // 教学注释: 处理未知异常
            m_heapManager->FreeCbvSrvUav(allocation);
            core::LogError("BindlessResourceManager", "RegisterBuffer: unknown exception occurred");
            return std::nullopt;
        }
    }

    /**
     * @brief 从Bindless系统注销资源
     *
     * 教学要点:
     * 1. 通过ResourceBindingTraits清除Bindless绑定
     * 2. 自动释放描述符通过DescriptorHandle的RAII机制
     * 3. 线程安全的资源注销
     *
     * RAII优势:
     * - DescriptorHandle析构时自动调用SafeRelease()
     * - 无需手动管理描述符释放
     */
    bool BindlessResourceManager::UnregisterResource(std::shared_ptr<D12Resource> resource)
    {
        if (!resource)
        {
            core::LogError("BindlessResourceManager", "UnregisterResource: resource is null");
            return false;
        }

        if (!m_initialized)
        {
            core::LogError("BindlessResourceManager", "UnregisterResource: not initialized");
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        // 教学注释: 检查资源是否已注册
        if (!resource->IsBindlessRegistered())
        {
            core::LogWarn("BindlessResourceManager", "UnregisterResource: resource not registered");
            return false;
        }

        auto bindlessIndex = resource->GetBindlessIndex();
        if (!bindlessIndex.has_value())
        {
            core::LogError("BindlessResourceManager", "UnregisterResource: invalid bindless index");
            return false;
        }

        uint32_t index = bindlessIndex.value();

        // 教学注释: 清除资源的Bindless绑定
        // ClearBindlessBinding() 会自动触发DescriptorHandle析构,释放描述符
        resource->ClearBindlessBinding();

        // 更新映射表
        m_resourceToIndex.erase(resource->GetResource());
        m_indexToType.erase(index);

        // 清空已注册列表中的条目
        if (index < m_registeredResources.size())
        {
            m_registeredResources[index].reset();
        }

        // 更新统计信息
        m_currentUsed--;
        m_freeIndices.push(index);

        core::LogInfo("BindlessResourceManager",
                      "UnregisterResource: successfully unregistered resource at index %u, current used: %u",
                      index, m_currentUsed);

        return true;
    }

    /**
     * @brief 获取资源的GPU描述符句柄
     */
    std::optional<D3D12_GPU_DESCRIPTOR_HANDLE> BindlessResourceManager::GetGPUHandle(std::shared_ptr<D12Resource> resource) const
    {
        if (!resource || !resource->IsBindlessRegistered())
        {
            return std::nullopt;
        }

        // 教学注释: 通过ResourceBindingTraits获取GPU句柄
        return resource->GetBindlessGpuHandle();
    }

    /**
     * @brief 通过索引获取GPU描述符句柄
     */
    std::optional<D3D12_GPU_DESCRIPTOR_HANDLE> BindlessResourceManager::GetGPUHandleByIndex(uint32_t index) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (index >= m_registeredResources.size() || !m_registeredResources[index])
        {
            return std::nullopt;
        }

        auto resource = m_registeredResources[index];
        return resource->GetBindlessGpuHandle();
    }

    /**
     * @brief 设置描述符表到命令列表
     *
     * 教学要点:
     * 1. 将Bindless描述符堆绑定到命令列表
     * 2. 设置根参数索引(RootParameterIndex)
     * 3. DirectX 12 Descriptor Table绑定机制
     */
    void BindlessResourceManager::SetDescriptorTable(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndex) const
    {
        if (!commandList || !m_heapManager)
        {
            core::LogError("BindlessResourceManager", "SetDescriptorTable: invalid parameters");
            return;
        }

        // 教学注释: 获取主描述符堆(CBV/SRV/UAV)
        auto mainHeap = m_heapManager->GetMainHeap();
        if (!mainHeap)
        {
            core::LogError("BindlessResourceManager", "SetDescriptorTable: main heap not available");
            return;
        }

        // 教学注释: 设置描述符堆到命令列表
        ID3D12DescriptorHeap* heaps[] = {mainHeap};
        commandList->SetDescriptorHeaps(1, heaps);

        // 教学注释: 绑定Descriptor Table到根参数
        auto gpuHandle = m_heapManager->GetMainHeapGPUStart();
        commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, gpuHandle);
    }

    /**
     * @brief 刷新所有描述符
     *
     * 教学注释: 保留接口用于未来扩展
     * 当前架构下描述符创建时即已完成初始化
     * 未来可用于动态资源更新场景
     */
    void BindlessResourceManager::RefreshAllDescriptors()
    {
        // 教学注释: 当前实现为空
        // 所有描述符在注册时已正确创建
        // 未来可添加重新创建描述符的逻辑
        core::LogInfo("BindlessResourceManager", "RefreshAllDescriptors: no action required (descriptors are static)");
    }

    /**
     * @brief 获取主描述符堆(CBV/SRV/UAV)
     */
    ID3D12DescriptorHeap* BindlessResourceManager::GetMainDescriptorHeap() const
    {
        return m_heapManager ? m_heapManager->GetMainHeap() : nullptr;
    }

    /**
     * @brief 获取采样器描述符堆
     */
    ID3D12DescriptorHeap* BindlessResourceManager::GetSamplerDescriptorHeap() const
    {
        return m_heapManager ? m_heapManager->GetSamplerHeap() : nullptr;
    }

    /**
     * @brief 设置描述符堆到命令列表
     *
     * 教学要点:
     * 1. 同时绑定主堆(CBV/SRV/UAV)和采样器堆
     * 2. DirectX 12 SetDescriptorHeaps API使用
     */
    void BindlessResourceManager::SetDescriptorHeaps(ID3D12GraphicsCommandList* commandList) const
    {
        if (!commandList || !m_heapManager)
        {
            core::LogError("BindlessResourceManager", "SetDescriptorHeaps: invalid parameters");
            return;
        }

        auto mainHeap    = m_heapManager->GetMainHeap();
        auto samplerHeap = m_heapManager->GetSamplerHeap();

        if (!mainHeap || !samplerHeap)
        {
            core::LogError("BindlessResourceManager", "SetDescriptorHeaps: heaps not available");
            return;
        }

        // 教学注释: 同时设置主堆和采样器堆
        ID3D12DescriptorHeap* heaps[] = {mainHeap, samplerHeap};
        commandList->SetDescriptorHeaps(2, heaps);
    }

    /**
     * @brief 获取指定类型的资源数量
     */
    uint32_t BindlessResourceManager::GetResourceCountByType(BindlessResourceType type) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        uint32_t count = 0;
        for (const auto& [index, resourceType] : m_indexToType)
        {
            if (resourceType == type)
            {
                count++;
            }
        }
        return count;
    }

    /**
     * @brief 获取描述符堆使用情况
     */
    DescriptorHeapUsage BindlessResourceManager::GetDescriptorHeapUsage() const
    {
        DescriptorHeapUsage usage;

        if (m_heapManager)
        {
            usage.cbvSrvUavUsed     = m_heapManager->GetCbvSrvUavCount();
            usage.cbvSrvUavCapacity = m_heapManager->GetCbvSrvUavCapacity();
            usage.rtvUsed           = m_heapManager->GetRtvCount();
            usage.rtvCapacity       = m_heapManager->GetRtvCapacity();
            usage.dsvUsed           = m_heapManager->GetDsvCount();
            usage.dsvCapacity       = m_heapManager->GetDsvCapacity();
            usage.samplerUsed       = m_heapManager->GetSamplerCount();
            usage.samplerCapacity   = m_heapManager->GetSamplerCapacity();
        }

        return usage;
    }

    /**
     * @brief 获取调试信息
     *
     * 教学要点:
     * 1. 使用std::ostringstream构建格式化字符串
     * 2. 提供完整的状态和统计信息
     */
    std::string BindlessResourceManager::GetDebugInfo() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::ostringstream oss;
        oss << "BindlessResourceManager[\n";
        oss << "  Initialized: " << (m_initialized ? "true" : "false") << "\n";
        oss << "  Initial Capacity: " << m_initialCapacity << "\n";
        oss << "  Max Capacity: " << m_maxCapacity << "\n";
        oss << "  Growth Factor: " << m_growthFactor << "\n";
        oss << "  Total Allocated: " << m_totalAllocated << "\n";
        oss << "  Current Used: " << m_currentUsed << "\n";
        oss << "  Peak Used: " << m_peakUsed << "\n";
        oss << "  Free Indices Available: " << m_freeIndices.size() << "\n";
        oss << "  Registered Resources: " << m_registeredResources.size() << "\n";

        // 按类型统计
        oss << "  Resource Types:\n";
        oss << "    Texture2D: " << GetResourceCountByType(BindlessResourceType::Texture2D) << "\n";
        oss << "    Texture3D: " << GetResourceCountByType(BindlessResourceType::Texture3D) << "\n";
        oss << "    TextureCube: " << GetResourceCountByType(BindlessResourceType::TextureCube) << "\n";
        oss << "    ConstantBuffer: " << GetResourceCountByType(BindlessResourceType::ConstantBuffer) << "\n";
        oss << "    StructuredBuffer: " << GetResourceCountByType(BindlessResourceType::StructuredBuffer) << "\n";
        oss << "    RawBuffer: " << GetResourceCountByType(BindlessResourceType::RawBuffer) << "\n";

        // 描述符堆使用情况
        if (m_heapManager)
        {
            auto usage = GetDescriptorHeapUsage();
            oss << "  Descriptor Heap Usage:\n";
            oss << "    CBV/SRV/UAV: " << usage.cbvSrvUavUsed << "/" << usage.cbvSrvUavCapacity << "\n";
            oss << "    RTV: " << usage.rtvUsed << "/" << usage.rtvCapacity << "\n";
            oss << "    DSV: " << usage.dsvUsed << "/" << usage.dsvCapacity << "\n";
            oss << "    Sampler: " << usage.samplerUsed << "/" << usage.samplerCapacity << "\n";
        }

        oss << "]";
        return oss.str();
    }
}
