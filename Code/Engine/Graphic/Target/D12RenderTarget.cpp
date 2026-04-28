#include "D12RenderTarget.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "../Mipmap/MipmapConfig.hpp"
#include "../Resource/GlobalDescriptorHeapManager.hpp"
#include "../Resource/Texture/D12Texture.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

using namespace enigma::graphic;

// ============================================================================
// D12RenderTarget::Builder
// ============================================================================

std::shared_ptr<D12RenderTarget> D12RenderTarget::Builder::Build()
{
    if (m_width <= 0 || m_height <= 0)
    {
        throw std::invalid_argument("Width and height must be greater than zero");
    }

    return std::shared_ptr<D12RenderTarget>(new D12RenderTarget(*this));
}

// ============================================================================
// D12RenderTarget lifecycle
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
      , m_clearValue(builder.m_clearValue)
      , m_mainTextureIndex(INVALID_BINDLESS_INDEX)
      , m_altTextureIndex(INVALID_BINDLESS_INDEX)
      , m_mainRtvHeapIndex(UINT32_MAX)
      , m_altRtvHeapIndex(UINT32_MAX)
{
    if (!builder.m_name.empty())
    {
        SetDebugName(builder.m_name);
    }

    InitializeTextures(m_width, m_height);
    CreateDescriptors();

    m_isValid = (m_mainTexture && m_altTexture);
}

// ============================================================================
// Texture initialization
// ============================================================================

void D12RenderTarget::InitializeTextures(int width, int height)
{
    TextureCreateInfo mainTexInfo{};
    mainTexInfo.type      = TextureType::Texture2D;
    mainTexInfo.width     = static_cast<uint32_t>(width);
    mainTexInfo.height    = static_cast<uint32_t>(height);
    mainTexInfo.depth     = 1;
    mainTexInfo.mipLevels = m_enableMipmap ? CalculateMipCount(width, height) : 1;
    mainTexInfo.arraySize = 1;
    mainTexInfo.format    = m_format;
    mainTexInfo.usage      = TextureUsage::RenderTarget | TextureUsage::ShaderResource;

    if (m_enableMipmap)
    {
        mainTexInfo.usage = mainTexInfo.usage | TextureUsage::UnorderedAccess;
    }
    mainTexInfo.clearValue = m_clearValue;

    std::string mainTexDebugName = GetDebugName() + "_MainTex";
    mainTexInfo.debugName        = mainTexDebugName.c_str();

    m_mainTexture = std::make_shared<D12Texture>(mainTexInfo);

    TextureCreateInfo altTexInfo = mainTexInfo;

    std::string altTexDebugName = GetDebugName() + "_AltTex";
    altTexInfo.debugName        = altTexDebugName.c_str();

    m_altTexture = std::make_shared<D12Texture>(altTexInfo);
}

// ============================================================================
// Descriptor creation
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

    if (m_mainRtvHeapIndex == UINT32_MAX)
    {
        auto mainRtvAlloc = heapManager->AllocateRtv();
        if (!mainRtvAlloc.isValid)
        {
            throw std::runtime_error("Failed to allocate main RTV descriptor");
        }

        m_mainRTV          = mainRtvAlloc.cpuHandle;
        m_mainRtvHeapIndex = mainRtvAlloc.heapIndex;
    }

    if (m_altRtvHeapIndex == UINT32_MAX)
    {
        auto altRtvAlloc = heapManager->AllocateRtv();
        if (!altRtvAlloc.isValid)
        {
            throw std::runtime_error("Failed to allocate alt RTV descriptor");
        }

        m_altRTV          = altRtvAlloc.cpuHandle;
        m_altRtvHeapIndex = altRtvAlloc.heapIndex;
    }

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format                        = m_format;
    rtvDesc.ViewDimension                 = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice            = 0;
    rtvDesc.Texture2D.PlaneSlice          = 0;

    heapManager->CreateRenderTargetView(
        device,
        m_mainTexture->GetResource(),
        &rtvDesc,
        m_mainRtvHeapIndex
    );

    heapManager->CreateRenderTargetView(
        device,
        m_altTexture->GetResource(),
        &rtvDesc,
        m_altRtvHeapIndex
    );

    m_mainSRV = m_mainTexture->GetSRVHandle();
    m_altSRV  = m_altTexture->GetSRVHandle();
}

// ============================================================================
// Bindless index registration
// ============================================================================

void D12RenderTarget::CreateDescriptorInGlobalHeap(
    ID3D12Device*                device,
    GlobalDescriptorHeapManager* heapManager)
{
    if (!IsBindlessRegistered())
    {
        return;
    }

    m_mainTextureIndex = m_mainTexture->GetBindlessIndex();
    m_altTextureIndex  = m_altTexture->GetBindlessIndex();

    UNUSED(device)
    UNUSED(heapManager)
}

// ============================================================================
// Size management
// ============================================================================

void D12RenderTarget::Resize(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        throw std::invalid_argument("Width and height must be greater than zero");
    }

    const bool wasMainTextureRegistered = m_mainTexture && m_mainTexture->IsBindlessRegistered();
    const bool wasAltTextureRegistered  = m_altTexture && m_altTexture->IsBindlessRegistered();

    if (wasMainTextureRegistered)
    {
        m_mainTexture->UnregisterBindless();
    }
    if (wasAltTextureRegistered)
    {
        m_altTexture->UnregisterBindless();
    }

    m_width  = width;
    m_height = height;
    m_mainTextureIndex = INVALID_BINDLESS_INDEX;
    m_altTextureIndex  = INVALID_BINDLESS_INDEX;
    m_formattedDebugName.clear();
    m_isUploaded = false;

    // Recreate textures and descriptors.
    InitializeTextures(width, height);
    CreateDescriptors();

    if (!Upload())
    {
        throw std::runtime_error("Failed to upload resized render target resources");
    }

    // Keep shader-visible indices valid after the underlying texture resources change.
    if (wasMainTextureRegistered || wasAltTextureRegistered)
    {
        if (!RegisterBindless().has_value())
        {
            throw std::runtime_error("Failed to register resized render target bindless descriptors");
        }
    }
}

bool D12RenderTarget::ResizeIfNeeded(int width, int height)
{
    if (m_width == width && m_height == height)
    {
        return false;
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
// 复合资源管理 - Upload和RegisterBindless重写 (Milestone 3.0 Bug Fix)
// ============================================================================

/**
 * @brief Propagate the upload lifecycle to the owned textures.
 */
bool D12RenderTarget::Upload(ID3D12GraphicsCommandList* commandList)
{
    UNUSED(commandList)

    // Render targets own two texture resources that must both satisfy the
    // upload lifecycle before the composite wrapper is considered ready.
    bool success = true;

    if (m_mainTexture)
    {
        success &= m_mainTexture->Upload(commandList);
    }

    if (m_altTexture)
    {
        success &= m_altTexture->Upload(commandList);
    }

    if (success)
    {
        m_isUploaded = true;
    }

    return success;
}

/**
 * @brief 重写RegisterBindless()方法 - 复合资源模式
 *
 * 教学要点:
 * 1. Composite模式: 遍历子资源，注册每个子资源到Bindless系统
 * 2. RenderTarget自身不需要Bindless索引，只有内部纹理需要索引
 * 3. 存储子资源索引: m_mainTextureIndex和m_altTextureIndex供着色器使用
 * 4. 返回主纹理索引: 与基类签名一致，允许外部代码获取索引
 *
 * Bindless架构:
 * - 主纹理和替代纹理各自注册到全局ResourceDescriptorHeap
 * - HLSL通过索引访问: ResourceDescriptorHeap[mainTextureIndex]
 * - 支持Ping-Pong双缓冲: Main/Alt交替读写
 */
std::optional<uint32_t> D12RenderTarget::RegisterBindless()
{
    // Composite Resource Pattern: Register all sub-resources

    // 1. 注册主纹理到Bindless系统
    if (m_mainTexture)
    {
        auto mainIndex = m_mainTexture->RegisterBindless();
        if (mainIndex.has_value())
        {
            m_mainTextureIndex = mainIndex.value();
        }
        else
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "RegisterBindless: Failed to register main texture for '%s'",
                           m_debugName.c_str());
            return std::nullopt;
        }
    }

    // 2. 注册替代纹理到Bindless系统
    if (m_altTexture)
    {
        auto altIndex = m_altTexture->RegisterBindless();
        if (altIndex.has_value())
        {
            m_altTextureIndex = altIndex.value();
        }
        else
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "RegisterBindless: Failed to register alt texture for '%s'",
                           m_debugName.c_str());
            return std::nullopt;
        }
    }
    return m_mainTextureIndex;
}

bool D12RenderTarget::IsValid() const
{
    return m_isValid && m_mainTexture && m_altTexture;
}

std::shared_ptr<D12Texture> D12RenderTarget::GetMainTexture() const
{
    if (!IsValid())
    {
        throw std::runtime_error("Attempted to use an invalid D12RenderTarget");
    }
    return m_mainTexture;
}

std::shared_ptr<D12Texture> D12RenderTarget::GetAltTexture() const
{
    if (!IsValid())
    {
        throw std::runtime_error("Attempted to use an invalid D12RenderTarget");
    }
    return m_altTexture;
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

    // Milestone 3.0 Bug Fix 最终方案:
    //
    // UploadToGPU()不再负责标记内部纹理的m_isUploaded
    // 这个职责已移至Upload()重写方法中
    //
    // 教学要点:
    // 1. RenderTarget是复合资源，不需要实际的数据上传
    // 2. 复合资源的Upload()重写方法负责管理子资源上传
    // 3. UploadToGPU()仅用于Template Method模式的接口完整性
    // 4. 真正的上传逻辑在Upload()重写中，调用子资源的Upload()

    // RenderTarget/DepthStencil纹理不需要实际上传（GPU直接写入）
    // Upload()重写方法已正确处理子资源上传和标记设置
    return true;
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

// ============================================================================
// DirectX 12资源访问 (底层资源指针获取)
// ============================================================================

ID3D12Resource* D12RenderTarget::GetMainTextureResource() const
{
    if (m_mainTexture)
    {
        return m_mainTexture->GetResource();
    }
    return nullptr;
}

ID3D12Resource* D12RenderTarget::GetAltTextureResource() const
{
    if (m_altTexture)
    {
        return m_altTexture->GetResource();
    }
    return nullptr;
}

// ============================================================================
// Mipmap Generation Convenience API
// ============================================================================

bool D12RenderTarget::GenerateMainMips(const MipmapConfig& config)
{
    if (!m_enableMipmap || !m_mainTexture)
    {
        return false;
    }
    return m_mainTexture->GenerateMips(config);
}

bool D12RenderTarget::GenerateAltMips(const MipmapConfig& config)
{
    if (!m_enableMipmap || !m_altTexture)
    {
        return false;
    }
    return m_altTexture->GenerateMips(config);
}

// ============================================================================
// Resource State Transition API (Component 6: Shader RT Fetching)
// ============================================================================

void D12RenderTarget::TransitionToShaderResource()
{
    // Transition main texture to PIXEL_SHADER_RESOURCE for sampling
    // D12RenderTarget is a composite resource - transition the underlying texture
    if (m_mainTexture)
    {
        m_mainTexture->TransitionResourceTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
}

void D12RenderTarget::TransitionToRenderTarget()
{
    // Transition main texture back to RENDER_TARGET for rendering output
    if (m_mainTexture)
    {
        m_mainTexture->TransitionResourceTo(D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
}
