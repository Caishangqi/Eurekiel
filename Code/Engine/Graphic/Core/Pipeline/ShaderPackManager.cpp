#include "ShaderPackManager.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    ShaderPackManager::ShaderPackManager()
        : m_compileFlags(0)
          , m_debugMode(false)
          , m_optimizeShaders(true)
          , m_hotReloadEnabled(false)
          , m_enableCache(true)
          , m_totalShaders(0)
          , m_compiledShaders(0)
          , m_failedShaders(0)
          , m_initialized(false)
    {
        enigma::core::LogInfo("ShaderPackManager", "Constructed");
    }

    ShaderPackManager::~ShaderPackManager()
    {
        Shutdown();
        enigma::core::LogInfo("ShaderPackManager", "Destroyed");
    }

    void ShaderPackManager::Shutdown()
    {
        // 桩实现
    }

    // TODO: 其他方法的完整实现由您来完成
}
