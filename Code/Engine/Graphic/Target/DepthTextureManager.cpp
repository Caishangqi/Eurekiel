/**
 * @file DepthTextureManager.cpp
 * @brief DepthTextureManager实现 - Iris兼容的深度纹理管理
 */

#include "DepthTextureManager.hpp"
#include "Engine/Graphic/Target/D12DepthTexture.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"

#include <sstream>
#include <stdexcept>

using namespace enigma::graphic;

// ============================================================================
// DepthTextureManager 统一构造函数 - 与RenderTargetManager保持一致
// ============================================================================

DepthTextureManager::DepthTextureManager(
    int                                      baseWidth,
    int                                      baseHeight,
    const std::array<DepthTextureConfig, 3>& depthConfigs,
    int                                      depthCount
)
    : m_renderWidth(baseWidth)
      , m_renderHeight(baseHeight)
{
    // 参数验证 - 尺寸
    if (baseWidth <= 0 || baseHeight <= 0)
    {
        throw std::invalid_argument("Base width and height must be greater than zero");
    }

    // 参数验证 - depthCount范围 [1, 3]
    if (depthCount < 1 || depthCount > 3)
    {
        throw std::out_of_range("Depth count must be in range [1-3]");
    }

    // [FIX] 预分配vector空间，避免越界访问
    // 与ConfigureDepthTextures()保持一致的vector操作模式
    m_depthTextures.reserve(depthCount);
    m_depthConfigs.reserve(depthCount);

    // 创建激活的深度纹理
    for (int i = 0; i < depthCount; ++i)
    {
        const auto& config = depthConfigs[i];

        // 验证配置
        if (!config.IsValid())
        {
            throw std::invalid_argument(
                "Invalid depth texture config at index " + std::to_string(i)
            );
        }

        // 保存配置
        DepthTextureConfig savedConfig;
        savedConfig.width        = config.width;
        savedConfig.height       = config.height;
        savedConfig.format       = config.format;
        savedConfig.semanticName = config.semanticName;
        m_depthConfigs.push_back(savedConfig);

        // 创建深度纹理
        DepthTextureCreateInfo createInfo(
            config.semanticName.c_str(),
            static_cast<uint32_t>(config.width),
            static_cast<uint32_t>(config.height),
            config.format,
            1.0f, // clearDepth
            0 // clearStencil
        );

        // [FIX] 使用push_back代替下标访问，避免越界
        // 创建D12DepthTexture（DSV自动创建）
        m_depthTextures.push_back(std::make_shared<D12DepthTexture>(createInfo));
    }
}

// ============================================================================
// 深度纹理访问接口
// ============================================================================

std::shared_ptr<D12DepthTexture> DepthTextureManager::GetDepthTexture(int index) const
{
    if (!IsValidIndex(index))
    {
        throw std::out_of_range(
            "Depth texture index " + std::to_string(index) + " out of range [0-2]"
        );
    }
    return m_depthTextures[index];
}

uint32_t DepthTextureManager::GetDepthTextureIndex(int index) const
{
    if (!IsValidIndex(index))
    {
        throw std::out_of_range(
            "Depth texture index " + std::to_string(index) + " out of range [0-2]"
        );
    }

    // 获取深度纹理的Bindless索引
    auto depthTex = m_depthTextures[index];
    if (!depthTex)
    {
        throw std::runtime_error(
            "Depth texture " + std::to_string(index) + " is null"
        );
    }

    return depthTex->GetBindlessIndex();
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthTextureManager::GetDSV(int index) const
{
    if (!IsValidIndex(index))
    {
        throw std::out_of_range(
            "Depth texture index " + std::to_string(index) + " out of range [0-2]"
        );
    }

    auto depthTex = m_depthTextures[index];
    if (!depthTex)
    {
        return {}; // 返回空句柄
    }

    // 直接从D12DepthTexture获取DSV句柄
    return depthTex->GetDSVHandle();
}

// ============================================================================
// Iris兼容的深度复制接口
// ============================================================================

void DepthTextureManager::CopyPreTranslucentDepth(ID3D12GraphicsCommandList* cmdList)
{
    // 复制depthtex0 -> depthtex1 (对应Iris的CopyPreTranslucentDepth)
    CopyDepth(cmdList, 0, 1);
}

void DepthTextureManager::CopyPreHandDepth(ID3D12GraphicsCommandList* cmdList)
{
    // 复制depthtex0 -> depthtex2 (对应Iris的CopyPreHandDepth)
    CopyDepth(cmdList, 0, 2);
}

void DepthTextureManager::CopyDepth(
    ID3D12GraphicsCommandList* cmdList,
    int                        srcIndex,
    int                        destIndex
)
{
    // 参数验证
    if (!cmdList)
    {
        throw std::invalid_argument("Command list cannot be null");
    }

    if (!IsValidIndex(srcIndex) || !IsValidIndex(destIndex))
    {
        throw std::out_of_range("Source or destination index out of range [0-2]");
    }

    if (srcIndex == destIndex)
    {
        throw std::invalid_argument("Source and destination cannot be the same");
    }

    // 获取深度纹理
    auto srcTexture  = m_depthTextures[srcIndex];
    auto destTexture = m_depthTextures[destIndex];

    if (!srcTexture || !destTexture)
    {
        throw std::runtime_error("Source or destination texture is null");
    }

    // 获取D3D12Resource（使用GetDepthTextureResource()）
    auto srcResource  = srcTexture->GetDepthTextureResource();
    auto destResource = destTexture->GetDepthTextureResource();

    if (!srcResource || !destResource)
    {
        throw std::runtime_error("Failed to get D3D12 resources for depth copy");
    }

    // 1. 转换资源状态: DSV -> COPY_SOURCE/DEST
    D3D12_RESOURCE_BARRIER barriers[2];

    // 源纹理: DEPTH_WRITE -> COPY_SOURCE
    barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[0].Transition.pResource   = srcResource;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    // 目标纹理: DEPTH_WRITE -> COPY_DEST
    barriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[1].Transition.pResource   = destResource;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    D3D12RenderSystem::TransitionResources(cmdList, barriers, 2, "DepthTexture::CopyDepth::PreCopy");

    // 2. 复制深度纹理
    cmdList->CopyResource(destResource, srcResource);

    // 3. 恢复资源状态: COPY_SOURCE/DEST -> DSV
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    D3D12RenderSystem::TransitionResources(cmdList, barriers, 2, "DepthTexture::CopyDepth::PostCopy");
}

// ============================================================================
// Milestone 4: 深度缓冲管理 - 切换和复制实现
// ============================================================================

void DepthTextureManager::SwitchDepthBuffer(int newActiveIndex)
{
    // 参数验证
    if (!IsValidIndex(newActiveIndex))
    {
        throw std::out_of_range(
            "Depth buffer index " + std::to_string(newActiveIndex) + " out of range [0-2]"
        );
    }

    // 验证深度纹理存在
    if (!m_depthTextures[newActiveIndex])
    {
        throw std::runtime_error(
            "Cannot switch to null depth texture " + std::to_string(newActiveIndex)
        );
    }

    // 更新活动索引
    int oldIndex = m_currentActiveDepthIndex;
    UNUSED(oldIndex)
    m_currentActiveDepthIndex = newActiveIndex;

    // 日志记录
    const char* depthNames[3] = {"depthtex0", "depthtex1", "depthtex2"};
    // TODO: 添加日志系统后启用
    // LogInfo("DepthTextureManager: Switched depth buffer from {} to {}",
    //          depthNames[oldIndex], depthNames[newActiveIndex]);
}

void DepthTextureManager::CopyDepth(
    int srcIndex,
    int dstIndex
)
{
    // 获取当前活动的命令列表
    ID3D12GraphicsCommandList* cmdList = D3D12RenderSystem::GetCurrentCommandList();
    if (!cmdList)
    {
        throw std::runtime_error("CopyDepthBuffer: No active command list");
    }

    // 直接调用内部CopyDepth方法（已包含所有验证和状态转换逻辑）
    CopyDepth(cmdList, srcIndex, dstIndex);

    // 日志记录
    const char* depthNames[3] = {"depthtex0", "depthtex1", "depthtex2"};
    // TODO: 添加日志系统后启用
    // LogInfo("DepthTextureManager: Copied depth buffer {} -> {}",
    //          depthNames[srcIndex], depthNames[dstIndex]);
}

int DepthTextureManager::GetActiveDepthBufferIndex() const noexcept
{
    return m_currentActiveDepthIndex;
}

// ============================================================================
// 窗口尺寸变化响应
// ============================================================================

void DepthTextureManager::OnResize(int newWidth, int newHeight)
{
    // 参数验证
    if (newWidth <= 0 || newHeight <= 0)
    {
        throw std::invalid_argument("New width and height must be greater than zero");
    }

    m_renderWidth  = newWidth;
    m_renderHeight = newHeight;

    // M3.1: 遍历所有深度纹理
    for (size_t i = 0; i < m_depthTextures.size(); ++i)
    {
        if (m_depthTextures[i])
        {
            // depthtex0强制使用渲染分辨率
            int targetWidth  = newWidth;
            int targetHeight = newHeight;

            // depthtex1-N保持原有分辨率比例
            if (i > 0 && i < m_depthConfigs.size())
            {
                // 计算原有分辨率比例
                float widthRatio = static_cast<float>(m_depthConfigs[i].width) /
                    static_cast<float>(m_renderWidth);
                float heightRatio = static_cast<float>(m_depthConfigs[i].height) /
                    static_cast<float>(m_renderHeight);

                targetWidth  = static_cast<int>(newWidth * widthRatio);
                targetHeight = static_cast<int>(newHeight * heightRatio);

                // 更新配置
                m_depthConfigs[i].width  = targetWidth;
                m_depthConfigs[i].height = targetHeight;
            }
            else if (i == 0)
            {
                // 更新depthtex0配置
                m_depthConfigs[0].width  = newWidth;
                m_depthConfigs[0].height = newHeight;
            }

            bool success = m_depthTextures[i]->Resize(
                static_cast<uint32_t>(targetWidth),
                static_cast<uint32_t>(targetHeight)
            );

            if (!success)
            {
                throw std::runtime_error(
                    "Failed to resize depth texture " + std::to_string(i)
                );
            }
            // D12DepthTexture::Resize()自动处理DSV重建
        }
    }
}

// ============================================================================
// M3.1新增: 动态配置API实现
// ============================================================================

void DepthTextureManager::ConfigureDepthTextures(int count)
{
    // 参数验证
    if (count < 1 || count > 16)
    {
        throw std::out_of_range("Depth texture count must be in range [1-16], got " + std::to_string(count));
    }

    int currentCount = static_cast<int>(m_depthTextures.size());

    // 场景1: 需要增加深度纹理
    if (count > currentCount)
    {
        int addCount = count - currentCount;
        m_depthTextures.reserve(count);
        m_depthConfigs.reserve(count);

        for (int i = 0; i < addCount; ++i)
        {
            int         newIndex = currentCount + i;
            std::string name     = "depthtex" + std::to_string(newIndex);

            // 新增深度纹理使用渲染分辨率
            DepthTextureConfig config3;
            config3.width        = m_renderWidth;
            config3.height       = m_renderHeight;
            config3.format       = DepthFormat::D32_FLOAT;
            config3.semanticName = name;
            m_depthConfigs.push_back(config3);

            // 创建深度纹理
            DepthTextureCreateInfo createInfo(
                name.c_str(),
                static_cast<uint32_t>(m_renderWidth),
                static_cast<uint32_t>(m_renderHeight),
                DepthFormat::D32_FLOAT,
                1.0f,
                0
            );

            m_depthTextures.push_back(std::make_shared<D12DepthTexture>(createInfo));
        }
    }
    // 场景2: 需要减少深度纹理
    else if (count < currentCount)
    {
        // depthtex0不可删除
        if (count < 1)
        {
            throw std::invalid_argument("Cannot delete depthtex0 (minimum count is 1)");
        }

        // 删除多余的深度纹理（从后往前删）
        m_depthTextures.resize(count);
        m_depthConfigs.resize(count);

        // 如果活动索引超出范围，重置为0
        if (m_currentActiveDepthIndex >= count)
        {
            m_currentActiveDepthIndex = 0;
        }
    }
    // 场景3: 数量不变，无操作
}

void DepthTextureManager::SetDepthTextureResolution(int index, int width, int height)
{
    // 参数验证
    if (index < 0 || index >= static_cast<int>(m_depthTextures.size()))
    {
        throw std::out_of_range(
            "Depth texture index " + std::to_string(index) +
            " out of range [0-" + std::to_string(m_depthTextures.size() - 1) + "]"
        );
    }

    if (width <= 0 || height <= 0)
    {
        throw std::invalid_argument(
            "Width and height must be greater than zero, got " +
            std::to_string(width) + "x" + std::to_string(height)
        );
    }

    // depthtex0不可修改（强制=渲染分辨率）
    if (index == 0)
    {
        throw std::invalid_argument(
            "Cannot modify depthtex0 resolution (always equals render resolution)"
        );
    }

    // 更新配置
    m_depthConfigs[index].width  = width;
    m_depthConfigs[index].height = height;

    // 重建深度纹理
    const auto& config = m_depthConfigs[index];

    DepthFormat depthFormat = DepthFormat::D32_FLOAT;
    if (config.format == DepthFormat::D24_UNORM_S8_UINT)
    {
        depthFormat = DepthFormat::D24_UNORM_S8_UINT;
    }

    DepthTextureCreateInfo createInfo(
        config.semanticName.c_str(),
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        depthFormat,
        1.0f,
        0
    );

    // 替换旧的深度纹理（智能指针自动释放旧资源）
    m_depthTextures[index] = std::make_shared<D12DepthTexture>(createInfo);
}

int DepthTextureManager::GetDepthTextureCount() const noexcept
{
    return static_cast<int>(m_depthTextures.size());
}

DepthTextureConfig DepthTextureManager::GetDepthTextureConfig(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_depthConfigs.size()))
    {
        throw std::out_of_range(
            "Depth texture config index " + std::to_string(index) +
            " out of range [0-" + std::to_string(m_depthConfigs.size() - 1) + "]"
        );
    }

    return m_depthConfigs[index];
}

// ============================================================================
// 调试支持
// ============================================================================

std::string DepthTextureManager::GetDebugInfo() const
{
    std::ostringstream oss;

    oss << "DepthTextureManager (Render: " << m_renderWidth << "x" << m_renderHeight << "):\n";
    oss << "Total Depth Textures: " << m_depthTextures.size() << "\n";

    for (size_t i = 0; i < m_depthTextures.size(); ++i)
    {
        // M3.1: 使用配置信息
        if (i < m_depthConfigs.size())
        {
            const auto& config = m_depthConfigs[i];
            oss << "  [" << i << "] " << config.semanticName
                << " (" << config.width << "x" << config.height << ")";

            // 标记主深度纹理
            if (i == 0)
            {
                oss << " - 主深度纹理 (包含所有几何体)";
            }
            else if (i == 1)
            {
                oss << " - 不含半透明深度 (TERRAIN_TRANSLUCENT前复制)";
            }
            else if (i == 2)
            {
                oss << " - 不含手部深度 (HAND_SOLID前复制)";
            }

            oss << "\n";
        }

        auto depthTex = m_depthTextures[i];
        if (depthTex)
        {
            oss << "      Format: ";
            if (i < m_depthConfigs.size())
            {
                switch (m_depthConfigs[i].format)
                {
                case DepthFormat::D24_UNORM_S8_UINT:
                    oss << "D24_UNORM_S8_UINT";
                    break;
                case DepthFormat::D32_FLOAT:
                    oss << "D32_FLOAT";
                    break;
                case DepthFormat::D16_UNORM:
                    oss << "D16_UNORM";
                    break;
                default:
                    oss << "UNKNOWN";
                    break;
                }
            }
            else
            {
                oss << "UNKNOWN";
            }

            oss << ", Bindless Index: " << depthTex->GetBindlessIndex() << "\n";
        }
        else
        {
            oss << "      [NULL TEXTURE]\n";
        }
    }

    return oss.str();
}
