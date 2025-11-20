#include "D12Texture.hpp"
#include <cassert>
#include <algorithm>
#include <sstream>

#include "../../Core/DX12/D3D12RenderSystem.hpp"
#include "../BindlessIndexAllocator.hpp"
#include "../GlobalDescriptorHeapManager.hpp"
#include "../UploadContext.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

namespace enigma::graphic
{
    // ===== D12Texture类实现 =====

    /**
     * 构造函数实现
     * 参考Iris的GlTexture构造函数逻辑
     */
    D12Texture::D12Texture(const TextureCreateInfo& createInfo)
        : D12Resource() // 调用基类构造函数
          , m_textureType(createInfo.type)
          , m_width(createInfo.width)
          , m_height(createInfo.height)
          , m_depth(createInfo.depth)
          , m_mipLevels(createInfo.mipLevels)
          , m_arraySize(createInfo.arraySize)
          , m_format(createInfo.format)
          , m_usage(createInfo.usage)
          , m_srvHandle{0}
          , m_uavHandle{0}
          , m_hasSRV(false)
          , m_hasUAV(false)
          , m_formattedDebugName() // 初始化格式化调试名称
    {
        // 教学注释：验证输入参数的有效性
        assert(createInfo.width > 0 && "Texture width must be greater than 0");
        assert(createInfo.height > 0 && "Texture height must be greater than 0");
        assert(createInfo.depth > 0 && "Texture depth must be greater than 0");

        // 创建DirectX 12纹理资源
        if (!CreateD3D12Resource(createInfo))
        {
            // 创建失败，保持无效状态
            return;
        }

        // 创建描述符视图
        if (!CreateShaderResourceView())
        {
            return;
        }

        // 如果支持UAV，创建UAV视图
        if (HasFlag(m_usage, TextureUsage::UnorderedAccess))
        {
            CreateUnorderedAccessView();
        }

        // 设置调试名称（使用基类方法）
        if (createInfo.debugName)
        {
            SetDebugName(std::string(createInfo.debugName));
        }

        // 复制初始数据到CPU端（如果提供了）
        if (createInfo.initialData && createInfo.dataSize > 0)
        {
            SetInitialData(createInfo.initialData, createInfo.dataSize);
        }

        // 资源创建成功后，基类的SetResource方法已经设置了有效状态
        // 不需要手动设置m_isValid
    }

    /**
     * 析构函数实现
     * 对应Iris的GlTexture.destroy()方法
     */
    D12Texture::~D12Texture()
    {
        // ComPtr会自动释放DirectX 12资源
        // 描述符会在描述符堆销毁时自动释放
    }

    /**
     * 移动构造函数
     */
    D12Texture::D12Texture(D12Texture&& other) noexcept
        : D12Resource(std::move(other)) // 调用基类移动构造函数
          , m_textureType(other.m_textureType)
          , m_width(other.m_width)
          , m_height(other.m_height)
          , m_depth(other.m_depth)
          , m_mipLevels(other.m_mipLevels)
          , m_arraySize(other.m_arraySize)
          , m_format(other.m_format)
          , m_usage(other.m_usage)
          , m_srvHandle(other.m_srvHandle)
          , m_uavHandle(other.m_uavHandle)
          , m_hasSRV(other.m_hasSRV)
          , m_hasUAV(other.m_hasUAV)
          , m_formattedDebugName(std::move(other.m_formattedDebugName))
    {
        // 清空源对象
        other.m_srvHandle = {0};
        other.m_uavHandle = {0};
        other.m_hasSRV    = false;
        other.m_hasUAV    = false;
        // 基类移动构造函数会处理资源和有效状态的转移
    }

    /**
     * 移动赋值操作符
     */
    D12Texture& D12Texture::operator=(D12Texture&& other) noexcept
    {
        if (this != &other)
        {
            // 调用基类移动赋值
            D12Resource::operator=(std::move(other));

            // 移动D12Texture特有成员
            m_textureType        = other.m_textureType;
            m_width              = other.m_width;
            m_height             = other.m_height;
            m_depth              = other.m_depth;
            m_mipLevels          = other.m_mipLevels;
            m_arraySize          = other.m_arraySize;
            m_format             = other.m_format;
            m_usage              = other.m_usage;
            m_srvHandle          = other.m_srvHandle;
            m_uavHandle          = other.m_uavHandle;
            m_hasSRV             = other.m_hasSRV;
            m_hasUAV             = other.m_hasUAV;
            m_formattedDebugName = std::move(other.m_formattedDebugName);

            // 清空源对象
            other.m_srvHandle = {0};
            other.m_uavHandle = {0};
            other.m_hasSRV    = false;
            other.m_hasUAV    = false;
            // 基类移动赋值运算符会处理资源和有效状态的转移
        }
        return *this;
    }

    // ==================== 描述符访问接口 ====================

    /**
     * 获取着色器资源视图句柄
     */
    D3D12_CPU_DESCRIPTOR_HANDLE D12Texture::GetSRVHandle() const
    {
        assert(m_hasSRV && "Texture does not have a shader resource view");
        return m_srvHandle;
    }

    /**
     * 获取无序访问视图句柄
     */
    D3D12_CPU_DESCRIPTOR_HANDLE D12Texture::GetUAVHandle() const
    {
        assert(m_hasUAV && "Texture does not have an unordered access view");
        return m_uavHandle;
    }

    // ==================== Bindless支持说明 (Milestone 2.3更新) ====================
    //
    // Bindless注册功能已移至D12Resource基类，此处删除冗余实现
    // 统一的RegisterToBindlessManager/UnregisterFromBindlessManager方法
    // 现在由基类提供，确保所有资源类型的一致性
    //
    // ==================== 资源索引访问接口 ====================

    // ==================== 纹理操作接口 ====================

    /**
     * 更新纹理数据
     */
    bool D12Texture::UpdateTextureData(
        const void* data,
        size_t      dataSize,
        uint32_t    mipLevel,
        uint32_t    arraySlice
    )
    {
        UNUSED(dataSize)
        UNUSED(mipLevel)
        UNUSED(arraySlice)
        auto graphicCommandQueue = D3D12RenderSystem::GetCommandListManager()->GetCommandQueue(CommandListManager::Type::Graphics);
        if (!graphicCommandQueue || !data || !IsValid())
        {
            return false;
        }
        // 需要创建上传缓冲区并使用CopyTextureRegion
        return true;
    }

    /**
     * 生成Mip贴图
     */
    bool D12Texture::GenerateMips()
    {
        auto graphicCommandQueue = D3D12RenderSystem::GetCommandListManager()->GetCommandQueue(CommandListManager::Type::Graphics);
        if (!graphicCommandQueue || !IsValid() || m_mipLevels <= 1)
        {
            return false;
        }
        // 通过Compute Shader实现
        return true;
    }

    // ==================== 调试支持 ====================

    /**
     * @brief 重写虚函数：设置调试名称并添加纹理特定逻辑
     *
     * 教学要点: 重写基类虚函数，在设置纹理名称时同时更新SRV和UAV的调试名称
     * 这样PIX调试时可以看到更详细的纹理视图信息
     */
    void D12Texture::SetDebugName(const std::string& name)
    {
        // 调用基类实现设置基本调试名称
        D12Resource::SetDebugName(name);

        // 清空格式化名称，让GetDebugName()重新生成
        m_formattedDebugName.clear();

        // TODO: 设置SRV和UAV描述符的调试名称
        // 这需要等待描述符管理器实现后添加
        // if (m_hasSRV) { /* 设置SRV调试名称 */ }
        // if (m_hasUAV) { /* 设置UAV调试名称 */ }
    }

    /**
     * @brief 重写虚函数：获取包含纹理尺寸信息的调试名称
     *
     * 教学要点: 重写基类虚函数，返回格式化的纹理调试信息
     * 格式: "TextureName (2048x1024, RGBA8, Mip:4)"
     */
    const std::string& D12Texture::GetDebugName() const
    {
        // 如果格式化名称为空，重新生成
        if (m_formattedDebugName.empty())
        {
            const std::string& baseName = D12Resource::GetDebugName();
            if (baseName.empty())
            {
                m_formattedDebugName = "[Unnamed Texture]";
            }
            else
            {
                m_formattedDebugName = baseName;
            }

            // 添加纹理尺寸信息
            m_formattedDebugName += " (";
            m_formattedDebugName += std::to_string(m_width) + "x" + std::to_string(m_height);
            if (m_depth > 1)
            {
                m_formattedDebugName += "x" + std::to_string(m_depth);
            }

            // 添加格式信息 (简化)
            switch (m_format)
            {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
                m_formattedDebugName += ", RGBA8";
                break;
            case DXGI_FORMAT_R16G16B16A16_FLOAT:
                m_formattedDebugName += ", RGBA16F";
                break;
            case DXGI_FORMAT_R32G32B32A32_FLOAT:
                m_formattedDebugName += ", RGBA32F";
                break;
            case DXGI_FORMAT_D24_UNORM_S8_UINT:
                m_formattedDebugName += ", D24S8";
                break;
            case DXGI_FORMAT_D32_FLOAT:
                m_formattedDebugName += ", D32F";
                break;
            default:
                m_formattedDebugName += ", Format:" + std::to_string(static_cast<int>(m_format));
                break;
            }

            // 添加Mip层级信息
            if (m_mipLevels > 1)
            {
                m_formattedDebugName += ", Mip:" + std::to_string(m_mipLevels);
            }

            // 添加数组信息
            if (m_arraySize > 1)
            {
                m_formattedDebugName += ", Array:" + std::to_string(m_arraySize);
            }

            m_formattedDebugName += ")";
        }

        return m_formattedDebugName;
    }

    /**
     * @brief 重写虚函数：获取纹理的详细调试信息
     *
     * 教学要点: 提供纹理特定的详细调试信息
     * 包括尺寸、格式、Mip层级、使用标志、Bindless索引等信息
     */
    std::string D12Texture::GetDebugInfo() const
    {
        std::string info = "D12Texture Debug Info:\n";
        info             += "  Name: " + GetDebugName() + "\n";
        info             += "  Size: " + std::to_string(m_width) + "x" + std::to_string(m_height);
        if (m_depth > 1)
            info += "x" + std::to_string(m_depth);
        info += "\n";
        info += "  Format: " + std::to_string(static_cast<int>(m_format)) + "\n";
        info += "  Mip Levels: " + std::to_string(m_mipLevels) + "\n";
        info += "  Array Size: " + std::to_string(m_arraySize) + "\n";
        info += "  GPU Address: 0x" + std::to_string(GetGPUVirtualAddress()) + "\n";

        // 纹理类型
        info += "  Type: ";
        switch (m_textureType)
        {
        case TextureType::Texture1D:
            info += "1D Texture\n";
            break;
        case TextureType::Texture2D:
            info += "2D Texture\n";
            break;
        case TextureType::Texture3D:
            info += "3D Texture\n";
            break;
        case TextureType::TextureCube:
            info += "Cube Texture\n";
            break;
        case TextureType::Texture1DArray:
            info += "1D Texture Array\n";
            break;
        case TextureType::Texture2DArray:
            info += "2D Texture Array\n";
            break;
        default:
            info += "Unknown\n";
            break;
        }

        // 使用标志
        info += "  Usage: ";
        if (HasFlag(m_usage, TextureUsage::ShaderResource))
            info += "SRV ";
        if (HasFlag(m_usage, TextureUsage::UnorderedAccess))
            info += "UAV ";
        if (HasFlag(m_usage, TextureUsage::CopySource))
            info += "CopySrc ";
        if (HasFlag(m_usage, TextureUsage::CopyDestination))
            info += "CopyDst ";
        info += "\n";

        // 视图状态
        info += "  Has SRV: " + std::string(m_hasSRV ? "Yes" : "No") + "\n";
        info += "  Has UAV: " + std::string(m_hasUAV ? "Yes" : "No") + "\n";

        // Bindless状态
        info += "  Bindless Index: ";

        if (!IsBindlessRegistered())
        {
            info += "Not Registered\n";
        }
        else
        {
            info += std::to_string(GetBindlessIndex()) + "\n";
        }

        // 资源状态
        info += "  Current State: " + std::to_string(static_cast<int>(GetCurrentState())) + "\n";
        info += "  Valid: " + std::string(IsValid() ? "Yes" : "No");

        return info;
    }

    // ==================== 静态辅助方法 ====================

    /**
     * 计算纹理大小
     */
    size_t D12Texture::CalculateTextureSize(uint32_t width, uint32_t height, DXGI_FORMAT format)
    {
        uint32_t bytesPerPixel = GetFormatBytesPerPixel(format);
        return static_cast<size_t>(width) * height * bytesPerPixel;
    }

    /**
     * 获取格式的字节数
     */
    uint32_t D12Texture::GetFormatBytesPerPixel(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SINT:
            return 1;

        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_R16_FLOAT:
            return 2;

        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
            return 4;

        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G32_FLOAT:
            return 8;

        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
        case DXGI_FORMAT_R32G32B32_FLOAT:
            return 12;

        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return 16;

        default:
            return 4; // 默认假设4字节
        }
    }

    /**
     * 检查格式是否支持UAV
     */
    bool D12Texture::IsUAVCompatibleFormat(DXGI_FORMAT format)
    {
        // 常见的UAV兼容格式
        switch (format)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
            return true;
        default:
            return false;
        }
    }

    // ==================== 内部辅助方法 ====================

    /**
     * 创建DirectX 12纹理资源的核心实现
     * 对应Iris的IrisRenderSystem.createTexture()逻辑
     */
    bool D12Texture::CreateD3D12Resource(const TextureCreateInfo& createInfo)
    {
        // 1. 设置堆属性（纹理通常使用DEFAULT堆）
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type                  = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask      = 1;
        heapProps.VisibleNodeMask       = 1;

        // 2. 创建资源描述符
        D3D12_RESOURCE_DESC resourceDesc = GetResourceDesc(createInfo);

        // 3. Determine the initial resource status
        D3D12_RESOURCE_STATES initialState = GetInitialState(m_usage);

        // 4. Construct OptimizedClearValue (RenderTarget Fast Clear optimization)
        //Teaching points:
        // - RenderTarget texture requires OptimizedClearValue to enable Fast Clear optimization
        // - Clear value (0,0,0,1) must match Rgba8::BLACK in RenderTargetBinder::PerformClearOperations()
        // - If the clear values do not match, DirectX 12 cannot use Fast Clear and performance will decrease
        // - non-RenderTarget textures pass nullptr (no clear optimization required)
        D3D12_CLEAR_VALUE        optimizedClearValue = {};
        const D3D12_CLEAR_VALUE* pClearValue         = nullptr;

        if (HasFlag(m_usage, TextureUsage::RenderTarget))
        {
            // Use clearValue from createInfo for OptimizedClearValue
            optimizedClearValue.Format = m_format;
            float clearColor[4];
            createInfo.clearValue.GetColorAsFloats(clearColor);
            optimizedClearValue.Color[0] = clearColor[0];
            optimizedClearValue.Color[1] = clearColor[1];
            optimizedClearValue.Color[2] = clearColor[2];
            optimizedClearValue.Color[3] = clearColor[3];
            pClearValue                  = &optimizedClearValue;
        }

        // 5. Create submission resources
        // Create resources using the D3D12RenderSystem unified interface
        // Comply with the principle of layered architecture: the resource layer accesses DirectX through the API encapsulation layer
        ID3D12Resource* resource = nullptr;
        HRESULT         hr       = D3D12RenderSystem::CreateCommittedResource(
            heapProps, // Heap properties
            resourceDesc, // resource description
            initialState, // initial state
            pClearValue, // OptimizedClearValue (RenderTarget enables Fast Clear)
            &resource // Output interface
        );

        if (FAILED(hr))
        {
            assert(false && "Failed to create D3D12 texture resource");
            return false;
        }

        // 计算纹理大小
        size_t textureSize = CalculateTextureSize(m_width, m_height, m_format);
        if (m_textureType == TextureType::Texture3D)
        {
            textureSize *= m_depth;
        }
        textureSize *= m_arraySize * m_mipLevels;

        // 使用基类方法设置资源
        SetResource(resource, initialState, textureSize);

        return true;
    }

    /**
     * 创建着色器资源视图
     */
    bool D12Texture::CreateShaderResourceView()
    {
        if (!HasFlag(m_usage, TextureUsage::ShaderResource))
        {
            return true; // 不需要SRV
        }

        // TODO: 实现SRV创建
        // 需要从D3D12RenderSystem获取描述符堆并创建SRV
        m_hasSRV = true;
        return true;
    }

    /**
     * 创建无序访问视图
     */
    bool D12Texture::CreateUnorderedAccessView()
    {
        if (!HasFlag(m_usage, TextureUsage::UnorderedAccess))
        {
            return true; // 不需要UAV
        }

        if (!IsUAVCompatibleFormat(m_format))
        {
            return false; // 格式不支持UAV
        }

        // TODO: 实现UAV创建
        // 需要从D3D12RenderSystem获取描述符堆并创建UAV
        m_hasUAV = true;
        return true;
    }

    /**
     * 获取资源描述
     */
    D3D12_RESOURCE_DESC D12Texture::GetResourceDesc(const TextureCreateInfo& createInfo)
    {
        D3D12_RESOURCE_DESC desc = {};

        // 设置维度
        switch (createInfo.type)
        {
        case TextureType::Texture1D:
        case TextureType::Texture1DArray:
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
            break;
        case TextureType::Texture2D:
        case TextureType::Texture2DArray:
        case TextureType::TextureCube:
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            break;
        case TextureType::Texture3D:
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
            break;
        }

        desc.Alignment          = 0; // 让DirectX自动选择对齐
        desc.Width              = createInfo.width;
        desc.Height             = createInfo.height;
        desc.DepthOrArraySize   = static_cast<UINT16>((createInfo.type == TextureType::Texture3D) ? createInfo.depth : createInfo.arraySize);
        desc.MipLevels          = static_cast<UINT16>(createInfo.mipLevels);
        desc.Format             = createInfo.format;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags              = GetResourceFlags(createInfo.usage);

        return desc;
    }

    /**
     * 获取资源标志
     */
    D3D12_RESOURCE_FLAGS D12Texture::GetResourceFlags(TextureUsage usage)
    {
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

        // 如果包含无序访问用途，添加UAV标志
        if (HasFlag(usage, TextureUsage::UnorderedAccess))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        // Milestone 3.0修复：添加RenderTarget标志处理
        if (HasFlag(usage, TextureUsage::RenderTarget))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }

        // Milestone 3.0修复：添加DepthStencil标志处理
        if (HasFlag(usage, TextureUsage::DepthStencil))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        return flags;
    }

    /**
     * 获取初始资源状态
     */
    D3D12_RESOURCE_STATES D12Texture::GetInitialState(TextureUsage usage)
    {
        // [FIX] Bug #538/#527 - 根据主要用途确定初始状态
        // 教学要点:
        // - RenderTarget和DepthStencil必须优先检查
        // - 初始状态必须匹配首次使用场景，避免不必要的状态转换
        // - 检查顺序很重要：从最具体到最通用

        if (HasFlag(usage, TextureUsage::RenderTarget))
        {
            // RenderTarget的首次使用通常是OMSetRenderTargets
            return D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
        else if (HasFlag(usage, TextureUsage::DepthStencil))
        {
            // DepthStencil的首次使用通常是深度写入
            return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }
        else if (HasFlag(usage, TextureUsage::UnorderedAccess))
        {
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        else if (HasFlag(usage, TextureUsage::CopyDestination))
        {
            return D3D12_RESOURCE_STATE_COPY_DEST;
        }
        else if (HasFlag(usage, TextureUsage::CopySource))
        {
            return D3D12_RESOURCE_STATE_COPY_SOURCE;
        }
        else
        {
            // 默认为ShaderResource状态
            return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
    }

    // ==================== 静态辅助函数 ====================

    /**
     * 检查是否为深度格式
     */
    static bool IsDepthFormat(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return true;
        default:
            return false;
        }
    }

    /**
     * @brief 分配Bindless纹理索引 (Template Method模式 - 子类实现)
     *
     * 教学要点:
     * 1. Template Method模式: 基类管理流程,子类决定索引类型
     * 2. 纹理调用AllocateTextureIndex()获取纹理专用索引(0-999999)
     * 3. FreeList架构: O(1)分配,无需遍历
     */
    uint32_t D12Texture::AllocateBindlessIndexInternal(BindlessIndexAllocator* allocator) const
    {
        if (!allocator)
        {
            return BindlessIndexAllocator::INVALID_INDEX;
        }

        // 调用纹理索引分配器
        return allocator->AllocateTextureIndex();
    }

    /**
     * @brief 释放Bindless纹理索引 (Template Method模式 - 子类实现)
     *
     * 教学要点:
     * 1. Template Method模式: 子类实现具体释放逻辑
     * 2. 纹理调用FreeTextureIndex()释放纹理索引
     * 3. 索引范围验证: 0-999999
     */
    bool D12Texture::FreeBindlessIndexInternal(BindlessIndexAllocator* allocator, uint32_t index) const
    {
        if (!allocator)
        {
            return false;
        }

        // 调用纹理索引释放器
        return allocator->FreeTextureIndex(index);
    }

    /**
     * @brief 在全局描述符堆中创建纹理描述符 (SM6.6 Bindless架构)
     *
     * 教学要点:
     * 1. SM6.6 Bindless架构要求在全局描述符堆的指定索引位置创建SRV
     * 2. Bindless索引由BindlessIndexAllocator分配,已存储在m_bindlessIndex中
     * 3. GlobalDescriptorHeapManager负责在指定索引位置创建描述符
     * 4. 此方法只在RegisterToBindlessManager()后调用
     */
    void D12Texture::CreateDescriptorInGlobalHeap(
        ID3D12Device*                device,
        GlobalDescriptorHeapManager* heapManager)
    {
        // 验证参数和状态
        if (!device || !heapManager || !IsValid() || !IsBindlessRegistered())
        {
            core::LogError("D12Texture",
                           "CreateDescriptorInGlobalHeap: Invalid parameters or resource not registered");
            return;
        }

        // 获取纹理资源
        auto* resource = GetResource();
        if (!resource)
        {
            core::LogError("D12Texture", "CreateDescriptorInGlobalHeap: Resource is null");
            return;
        }

        // 1. 创建SRV描述符结构
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                          = m_format;
        srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        // 2. 根据纹理类型设置视图维度
        switch (m_textureType)
        {
        case TextureType::Texture1D:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            srvDesc.Texture1D.MipLevels           = m_mipLevels;
            srvDesc.Texture1D.MostDetailedMip     = 0;
            srvDesc.Texture1D.ResourceMinLODClamp = 0.0f;
            break;

        case TextureType::Texture2D:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels           = m_mipLevels;
            srvDesc.Texture2D.MostDetailedMip     = 0;
            srvDesc.Texture2D.PlaneSlice          = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
            break;

        case TextureType::Texture3D:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Texture3D.MipLevels           = m_mipLevels;
            srvDesc.Texture3D.MostDetailedMip     = 0;
            srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
            break;

        case TextureType::TextureCube:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MipLevels           = m_mipLevels;
            srvDesc.TextureCube.MostDetailedMip     = 0;
            srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
            break;

        case TextureType::Texture1DArray:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            srvDesc.Texture1DArray.MipLevels           = m_mipLevels;
            srvDesc.Texture1DArray.MostDetailedMip     = 0;
            srvDesc.Texture1DArray.FirstArraySlice     = 0;
            srvDesc.Texture1DArray.ArraySize           = m_arraySize;
            srvDesc.Texture1DArray.ResourceMinLODClamp = 0.0f;
            break;

        case TextureType::Texture2DArray:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MipLevels           = m_mipLevels;
            srvDesc.Texture2DArray.MostDetailedMip     = 0;
            srvDesc.Texture2DArray.FirstArraySlice     = 0;
            srvDesc.Texture2DArray.ArraySize           = m_arraySize;
            srvDesc.Texture2DArray.PlaneSlice          = 0;
            srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
            break;

        default:
            core::LogError("D12Texture", "CreateDescriptorInGlobalHeap: Unknown texture type");
            return;
        }

        // 3. 在全局描述符堆的Bindless索引位置创建SRV
        // 使用GlobalDescriptorHeapManager的SM6.6索引创建接口
        heapManager->CreateShaderResourceView(device, resource, &srvDesc, GetBindlessIndex());

        core::LogInfo("D12Texture",
                      "CreateDescriptorInGlobalHeap: Created SRV at bindless index %u for texture '%s'", GetBindlessIndex(), GetDebugName().c_str());
    }

    // ==================== GPU资源上传(Milestone 2.7) ====================

    /**
     * @brief 实现纹理特定的上传逻辑
     * 教学要点:
     * 1. 计算纹理行间距(RowPitch)必须256字节对齐(DirectX 12要求)
     * 2. 只上传Mip 0层级(第一个最高分辨率层)
     * 3. 使用UploadContext::UploadTextureData()执行GPU复制
     * 4. 资源状态转换由基类D12Resource::Upload()处理
     */
    bool D12Texture::UploadToGPU(
        ID3D12GraphicsCommandList* commandList,
        UploadContext&             uploadContext
    )
    {
        /*
         * ============================================================================
         * Educational Note: Why RenderTarget/DepthStencil Doesn't Need Upload()
         * ============================================================================
         *
         * Key Concept: Input Texture vs Output Texture
         * ---------------------------------------------
         * - Input Texture (e.g., "texture/block/stone.png"):
         *   Loaded from disk/memory, requires CPU-to-GPU data transfer via upload heap.
         *
         * - Output Texture (RenderTarget/DepthStencil):
         *   GPU render output destination, no CPU data exists initially.
         *   GPU directly writes rendered pixels during draw calls.
         *
         * RenderTarget Lifecycle (e.g., colortex0 in Iris/Minecraft Deferred Rendering)
         * -------------------------------------------------------------------------------
         * 1. Create():
         *    Allocate empty GPU memory (e.g., 1920x1080 RGBA16F = 16.6MB).
         *    Initial state: D3D12_RESOURCE_STATE_COMMON (undefined content).
         *
         * 2. Upload() [THIS METHOD]:
         *    Does NOT transfer CPU data (because no CPU data exists).
         *    Only marks m_isUploaded = true for safety checks.
         *    Allows subsequent RegisterBindless() to proceed.
         *
         * 3. RegisterBindless():
         *    Create Shader Resource View (SRV) for shader sampling.
         *    Assign bindless index (e.g., colortex0 -> index 10).
         *
         * 4. OMSetRenderTargets():
         *    Bind as render target for GPU output (RTV - Render Target View).
         *    State transition: COMMON -> RENDER_TARGET.
         *
         * 5. Draw Calls (Geometry Pass):
         *    GPU writes rendered pixels directly to this texture.
         *    Example: Store albedo, normal, depth into colortex0-2 (GBuffer).
         *
         * 6. Later Rendering Pass (Lighting Pass):
         *    Sample this texture via SRV (bindless index).
         *    State transition: RENDER_TARGET -> PIXEL_SHADER_RESOURCE.
         *    Example: Read GBuffer to compute final lighting.
         *
         * Why Still Call Upload() for RenderTarget?
         * ------------------------------------------
         * - Architecture Consistency:
         *   All textures follow unified lifecycle: Create() -> Upload() -> RegisterBindless().
         *   This simplifies code and prevents special-case bugs.
         *
         * - Safety Check:
         *   RegisterBindless() requires m_isUploaded == true to prevent using uninitialized resources.
         *   Without calling Upload(), RegisterBindless() would fail assertion.
         *
         * - State Transition (if needed):
         *   Base class D12Resource::Upload() may convert resource state.
         *   Example: COMMON -> RENDER_TARGET (though typically done at first OMSetRenderTargets).
         *
         * Analogy for Understanding
         * -------------------------
         * - Input Texture = Import a photo into Photoshop (requires loading from disk).
         * - Output Texture = Create a new blank canvas in Photoshop (empty, ready for painting).
         *
         * Iris/Minecraft Deferred Rendering Context
         * ------------------------------------------
         * - colortex0-15 = RenderTargets storing GBuffer (Albedo, Normal, Depth, etc.).
         * - texture/block/stone.png = Input Texture loaded from resource pack.
         * - Geometry Pass: Render scene -> Write to colortex0-2 (Output).
         * - Lighting Pass: Read colortex0-2 (Input via SRV) -> Compute lighting -> Write to colortex3.
         * ============================================================================
         */
        if (HasFlag(m_usage, TextureUsage::RenderTarget) || HasFlag(m_usage, TextureUsage::DepthStencil))
        {
            // Mark as uploaded (no actual data transfer, just state marking)
            m_isUploaded = true;

            core::LogInfo(LogRenderer,
                          "Texture '%s' marked as uploaded (RenderTarget/DepthStencil, no CPU data needed)",
                          GetDebugName().empty() ? "<unnamed>" : GetDebugName().c_str());

            return true;
        }

        // 输入纹理：必须提供CPU数据
        if (!HasCPUData())
        {
            core::LogError(LogRenderer,
                           "D12Texture::UploadToGPU: No CPU data available for input texture '%s'",
                           GetDebugName().empty() ? "<unnamed>" : GetDebugName().c_str());
            return false;
        }

        // 1. 计算行间距(RowPitch) - 必须256字节对齐
        uint32_t bytesPerPixel = GetFormatBytesPerPixel(m_format);
        uint32_t rowPitch      = m_width * bytesPerPixel;
        rowPitch               = (rowPitch + 255) & ~255; // 256字节对齐

        // 2. 计算切片间距(SlicePitch)
        uint32_t slicePitch = rowPitch * m_height;

        // 3. 调用UploadContext上传纹理数据(只上传Mip 0)
        bool uploadSuccess = uploadContext.UploadTextureData(
            commandList,
            GetResource(), // GPU纹理资源
            GetCPUData(), // CPU数据指针
            GetCPUDataSize(), // 数据大小
            rowPitch, // 行间距
            slicePitch, // 切片间距
            m_width, // 宽度
            m_height, // 高度
            m_format // 格式
        );

        if (!uploadSuccess)
        {
            core::LogError(LogRenderer,
                           "D12Texture::UploadToGPU: Failed to upload texture '%s'",
                           GetDebugName().empty() ? "<unnamed>" : GetDebugName().c_str());
            return false;
        }

        core::LogDebug(LogRenderer,
                       "D12Texture::UploadToGPU: Successfully uploaded texture '%s' (%ux%u, %u bytes)",
                       GetDebugName().empty() ? "<unnamed>" : GetDebugName().c_str(),
                       m_width, m_height, GetCPUDataSize());

        return true;
    }

    /**
     * @brief 获取纹理上传后的目标资源状态
     * 教学要点:
     * 1. 纹理通常作为像素着色器资源使用(SRV)
     * 2. D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE是最常见的纹理状态
     * 3. 如果需要其他状态(如UAV),子类可继续重写此方法
     */
    D3D12_RESOURCE_STATES D12Texture::GetUploadDestinationState() const
    {
        // 纹理默认转换到像素着色器资源状态
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
} // namespace enigma::graphic
