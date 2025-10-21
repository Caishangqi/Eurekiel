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
// DepthTextureManager 构造函数 - 创建3个深度纹理
// ============================================================================

DepthTextureManager::DepthTextureManager(
    int         width,
    int         height,
    DXGI_FORMAT depthFormat
)
    : m_width(width)
      , m_height(height)
      , m_depthFormat(depthFormat)
{
    // 参数验证
    if (width <= 0 || height <= 0)
    {
        throw std::invalid_argument("Width and height must be greater than zero");
    }

    // 深度格式验证
    if (depthFormat != DXGI_FORMAT_D24_UNORM_S8_UINT &&
        depthFormat != DXGI_FORMAT_D32_FLOAT &&
        depthFormat != DXGI_FORMAT_D32_FLOAT_S8X24_UINT &&
        depthFormat != DXGI_FORMAT_D16_UNORM)
    {
        throw std::invalid_argument("Invalid depth format. Must be a valid depth/stencil format.");
    }

    // 创建3个深度纹理 (depthtex0/1/2)
    const char* depthNames[3] = {"depthtex0", "depthtex1", "depthtex2"};

    // 根据格式确定深度类型
    DepthType depthType = DepthType::DepthStencil; // 默认D24_UNORM_S8_UINT
    if (depthFormat == DXGI_FORMAT_D32_FLOAT || depthFormat == DXGI_FORMAT_D16_UNORM)
    {
        depthType = DepthType::DepthOnly;
    }

    for (int i = 0; i < 3; ++i)
    {
        // 使用DepthTextureCreateInfo创建深度纹理
        DepthTextureCreateInfo createInfo(
            depthNames[i], // name
            static_cast<uint32_t>(width), // width
            static_cast<uint32_t>(height), // height
            depthType, // depthType
            1.0f, // clearDepth
            0 // clearStencil
        );

        // 创建D12DepthTexture（DSV自动创建）
        m_depthTextures[i] = std::make_shared<D12DepthTexture>(createInfo);
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

    cmdList->ResourceBarrier(2, barriers);

    // 2. 复制深度纹理
    cmdList->CopyResource(destResource, srcResource);

    // 3. 恢复资源状态: COPY_SOURCE/DEST -> DSV
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    cmdList->ResourceBarrier(2, barriers);
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

    // 更新尺寸
    m_width  = newWidth;
    m_height = newHeight;

    // 重新创建3个深度纹理（使用D12DepthTexture::Resize()）
    for (int i = 0; i < 3; ++i)
    {
        if (m_depthTextures[i])
        {
            bool success = m_depthTextures[i]->Resize(
                static_cast<uint32_t>(newWidth),
                static_cast<uint32_t>(newHeight)
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
// 调试支持
// ============================================================================

std::string DepthTextureManager::GetDebugInfo() const
{
    std::ostringstream oss;

    oss << "DepthTextureManager (" << m_width << "x" << m_height << "):\n";

    for (int i = 0; i < 3; ++i)
    {
        const char* names[3]    = {"depthtex0", "depthtex1", "depthtex2"};
        const char* purposes[3] = {
            "主深度纹理 (包含所有几何体)",
            "不含半透明深度 (TERRAIN_TRANSLUCENT前复制)",
            "不含手部深度 (HAND_SOLID前复制)"
        };

        oss << "  [" << i << "] " << names[i] << " - " << purposes[i] << "\n";

        auto depthTex = m_depthTextures[i];
        if (depthTex)
        {
            oss << "      Format: ";
            switch (m_depthFormat)
            {
            case DXGI_FORMAT_D24_UNORM_S8_UINT:
                oss << "D24_UNORM_S8_UINT";
                break;
            case DXGI_FORMAT_D32_FLOAT:
                oss << "D32_FLOAT";
                break;
            case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
                oss << "D32_FLOAT_S8X24_UINT";
                break;
            case DXGI_FORMAT_D16_UNORM:
                oss << "D16_UNORM";
                break;
            default:
                oss << "UNKNOWN";
                break;
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
