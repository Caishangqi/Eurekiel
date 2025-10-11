#include "D12RenderTarget.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "../Resource/GlobalDescriptorHeapManager.hpp"
#include "../Resource/Texture/D12Texture.hpp"

using namespace enigma::graphic;

// ============================================================================
// D12RenderTarget::Builder 实现
// ============================================================================

std::shared_ptr<D12RenderTarget> D12RenderTarget::Builder::Build()
{
    // 参数验证
    if (m_width <= 0 || m_height <= 0)
    {
        throw std::invalid_argument("Width and height must be greater than zero");
    }

    // 使用私有构造函数创建RenderTarget (Builder是friend类)
    return std::shared_ptr<D12RenderTarget>(new D12RenderTarget(*this));
}

// ============================================================================
// D12RenderTarget 构造函数和生命周期管理
// ============================================================================

D12RenderTarget::D12RenderTarget(const Builder& builder)
    : D12Resource()
      , m_mainTexture(nullptr)
      , m_altTexture(nullptr)
      , m_format(builder.m_format)
      , m_width(builder.m_width)
      , m_height(builder.m_height)
      , m_allowLinearFilter(builder.m_allowLinearFilter)
      , m_sampleCount(builder.m_sampleCount)
      , m_enableMipmap(builder.m_enableMipmap)
      , m_mainTextureIndex(INVALID_BINDLESS_INDEX)
      , m_altTextureIndex(INVALID_BINDLESS_INDEX)
{
    // 设置调试名称
    if (!builder.m_name.empty())
    {
        SetDebugName(builder.m_name);
    }

    // 初始化双纹理和描述符
    InitializeTextures(m_width, m_height);
    CreateDescriptors();

    // 标记为有效 (继承自D12Resource)
    m_isValid = (m_mainTexture && m_altTexture);
}

// ============================================================================
// 纹理初始化 (对应Iris setupTexture方法)
// ============================================================================

void D12RenderTarget::InitializeTextures(int width, int height)
{
    // 创建主纹理 (对应Iris mainTexture)
    TextureCreateInfo mainTexInfo{};
    mainTexInfo.type      = TextureType::Texture2D;
    mainTexInfo.width     = static_cast<uint32_t>(width);
    mainTexInfo.height    = static_cast<uint32_t>(height);
    mainTexInfo.depth     = 1;
    mainTexInfo.mipLevels = m_enableMipmap ? 0 : 1; // 0 = 自动生成所有Mip级别
    mainTexInfo.arraySize = 1;
    mainTexInfo.format    = m_format;
    mainTexInfo.usage     = TextureUsage::ShaderResource; // RenderTarget可作为SRV采样
    mainTexInfo.debugName = (GetDebugName() + "_MainTex").c_str();

    m_mainTexture = std::make_shared<D12Texture>(mainTexInfo);

    // 创建替代纹理 (对应Iris altTexture - 用于Ping-Pong渲染)
    TextureCreateInfo altTexInfo = mainTexInfo;
    altTexInfo.debugName         = (GetDebugName() + "_AltTex").c_str();

    m_altTexture = std::make_shared<D12Texture>(altTexInfo);

    // 教学要点: Iris通过双纹理实现Ping-Pong渲染
    // - 一个用于渲染输出
    // - 另一个保存历史帧数据,供着色器采样
}

// ============================================================================
// 描述符创建 (DirectX 12专有)
// ============================================================================

void D12RenderTarget::CreateDescriptors()
{
    auto device = D3D12RenderSystem::GetDevice();
    if (!device)
    {
        throw std::runtime_error("Failed to get D3D12 device for descriptor creation");
    }

    auto heapManager = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
    if (!heapManager)
    {
        throw std::runtime_error("Failed to get GlobalDescriptorHeapManager");
    }

    // 分配RTV描述符 (渲染目标视图 - 用于渲染输出)
    auto mainRtvAlloc = heapManager->AllocateRtv();
    auto altRtvAlloc  = heapManager->AllocateRtv();

    if (!mainRtvAlloc.isValid || !altRtvAlloc.isValid)
    {
        throw std::runtime_error("Failed to allocate RTV descriptors");
    }

    m_mainRTV = mainRtvAlloc.cpuHandle;
    m_altRTV  = altRtvAlloc.cpuHandle;

    // 创建RTV视图
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format                        = m_format;
    rtvDesc.ViewDimension                 = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice            = 0;
    rtvDesc.Texture2D.PlaneSlice          = 0;

    heapManager->CreateRenderTargetView(
        device,
        m_mainTexture->GetResource(),
        &rtvDesc,
        mainRtvAlloc.heapIndex
    );

    heapManager->CreateRenderTargetView(
        device,
        m_altTexture->GetResource(),
        &rtvDesc,
        altRtvAlloc.heapIndex
    );

    // SRV会在纹理注册到Bindless系统时自动创建
    // 这里只获取纹理的SRV句柄供传统绑定使用
    m_mainSRV = m_mainTexture->GetSRVHandle();
    m_altSRV  = m_altTexture->GetSRVHandle();

    // 教学要点:
    // - RTV用于渲染输出 (OMSetRenderTargets)
    // - SRV用于着色器采样 (SetGraphicsRootDescriptorTable)
    // - DirectX 12需要显式创建每种视图类型
}

// ============================================================================
// Bindless索引注册 (Milestone 3.0核心功能)
// ============================================================================

void D12RenderTarget::CreateDescriptorInGlobalHeap(
    ID3D12Device*                device,
    GlobalDescriptorHeapManager* heapManager)
{
    if (!IsBindlessRegistered())
    {
        return; // 未注册Bindless,不需要创建描述符
    }

    // 获取主纹理和替代纹理的Bindless索引
    m_mainTextureIndex = m_mainTexture->GetBindlessIndex();
    m_altTextureIndex  = m_altTexture->GetBindlessIndex();

    // 教学要点:
    // 1. RenderTarget的Bindless注册委托给内部D12Texture
    // 2. m_mainTexture和m_altTexture各自管理自己的Bindless索引
    // 3. 双纹理索引存储在RenderTarget中,供RenderTargetManager使用
    // 4. 着色器通过这些索引访问: ResourceDescriptorHeap[mainTextureIndex]

    UNUSED(device)
    UNUSED(heapManager)
}

// ============================================================================
// 尺寸管理 (对应Iris resize方法)
// ============================================================================

void D12RenderTarget::Resize(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        throw std::invalid_argument("Width and height must be greater than zero");
    }

    m_width  = width;
    m_height = height;

    // 重新创建纹理和描述符
    InitializeTextures(width, height);
    CreateDescriptors();

    // 如果之前已注册Bindless,需要重新注册
    if (IsBindlessRegistered())
    {
        UnregisterBindless(); // 注销旧索引
        RegisterBindless(); // 分配新索引并创建描述符
    }

    // 教学要点: Iris的resize逻辑 - 完全重建纹理资源
}

bool D12RenderTarget::ResizeIfNeeded(int width, int height)
{
    if (m_width == width && m_height == height)
    {
        return false; // 尺寸未变化,无需resize
    }

    Resize(width, height);
    return true;
}

// ============================================================================
// 调试支持 (重写D12Resource虚函数)
// ============================================================================

void D12RenderTarget::SetDebugName(const std::string& name)
{
    D12Resource::SetDebugName(name); // 调用基类实现

    // 同时更新双纹理的调试名称
    if (m_mainTexture)
    {
        m_mainTexture->SetDebugName(name + "_MainTex");
    }
    if (m_altTexture)
    {
        m_altTexture->SetDebugName(name + "_AltTex");
    }

    // 清空缓存的格式化名称,强制重新生成
    m_formattedDebugName.clear();
}

const std::string& D12RenderTarget::GetDebugName() const
{
    // 延迟格式化: 只在第一次调用或名称变更时生成
    if (m_formattedDebugName.empty())
    {
        // 格式化示例: "ColorTarget0 (1920x1080, RGBA8)"
        const char* formatName = "Unknown";
        switch (m_format)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM: formatName = "RGBA8";
            break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT: formatName = "RGBA16F";
            break;
        case DXGI_FORMAT_R32G32B32A32_FLOAT: formatName = "RGBA32F";
            break;
        default: formatName = "Custom";
            break;
        }

        char buffer[256];
        sprintf_s(buffer, "%s (%dx%d, %s)",
                  m_debugName.c_str(),
                  m_width, m_height,
                  formatName);
        m_formattedDebugName = buffer;
    }

    return m_formattedDebugName;
}

std::string D12RenderTarget::GetDebugInfo() const
{
    char buffer[512];
    sprintf_s(buffer,
              "RenderTarget: %s\n"
              "  Dimensions: %dx%d\n"
              "  Format: %d (DXGI)\n"
              "  Sample Count: %d\n"
              "  Mipmap: %s\n"
              "  Main Texture Index: %u\n"
              "  Alt Texture Index: %u\n"
              "  Bindless Registered: %s\n"
              "  Valid: %s",
              m_debugName.c_str(),
              m_width, m_height,
              m_format,
              m_sampleCount,
              m_enableMipmap ? "Yes" : "No",
              m_mainTextureIndex,
              m_altTextureIndex,
              IsBindlessRegistered() ? "Yes" : "No",
              IsValid() ? "Yes" : "No"
    );

    return std::string(buffer);
}

// ============================================================================
// Bindless资源类型 (重写D12Resource虚函数)
// ============================================================================

/**
 * @brief 分配RenderTarget专属的Bindless索引
 *
 * 教学要点:
 * 1. RenderTarget索引范围: 0 - 999,999 (1M个纹理容量)
 * 2. 调用BindlessIndexAllocator::AllocateTextureIndex()
 * 3. RenderTarget作为纹理资源,使用纹理索引而非缓冲区索引
 * 4. 双纹理架构: 此函数仅为主纹理分配索引,替代纹理索引由内部管理
 */
uint32_t D12RenderTarget::AllocateBindlessIndexInternal(BindlessIndexAllocator* allocator) const
{
    if (!allocator)
    {
        return BindlessIndexAllocator::INVALID_INDEX;
    }

    // 调用纹理索引分配器
    // RenderTarget的主纹理和替代纹理都使用纹理索引范围
    return allocator->AllocateTextureIndex();
}

/**
 * @brief 释放RenderTarget专属的Bindless索引
 *
 * 教学要点:
 * 1. 必须使用FreeTextureIndex()释放纹理索引
 * 2. 不能使用FreeBufferIndex()释放纹理索引(会导致索引泄漏)
 * 3. FreeList架构保证O(1)时间复杂度
 * 4. 双纹理架构: 主纹理和替代纹理索引都需要释放
 */
bool D12RenderTarget::FreeBindlessIndexInternal(BindlessIndexAllocator* allocator, uint32_t index) const
{
    if (!allocator)
    {
        return false;
    }

    // 调用纹理索引释放器
    return allocator->FreeTextureIndex(index);
}

// ============================================================================
// GPU资源上传 (重写D12Resource纯虚函数)
// ============================================================================

bool D12RenderTarget::UploadToGPU(
    ID3D12GraphicsCommandList* commandList,
    UploadContext&             uploadContext)
{
    UNUSED(commandList)
    UNUSED(uploadContext)

    // RenderTarget不需要上传初始数据
    // 它是渲染输出目标,由GPU直接写入,不需要CPU数据上传

    // 教学要点:
    // 1. RenderTarget与Texture的区别:
    //    - Texture: 需要上传贴图数据 (DDS/PNG等)
    //    - RenderTarget: GPU直接渲染输出,无需CPU数据
    // 2. 返回true表示"上传成功"(实际上无需上传)
    // 3. 状态转换由基类Upload()处理

    return true; // 无需上传,直接成功
}

D3D12_RESOURCE_STATES D12RenderTarget::GetUploadDestinationState() const
{
    // RenderTarget上传后的目标状态: RENDER_TARGET
    // 因为RenderTarget主要用于渲染输出

    // 教学要点:
    // 1. RENDER_TARGET状态允许作为OMSetRenderTargets()的输出
    // 2. 后续如果需要作为SRV采样,需要转换到PIXEL_SHADER_RESOURCE
    // 3. 这是RenderTarget的典型状态转换流程

    return D3D12_RESOURCE_STATE_RENDER_TARGET;
}
