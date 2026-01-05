#include "Engine/Graphic/Target/ShadowTextureManager.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"
#include <algorithm>

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/Logger.hpp"
using namespace enigma::core;

namespace enigma::graphic
{
    // ============================================================================
    // 构造函数
    // ============================================================================

    ShadowTextureManager::ShadowTextureManager(
        const std::array<RTConfig, 2>& rtConfigs,
        int                            shadowTexCount
    )
        : m_configs(rtConfigs)
          , m_activeShadowTexCount(shadowTexCount)
          , m_baseWidth(0)
          , m_baseHeight(0)
    {
        // 验证并修��shadowTexCount范围
        if (m_activeShadowTexCount < 0 || m_activeShadowTexCount > MAX_SHADOW_TEXTURES)
        {
            LogWarn(LogRenderer, "Invalid shadowTexCount: {}, clamping to [0, {}]",
                    m_activeShadowTexCount, MAX_SHADOW_TEXTURES);
            m_activeShadowTexCount = std::clamp(m_activeShadowTexCount, 0, MAX_SHADOW_TEXTURES);
        }

        // 从第一个配置中提取基准尺寸
        if (m_activeShadowTexCount > 0 && m_configs[0].width > 0)
        {
            m_baseWidth  = m_configs[0].width;
            m_baseHeight = m_configs[0].height;
        }

        // 创建ShadowTex纹理
        CreateShadowTargets();

        core::LogInfo(core::LogRenderer, "Created with {} active ShadowTextures", m_activeShadowTexCount);
    }

    // ============================================================================
    // 创建ShadowTex纹理
    // ============================================================================

    void ShadowTextureManager::CreateShadowTargets()
    {
        m_shadowTargets.clear();
        m_shadowTargets.reserve(m_activeShadowTexCount); // 只有单个纹理，无Flipper

        for (int i = 0; i < m_activeShadowTexCount; ++i)
        {
            const auto& config = m_configs[i];

            // 构造TextureCreateInfo
            TextureCreateInfo createInfo;
            createInfo.type        = TextureType::Texture2D;
            createInfo.width       = config.width;
            createInfo.height      = config.height;
            createInfo.depth       = 1;
            createInfo.mipLevels   = 1;
            createInfo.arraySize   = 1;
            createInfo.format      = config.format;
            createInfo.usage       = TextureUsage::ShaderResource; // 只读纹理
            createInfo.initialData = nullptr;
            createInfo.dataSize    = 0;
            createInfo.rowPitch    = 0;
            createInfo.slicePitch  = 0;
            createInfo.debugName   = config.name.c_str();

            // 创建D12Texture（使用构造函数，不是Builder模式）
            auto texture = std::make_shared<D12Texture>(createInfo);

            m_shadowTargets.push_back(texture);

            LogDebug(LogRenderer, "Created ShadowTex[{}]: {}x{}, Format: {}",
                     i, config.width, config.height, static_cast<int>(config.format));
        }
    }

    // ============================================================================
    // ShadowTex访问接口
    // ============================================================================

    std::shared_ptr<D12Texture> ShadowTextureManager::GetShadowTarget(int index) const
    {
        if (index < 0 || index >= m_activeShadowTexCount)
        {
            LogError(LogRenderer, "Invalid index: {}, valid range: [0, {})",
                     index, m_activeShadowTexCount);
            return nullptr;
        }

        return m_shadowTargets[index];
    }

    uint32_t ShadowTextureManager::GetShadowTexIndex(int index) const
    {
        if (index < 0 || index >= m_activeShadowTexCount)
        {
            LogError(LogRenderer, "Invalid index: {}, valid range: [0, {})",
                     index, m_activeShadowTexCount);
            return 0;
        }

        const auto& texture = m_shadowTargets[index];
        return texture->GetBindlessIndex();
    }

    // ============================================================================
    // 窗口尺寸变化响应
    // ============================================================================

    void ShadowTextureManager::OnResize(int baseWidth, int baseHeight)
    {
        if (baseWidth <= 0 || baseHeight <= 0)
        {
            LogError(LogRenderer, "Invalid resize dimensions: {}x{}", baseWidth, baseHeight);
            return;
        }

        // 更新基准尺寸
        m_baseWidth  = baseWidth;
        m_baseHeight = baseHeight;

        // 更新每个配置的实际尺寸（应用缩放因子）
        for (int i = 0; i < m_activeShadowTexCount; ++i)
        {
            auto& config  = m_configs[i];
            config.width  = static_cast<int>(baseWidth * config.widthScale);
            config.height = static_cast<int>(baseHeight * config.heightScale);
        }

        // 重建所有纹理
        CreateShadowTargets();

        // 索引更新由 UniformManager 负责

        LogInfo(LogRenderer, "Resized to {}x{}, rebuilt {} ShadowTextures",
                baseWidth, baseHeight, m_activeShadowTexCount);
    }
} // namespace enigma::graphic
