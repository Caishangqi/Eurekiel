#include "RenderTargetManager.hpp"
#include "D12RenderTarget.hpp"
#include "../Resource/Buffer/D12Buffer.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"

#include <sstream>
#include <stdexcept>

using namespace enigma::graphic;

// ============================================================================
// RenderTargetManager 构造函数 - 创建动态数量的RenderTarget (Milestone 3.0)
// ============================================================================

RenderTargetManager::RenderTargetManager(
    int                             baseWidth,
    int                             baseHeight,
    const std::array<RTConfig, 16>& rtConfigs,
    int                             colorTexCount
)
    : m_baseWidth(baseWidth)
      , m_baseHeight(baseHeight)
      , m_settings(rtConfigs)
      , m_flipState() // 默认构造 (所有RT初始状态: 读Main写Alt)
{
    // 参数验证 - 尺寸
    if (baseWidth <= 0 || baseHeight <= 0)
    {
        throw std::invalid_argument("Base width and height must be greater than zero");
    }

    // 参数验证 - colorTexCount范围 [1, 16]
    if (colorTexCount < MIN_COLOR_TEXTURES || colorTexCount > MAX_COLOR_TEXTURES)
    {
        // 记录警告并修正为默认值
        // LogError("RenderTargetManager",
        //          "colorTexCount {} out of range [{}, {}]. Using default {}.",
        //          colorTexCount, MIN_COLOR_TEXTURES, MAX_COLOR_TEXTURES, MAX_COLOR_TEXTURES);

        // 修正为默认值（16个）
        colorTexCount = MAX_COLOR_TEXTURES;
    }

    // 保存激活的colortex数量
    m_activeColorTexCount = colorTexCount;

    // LogInfo("RenderTargetManager",
    //         "Initializing with {} active colortex (max: {})",
    //         m_activeColorTexCount, MAX_COLOR_TEXTURES);

    // 调整vector大小为激活数量（内存优化核心）
    m_renderTargets.resize(m_activeColorTexCount);

    // 遍历激活的RT配置，使用Builder模式创建
    for (int i = 0; i < m_activeColorTexCount; ++i)
    {
        const auto& config = rtConfigs[i];

        // 计算当前RT的实际尺寸 (考虑widthScale/heightScale)
        int rtWidth  = static_cast<int>(baseWidth * config.widthScale);
        int rtHeight = static_cast<int>(baseHeight * config.heightScale);

        // 确保尺寸至少为1x1 (防止0尺寸RT)
        rtWidth  = (rtWidth > 0) ? rtWidth : 1;
        rtHeight = (rtHeight > 0) ? rtHeight : 1;

        // 使用Builder模式创建D12RenderTarget
        auto rtBuilder = D12RenderTarget::Create()
                         .SetFormat(config.format)
                         .SetDimensions(rtWidth, rtHeight)
                         .SetLinearFilter(config.allowLinearFilter)
                         .SetSampleCount(config.sampleCount)
                         .EnableMipmap(config.enableMipmap);

        // 设置调试名称 (例如: "colortex0")
        char debugName[32];
        sprintf_s(debugName, "colortex%d", i);
        rtBuilder.SetName(debugName);

        // 构建RT实例
        m_renderTargets[i] = rtBuilder.Build();

        // Milestone 3.0 Bug Fix: Correct Bindless Registration Flow
        // True Bindless Flow: Create() → Upload() → RegisterBindless()
        //
        // Why Upload() is required even for RenderTarget:
        // - Upload() sets m_isUploaded = true flag (required by RegisterBindless safety check)
        // - Upload() performs resource state transition (COMMON -> RENDER_TARGET)
        // - UploadToGPU() returns true for RenderTarget (no actual data transfer needed)
        //
        // Educational Note:
        // - RenderTarget is GPU output texture, no CPU data to upload
        // - But Upload() still performs state management and flag setting
        // - This follows Template Method pattern: base Upload() + virtual UploadToGPU()
        m_renderTargets[i]->Upload();
        m_renderTargets[i]->RegisterBindless();

        // LogInfo("RenderTargetManager",
        //         "Created colortex{} ({}x{}, format: {})",
        //         i, rtWidth, rtHeight, static_cast<int>(settings.format));
    }

    // LogInfo("RenderTargetManager",
    //         "✓ RenderTargetManager initialized with {}/{} active colortex",
    //         m_activeColorTexCount, MAX_COLOR_TEXTURES);

    // 教学要点:
    // 1. **动态数量支持**: std::vector<std::shared_ptr<D12RenderTarget>>管理1-16个RT
    // 2. **参数验证**: colorTexCount范围检查 + 超出范围自动修正
    // 3. **内存优化**: 仅创建激活数量的RT（例如4个节省75%内存）
    // 4. **Builder模式**: 统一创建流程
    // 5. **widthScale/heightScale**: 实现分辨率缩放 (例如: 0.5 = 半分辨率)
    // 6. **RegisterBindless()**: 自动分配索引并创建SRV描述符
}

// ============================================================================
// RTV访问接口 - 用于OMSetRenderTargets()
// ============================================================================

D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetManager::GetMainRTV(int rtIndex) const
{
    if (!IsValidIndex(rtIndex))
    {
        // 更新错误信息：显示实际激活数量
        char errorMsg[128];
        sprintf_s(errorMsg, "RenderTarget index %d out of range [0, %d)", rtIndex, m_activeColorTexCount);
        throw std::out_of_range(errorMsg);
    }

    return m_renderTargets[rtIndex]->GetMainRTV();
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetManager::GetAltRTV(int rtIndex) const
{
    if (!IsValidIndex(rtIndex))
    {
        // 更新错误信息：显示实际激活数量
        char errorMsg[128];
        sprintf_s(errorMsg, "RenderTarget index %d out of range [0, %d)", rtIndex, m_activeColorTexCount);
        throw std::out_of_range(errorMsg);
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
        // 更新错误信息：显示实际激活数量
        char errorMsg[128];
        sprintf_s(errorMsg, "RenderTarget index %d out of range [0, %d)", rtIndex, m_activeColorTexCount);
        throw std::out_of_range(errorMsg);
    }

    return m_renderTargets[rtIndex]->GetMainTextureIndex();
}

uint32_t RenderTargetManager::GetAltTextureIndex(int rtIndex) const
{
    if (!IsValidIndex(rtIndex))
    {
        // 更新错误信息：显示实际激活数量
        char errorMsg[128];
        sprintf_s(errorMsg, "RenderTarget index %d out of range [0, %d)", rtIndex, m_activeColorTexCount);
        throw std::out_of_range(errorMsg);
    }

    return m_renderTargets[rtIndex]->GetAltTextureIndex();
}

DXGI_FORMAT RenderTargetManager::GetRenderTargetFormat(int rtIndex) const
{
    if (!IsValidIndex(rtIndex))
    {
        char errorMsg[128];
        sprintf_s(errorMsg, "RenderTarget index %d out of range [0, %d)", rtIndex, m_activeColorTexCount);
        // ERROR_RECOVERABLE(errorMsg);
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    if (!m_renderTargets[rtIndex])
    {
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    return m_renderTargets[rtIndex]->GetFormat();
}

// ============================================================================
// BufferFlipState管理 - Main/Alt翻转逻辑
// ============================================================================

void RenderTargetManager::FlipRenderTarget(int rtIndex)
{
    // 边界检查
    if (!IsValidIndex(rtIndex))
    {
        char errorMsg[128];
        sprintf_s(errorMsg, "FlipRenderTarget: rtIndex %d out of range [0, %d)", rtIndex, m_activeColorTexCount);
        throw std::out_of_range(errorMsg);
    }

    // 执行翻转
    m_flipState.Flip(rtIndex);

    // 调试日志（注释掉，避免性能影响）
    // LogDebug("RenderTargetManager", "FlipRenderTarget: rtIndex=%d, isFlipped=%d",
    //          rtIndex, m_flipState.IsFlipped(rtIndex));
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
    // **Milestone 3.0更新**: 只遍历激活的colortex数量
    for (int i = 0; i < m_activeColorTexCount; ++i)
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

    // 未激活的colortex索引填充为0（着色器访问时应避免使用）
    for (int i = m_activeColorTexCount; i < MAX_COLOR_TEXTURES; ++i)
    {
        bufferData.readIndices[i]  = 0;
        bufferData.writeIndices[i] = 0;
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

    // 遍历激活的RT，调用ResizeIfNeeded()
    // **Milestone 3.0更新**: 只Resize激活的colortex
    for (int i = 0; i < m_activeColorTexCount; ++i)
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
    // 1. **动态数量优化**: 只Resize激活的colortex，未激活的不创建不Resize
    // 2. ResizeIfNeeded()内部比较尺寸,仅在变化时执行Resize
    // 3. D12RenderTarget::Resize()内部自动处理:
    //    - 释放旧纹理资源
    //    - 创建新纹理资源
    //    - 重新注册Bindless索引
    // 4. widthScale/heightScale确保RT相对屏幕的缩放比例不变
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

    // 遍历激活的RenderTarget
    // **Milestone 3.0更新**: 只为激活的colortex生成Mipmap
    for (int i = 0; i < m_activeColorTexCount; ++i)
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
    // 1. **动态数量优化**: 只为激活的colortex生成Mipmap
    // 2. Mipmap生成通常在写入RenderTarget后调用
    // 3. 对Main和Alt纹理都生成Mipmap，确保Ping-Pong双缓冲机制正常
    // 4. GenerateMips()内部使用Compute Shader实现高效Mipmap生成
    // 5. Mipmap提供多级细节纹理，减少远距离采样走样
}

// ============================================================================
// 调试支持 - 查询RT状态
// ============================================================================

std::string RenderTargetManager::GetDebugInfo(int rtIndex) const
{
    if (!IsValidIndex(rtIndex))
    {
        // 返回更详细的错误信息
        std::ostringstream oss;
        oss << "Invalid RenderTarget index: " << rtIndex << "\n";
        oss << "Valid range: [0, " << m_activeColorTexCount << ")\n";
        oss << "Active ColorTex: " << m_activeColorTexCount << " / " << MAX_COLOR_TEXTURES;
        return oss.str();
    }

    const auto& rt        = m_renderTargets[rtIndex];
    bool        isFlipped = m_flipState.IsFlipped(rtIndex);

    std::ostringstream oss;
    oss << "=== RenderTarget " << rtIndex << " (colortex" << rtIndex << ") ===\n";
    oss << "Status: Active (" << rtIndex + 1 << " / " << m_activeColorTexCount << ")\n";
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
    oss << "Active ColorTex: " << m_activeColorTexCount << " / " << MAX_COLOR_TEXTURES << "\n\n";

    // 表格表头
    oss << "Index | Name      | Resolution  | Format | Flip | Main Index | Alt Index | Status\n";
    oss << "------|-----------|-------------|--------|------|------------|-----------|--------\n";

    // 遍历所有可能的colortex（包括未激活的）
    for (int i = 0; i < MAX_COLOR_TEXTURES; ++i)
    {
        // 检查是否激活
        if (i < m_activeColorTexCount)
        {
            // 激活的colortex
            const auto& rt       = m_renderTargets[i];
            const auto& settings = m_settings[i];

            int         rtWidth    = static_cast<int>(m_baseWidth * settings.widthScale);
            int         rtHeight   = static_cast<int>(m_baseHeight * settings.heightScale);
            bool        isFlipped  = m_flipState.IsFlipped(i);
            std::string resolution = std::to_string(rtWidth) + "x" + std::to_string(rtHeight);

            char line[256];
            sprintf_s(line, "%-5d | colortex%-1d | %-11s | %-6d | %-4s | %-10u | %-10u | Active\n",
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
        else
        {
            // 未激活的colortex
            char line[256];
            sprintf_s(line, "%-5d | colortex%-1d | %-11s | %-6s | %-4s | %-10s | %-10s | Inactive\n",
                      i,
                      i,
                      "N/A",
                      "N/A",
                      "N/A",
                      "N/A",
                      "N/A"
            );
            oss << line;
        }
    }

    return oss.str();
}
