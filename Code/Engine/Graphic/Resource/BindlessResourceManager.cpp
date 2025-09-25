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

    void BindlessResourceManager::Shutdown()
    {
        ERROR_AND_DIE("BindlessResourceManager::Shutdown not implemented")
    }


    // TODO: 其他方法的完整实现由您来完成
}
