#include "BindlessResourceManager.hpp"

#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

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
        m_maxCapacity = maxCapacity;
        m_growthFactor = growthFactor;

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
        m_currentUsed = 0;
        m_peakUsed = 0;

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
            m_currentUsed = 0;
            m_peakUsed = 0;
        }

        // 释放描述符堆管理器
        if (m_heapManager)
        {
            m_heapManager.reset();
        }

        m_initialized = false;

        core::LogInfo("BindlessResourceManager", "Shutdown completed");
    }
}
