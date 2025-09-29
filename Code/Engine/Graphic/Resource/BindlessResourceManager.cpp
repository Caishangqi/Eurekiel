#include "BindlessResourceManager.hpp"

#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Texture/D12Texture.hpp"
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
}
