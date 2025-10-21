#include "RenderTargetManager.hpp"
#include "D12RenderTarget.hpp"
#include "../Resource/Buffer/D12Buffer.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"

#include <sstream>
#include <stdexcept>

using namespace enigma::graphic;

// ============================================================================
// RenderTargetManager 构造函数 - 创建16个RenderTarget
// ============================================================================

RenderTargetManager::RenderTargetManager(
    int                                         baseWidth,
    int                                         baseHeight,
    const std::array<RenderTargetSettings, 16>& rtSettings
)
    : m_baseWidth(baseWidth)
      , m_baseHeight(baseHeight)
      , m_settings(rtSettings)
      , m_flipState() // 默认构造 (所有RT初始状态: 读Main写Alt)
{
    // 参数验证
    if (baseWidth <= 0 || baseHeight <= 0)
    {
        throw std::invalid_argument("Base width and height must be greater than zero");
    }

    // 遍历16个RT配置,使用Builder模式创建
    for (int i = 0; i < 16; ++i)
    {
        const auto& settings = rtSettings[i];

        // 计算当前RT的实际尺寸 (考虑widthScale/heightScale)
        int rtWidth  = static_cast<int>(baseWidth * settings.widthScale);
        int rtHeight = static_cast<int>(baseHeight * settings.heightScale);

        // 确保尺寸至少为1x1 (防止0尺寸RT)
        rtWidth  = (rtWidth > 0) ? rtWidth : 1;
        rtHeight = (rtHeight > 0) ? rtHeight : 1;

        // 使用Builder模式创建D12RenderTarget
        auto rtBuilder = D12RenderTarget::Create()
                         .SetFormat(settings.format)
                         .SetDimensions(rtWidth, rtHeight)
                         .SetLinearFilter(settings.allowLinearFilter)
                         .SetSampleCount(settings.sampleCount)
                         .EnableMipmap(settings.enableMipmap);

        // 设置调试名称 (例如: "colortex0")
        char debugName[32];
        sprintf_s(debugName, "colortex%d", i);
        rtBuilder.SetName(debugName);

        // 构建RT实例
        m_renderTargets[i] = rtBuilder.Build();

        // 注册Bindless索引 (自动创建SRV描述符)
        m_renderTargets[i]->RegisterBindless();
    }

    // 教学要点:
    // 1. std::array<std::shared_ptr<D12RenderTarget>, 16>管理16个RT
    // 2. Builder模式统一创建流程
    // 3. widthScale/heightScale实现分辨率缩放 (例如: 0.5 = 半分辨率)
    // 4. RegisterBindless()自动分配索引并创建SRV描述符
}

// ============================================================================
// RTV访问接口 - 用于OMSetRenderTargets()
// ============================================================================

D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetManager::GetMainRTV(int rtIndex) const
{
    if (!IsValidIndex(rtIndex))
    {
        throw std::out_of_range("RenderTarget index out of range [0-15]");
    }

    return m_renderTargets[rtIndex]->GetMainRTV();
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetManager::GetAltRTV(int rtIndex) const
{
    if (!IsValidIndex(rtIndex))
    {
        throw std::out_of_range("RenderTarget index out of range [0-15]");
    }

    return m_renderTargets[rtIndex]->GetAltRTV();
}

// ============================================================================
// Bindless索引访问 - 用于着色器ResourceDescriptorHeap访问
// ============================================================================

uint32_t RenderTargetManager::GetMainTextureIndex(int rtIndex) const
{
    if (!IsValidIndex(rtIndex))
    {
        throw std::out_of_range("RenderTarget index out of range [0-15]");
    }

    return m_renderTargets[rtIndex]->GetMainTextureIndex();
}

uint32_t RenderTargetManager::GetAltTextureIndex(int rtIndex) const
{
    if (!IsValidIndex(rtIndex))
    {
        throw std::out_of_range("RenderTarget index out of range [0-15]");
    }

    return m_renderTargets[rtIndex]->GetAltTextureIndex();
}

// ============================================================================
// GPU常量缓冲上传 - RenderTargetsBuffer生成
// ============================================================================

uint32_t RenderTargetManager::CreateRenderTargetsBuffer()
{
    // 定义GPU端RenderTargetsBuffer结构体
    struct RenderTargetsBuffer
    {
        uint32_t readIndices[16]; // 读索引数组 (采样历史帧)
        uint32_t writeIndices[16]; // 写索引数组 (渲染输出)
    };

    RenderTargetsBuffer bufferData{};

    // 根据FlipState确定每个RT的读/写索引
    for (int i = 0; i < 16; ++i)
    {
        bool isFlipped = m_flipState.IsFlipped(i);

        if (!isFlipped)
        {
            // 未翻转: 读Main写Alt
            bufferData.readIndices[i]  = GetMainTextureIndex(i);
            bufferData.writeIndices[i] = GetAltTextureIndex(i);
        }
        else
        {
            // 已翻转: 读Alt写Main
            bufferData.readIndices[i]  = GetAltTextureIndex(i);
            bufferData.writeIndices[i] = GetMainTextureIndex(i);
        }
    }

    // 创建D12Buffer并上传到GPU
    BufferCreateInfo bufferInfo{};
    bufferInfo.size         = sizeof(RenderTargetsBuffer);
    bufferInfo.usage        = BufferUsage::ConstantBuffer;
    bufferInfo.memoryAccess = MemoryAccess::GPUOnly; // GPU专用 (最高性能)
    bufferInfo.initialData  = &bufferData; // 初始数据
    bufferInfo.debugName    = "RenderTargetsBuffer";

    auto renderTargetsBuffer = std::make_shared<D12Buffer>(bufferInfo);

    // 上传到GPU (通过Upload队列)
    renderTargetsBuffer->Upload();

    // 注册Bindless并返回索引
    renderTargetsBuffer->RegisterBindless();
    uint32_t bufferIndex = renderTargetsBuffer->GetBindlessIndex();

    // 教学要点:
    // 1. 根据m_flipState动态生成读/写索引数组
    // 2. 使用D12Buffer::ConstantBuffer创建常量缓冲
    // 3. 通过Upload()上传到GPU (内部使用Copy队列)
    // 4. RegisterBindless()分配索引供着色器访问
    // 5. 着色器通过: cbuffer RenderTargets : register(b0) { ... }访问

    return bufferIndex;
}

// ============================================================================
// 窗口尺寸变化响应 - 自动Resize所有RT
// ============================================================================

void RenderTargetManager::OnResize(int newBaseWidth, int newBaseHeight)
{
    if (newBaseWidth <= 0 || newBaseHeight <= 0)
    {
        throw std::invalid_argument("New base width and height must be greater than zero");
    }

    m_baseWidth  = newBaseWidth;
    m_baseHeight = newBaseHeight;

    // 遍历16个RT,调用ResizeIfNeeded()
    for (int i = 0; i < 16; ++i)
    {
        const auto& settings = m_settings[i];

        // 计算新尺寸 (考虑widthScale/heightScale)
        int newWidth  = static_cast<int>(newBaseWidth * settings.widthScale);
        int newHeight = static_cast<int>(newBaseHeight * settings.heightScale);

        // 确保尺寸至少为1x1
        newWidth  = (newWidth > 0) ? newWidth : 1;
        newHeight = (newHeight > 0) ? newHeight : 1;

        // 如果尺寸变化,自动Resize并重新注册Bindless
        bool resized = m_renderTargets[i]->ResizeIfNeeded(newWidth, newHeight);

        if (resized)
        {
            // Resize后需要重新注册Bindless索引
            // (D12RenderTarget::Resize()内部已处理)
            // 教学要点: D12RenderTarget内部已实现自动重新注册
        }
    }

    // 教学要点:
    // 1. ResizeIfNeeded()内部比较尺寸,仅在变化时执行Resize
    // 2. D12RenderTarget::Resize()内部自动处理:
    //    - 释放旧纹理资源
    //    - 创建新纹理资源
    //    - 重新注册Bindless索引
    // 3. widthScale/heightScale确保RT相对屏幕的缩放比例不变
}

// ============================================================================
// Mipmap生成 - Milestone 3.0延迟渲染管线
// ============================================================================

void RenderTargetManager::GenerateMipmaps(ID3D12GraphicsCommandList* cmdList)
{
    // 参数验证
    if (!cmdList)
    {
        throw std::invalid_argument("Command list cannot be null");
    }

    // 遍历16个RenderTarget
    for (int i = 0; i < 16; ++i)
    {
        // 检查是否启用Mipmap
        if (!m_settings[i].enableMipmap || !m_renderTargets[i])
        {
            continue;
        }

        // 对Main纹理生成Mipmap
        auto mainTex = m_renderTargets[i]->GetMainTexture();
        if (mainTex)
        {
            mainTex->GenerateMips();
        }

        // 对Alt纹理生成Mipmap
        auto altTex = m_renderTargets[i]->GetAltTexture();
        if (altTex)
        {
            altTex->GenerateMips();
        }
    }

    // 教学要点:
    // 1. Mipmap生成通常在写入RenderTarget后调用
    // 2. 对Main和Alt纹理都生成Mipmap，确保Ping-Pong双缓冲机制正常
    // 3. GenerateMips()内部使用Compute Shader实现高效Mipmap生成
    // 4. Mipmap提供多级细节纹理，减少远距离采样走样
}

// ============================================================================
// 调试支持 - 查询RT状态
// ============================================================================

std::string RenderTargetManager::GetDebugInfo(int rtIndex) const
{
    if (!IsValidIndex(rtIndex))
    {
        return "Invalid RenderTarget index";
    }

    const auto& rt        = m_renderTargets[rtIndex];
    bool        isFlipped = m_flipState.IsFlipped(rtIndex);

    std::ostringstream oss;
    oss << "=== RenderTarget " << rtIndex << " (colortex" << rtIndex << ") ===\n";
    oss << "Flip State: " << (isFlipped ? "Flipped (Read Alt, Write Main)" : "Normal (Read Main, Write Alt)") << "\n";
    oss << "Main Texture Index: " << rt->GetMainTextureIndex() << "\n";
    oss << "Alt Texture Index: " << rt->GetAltTextureIndex() << "\n";
    oss << "Settings:\n";
    oss << "  Width Scale: " << m_settings[rtIndex].widthScale << "\n";
    oss << "  Height Scale: " << m_settings[rtIndex].heightScale << "\n";
    oss << "  Format: " << m_settings[rtIndex].format << "\n";
    oss << "  Mipmap: " << (m_settings[rtIndex].enableMipmap ? "Yes" : "No") << "\n";
    oss << "  MSAA: " << m_settings[rtIndex].sampleCount << "x\n";
    oss << "\n" << rt->GetDebugInfo();

    return oss.str();
}

std::string RenderTargetManager::GetAllRenderTargetsInfo() const
{
    std::ostringstream oss;
    oss << "=== RenderTargetManager Overview ===\n";
    oss << "Base Resolution: " << m_baseWidth << "x" << m_baseHeight << "\n";
    oss << "Total RenderTargets: 16\n\n";

    // 表格表头
    oss << "Index | Name      | Resolution  | Format | Flip | Main Index | Alt Index\n";
    oss << "------|-----------|-------------|--------|------|------------|-----------\n";

    for (int i = 0; i < 16; ++i)
    {
        const auto& rt       = m_renderTargets[i];
        const auto& settings = m_settings[i];

        int         rtWidth    = static_cast<int>(m_baseWidth * settings.widthScale);
        int         rtHeight   = static_cast<int>(m_baseHeight * settings.heightScale);
        bool        isFlipped  = m_flipState.IsFlipped(i);
        std::string resolution = std::to_string(rtWidth) + "x" + std::to_string(rtHeight);

        char line[256];
        sprintf_s(line, "%-5d | colortex%-1d | %-11s | %-6d | %-4s | %-10u | %-10u\n",
                  i,
                  i,
                  resolution.c_str(),
                  settings.format,
                  isFlipped ? "Yes" : "No",
                  rt->GetMainTextureIndex(),
                  rt->GetAltTextureIndex()
        );
        oss << line;
    }

    return oss.str();
}
