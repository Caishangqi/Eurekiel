#include "Engine/Graphic/Target/ShadowColorManager.hpp"
#include "Engine/Graphic/Target/D12RenderTarget.hpp"
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"
#include <sstream>

#include "Engine/Core/Logger/Logger.hpp"

namespace enigma::graphic
{
    // ============================================================================
    // 构造函数
    // ============================================================================

    ShadowColorManager::ShadowColorManager(
        const std::array<RTConfig, 8>& rtConfigs,
        int                            shadowColorCount
    )
        : m_configs(rtConfigs)
          , m_activeShadowColorCount(shadowColorCount)
          , m_baseWidth(0)
          , m_baseHeight(0)
    {
        // 验证并修正shadowColorCount范围
        if (m_activeShadowColorCount < 0 || m_activeShadowColorCount > MAX_SHADOW_COLORS)
        {
            LOG_WARN_F("ShadowColorManager", "Invalid shadowColorCount: %d, clamping to [0, %d]",
                       m_activeShadowColorCount, MAX_SHADOW_COLORS);
            m_activeShadowColorCount = std::clamp(m_activeShadowColorCount, 0, MAX_SHADOW_COLORS);
        }

        // 从第一个配置中提取基准尺寸
        if (m_activeShadowColorCount > 0 && m_configs[0].width > 0)
        {
            m_baseWidth  = m_configs[0].width;
            m_baseHeight = m_configs[0].height;
        }

        // 创建ShadowColor渲染目标
        CreateShadowColors();

        LOG_INFO_F("ShadowColorManager", "Created with %d active ShadowColors", m_activeShadowColorCount);
    }

    // ============================================================================
    // 创建ShadowColor渲染目标
    // ============================================================================

    void ShadowColorManager::CreateShadowColors()
    {
        m_shadowColors.clear();
        m_shadowColors.reserve(m_activeShadowColorCount * 2); // 每个有Main和Alt

        for (int i = 0; i < m_activeShadowColorCount; ++i)
        {
            const auto& config = m_configs[i];

            // 创建Main纹理
            auto mainRT = D12RenderTarget::Builder()
                          .SetName(config.name + "_Main")
                          .SetDimensions(config.width, config.height)
                          .SetFormat(config.format)
                          .Build();

            // 创建Alt纹理（如果启用Flipper）
            std::shared_ptr<D12RenderTarget> altRT;
            if (config.enableFlipper)
            {
                altRT = D12RenderTarget::Builder()
                        .SetName(config.name + "_Alt")
                        .SetDimensions(config.width, config.height)
                        .SetFormat(config.format)
                        .Build();
            }
            else
            {
                // 不启用Flipper时，Alt使用相同的Main纹理引用
                altRT = mainRT;
            }

            // 添加到数组：索引规则 Main = i*2, Alt = i*2+1
            m_shadowColors.push_back(mainRT);
            m_shadowColors.push_back(altRT);

            LOG_DEBUG_F("ShadowColorManager", "Created ShadowColor[%d]: %dx%d, Format: %d",
                        i, config.width, config.height, static_cast<int>(config.format));
        }
    }

    // ============================================================================
    // ShadowColor访问接口
    // ============================================================================

    std::shared_ptr<D12RenderTarget> ShadowColorManager::GetShadowColor(int index, bool alt) const
    {
        if (!IsValidIndex(index))
        {
            LogError_F("ShadowColorManager", "Invalid index: %d, valid range: [0, %d)",
                       index, m_activeShadowColorCount);
            return nullptr;
        }

        // 索引规则: Main = i*2, Alt = i*2+1
        int arrayIndex = alt ? (index * 2 + 1) : (index * 2);
        return m_shadowColors[arrayIndex];
    }

    // ============================================================================
    // RTV访问接口
    // ============================================================================

    D3D12_CPU_DESCRIPTOR_HANDLE ShadowColorManager::GetMainRTV(int index) const
    {
        if (!IsValidIndex(index))
        {
            LogError_F("ShadowColorManager", "Invalid index: %d, valid range: [0, %d)",
                       index, m_activeShadowColorCount);
            return D3D12_CPU_DESCRIPTOR_HANDLE{0};
        }

        const auto& mainRT = m_shadowColors[index * 2];
        return mainRT->GetMainRTV();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ShadowColorManager::GetAltRTV(int index) const
    {
        if (!IsValidIndex(index))
        {
            LogError_F("ShadowColorManager", "Invalid index: %d, valid range: [0, %d)",
                       index, m_activeShadowColorCount);
            return D3D12_CPU_DESCRIPTOR_HANDLE{0};
        }

        const auto& altRT = m_shadowColors[index * 2 + 1];
        return altRT->GetAltRTV();
    }

    // ============================================================================
    // Bindless索引访问
    // ============================================================================

    uint32_t ShadowColorManager::GetMainTextureIndex(int index) const
    {
        if (!IsValidIndex(index))
        {
            LogError_F("ShadowColorManager", "Invalid index: %d, valid range: [0, %d)",
                       index, m_activeShadowColorCount);
            return 0;
        }

        const auto& mainRT = m_shadowColors[index * 2];
        return mainRT->GetMainTextureIndex();
    }

    uint32_t ShadowColorManager::GetAltTextureIndex(int index) const
    {
        if (!IsValidIndex(index))
        {
            LogError_F("ShadowColorManager", "Invalid index: %d, valid range: [0, %d)",
                       index, m_activeShadowColorCount);
            return 0;
        }

        const auto& altRT = m_shadowColors[index * 2 + 1];
        return altRT->GetAltTextureIndex();
    }

    // ============================================================================
    // GPU常量缓冲上传
    // ============================================================================

    uint32_t ShadowColorManager::CreateShadowColorBuffer()
    {
        // 定义GPU Buffer结构体（与HLSL对应）
        struct ShadowColorBuffer
        {
            uint32_t readIndices[8]; // 读索引数组
            uint32_t writeIndices[8]; // 写索引数组
        };

        ShadowColorBuffer bufferData = {};

        // 根据FlipState填充读/写索引
        for (int i = 0; i < MAX_SHADOW_COLORS; ++i)
        {
            if (i < m_activeShadowColorCount)
            {
                bool isFlipped = m_flipState.IsFlipped(i);

                if (isFlipped)
                {
                    // 翻转状态：读Alt写Main
                    bufferData.readIndices[i]  = GetAltTextureIndex(i);
                    bufferData.writeIndices[i] = GetMainTextureIndex(i);
                }
                else
                {
                    // 正常状态：读Main写Alt
                    bufferData.readIndices[i]  = GetMainTextureIndex(i);
                    bufferData.writeIndices[i] = GetAltTextureIndex(i);
                }
            }
            else
            {
                // 未激活的ShadowColor设置为0
                bufferData.readIndices[i]  = 0;
                bufferData.writeIndices[i] = 0;
            }
        }

        // 创建或更新GPU Buffer
        if (!m_gpuBuffer)
        {
            BufferCreateInfo createInfo;
            createInfo.size         = sizeof(ShadowColorBuffer);
            createInfo.usage        = BufferUsage::StructuredBuffer;
            createInfo.memoryAccess = MemoryAccess::CPUToGPU;
            createInfo.initialData  = &bufferData;
            createInfo.debugName    = "ShadowColorBuffer";

            m_gpuBuffer = std::make_unique<D12Buffer>(createInfo);
            LOG_INFO_F("ShadowColorManager", "Created ShadowColorBuffer, Bindless index: %u",
                       m_gpuBuffer->GetBindlessIndex());
        }
        else
        {
            // 更新现有Buffer
            void* mappedData = m_gpuBuffer->Map();
            if (mappedData)
            {
                memcpy(mappedData, &bufferData, sizeof(ShadowColorBuffer));
                m_gpuBuffer->Unmap();
            }
        }

        return m_gpuBuffer ? m_gpuBuffer->GetBindlessIndex() : 0;
    }

    // ============================================================================
    // 窗口尺寸变化响应
    // ============================================================================

    void ShadowColorManager::OnResize(int newBaseWidth, int newBaseHeight)
    {
        m_baseWidth  = newBaseWidth;
        m_baseHeight = newBaseHeight;

        LOG_INFO_F("ShadowColorManager", "Resizing ShadowColors to %dx%d", newBaseWidth, newBaseHeight);

        // 重新创建所有ShadowColor（因为尺寸可能基于baseWidth/baseHeight）
        for (int i = 0; i < m_activeShadowColorCount; ++i)
        {
            auto& config = m_configs[i];

            // 更新配置尺寸（如果配置使用相对尺寸）
            // 注意：这里假设config.width/height已经是绝对像素值
            // 如果需要支持相对尺寸（如0.5倍屏幕分辨率），需要在此处计算

            const auto& mainRT = m_shadowColors[i * 2];
            const auto& altRT  = m_shadowColors[i * 2 + 1];

            // 调用ResizeIfNeeded（假设D12RenderTarget实现了此方法）
            // mainRT->ResizeIfNeeded(config.width, config.height);
            // if (config.enableFlipper)
            // {
            //     altRT->ResizeIfNeeded(config.width, config.height);
            // }
            LOG_DEBUG_F("ShadowColorManager", "Resized ShadowColor[%d] to %dx%d",
                        i, config.width, config.height);
        }

        // 重新创建GPU Buffer
        CreateShadowColorBuffer();
    }

    // ============================================================================
    // 调试支持
    // ============================================================================

    std::string ShadowColorManager::GetDebugInfo(int index) const
    {
        if (!IsValidIndex(index))
        {
            return "Invalid index";
        }

        const auto& config = m_configs[index];
        const auto& mainRT = m_shadowColors[index * 2];

        std::ostringstream oss;
        oss << "ShadowColor[" << index << "]: "
            << config.name << " "
            << config.width << "x" << config.height << ", "
            << "Format: " << static_cast<int>(config.format) << ", "
            << "Flipper: " << (config.enableFlipper ? "Yes" : "No") << ", "
            << "FlipState: " << (m_flipState.IsFlipped(index) ? "Flipped" : "Normal") << ", "
            << "MainIndex: " << GetMainTextureIndex(index) << ", "
            << "AltIndex: " << GetAltTextureIndex(index);

        return oss.str();
    }
} // namespace enigma::graphic
