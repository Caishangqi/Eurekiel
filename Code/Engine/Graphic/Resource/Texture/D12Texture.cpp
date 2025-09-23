#include "D12Texture.hpp"
#include <cassert>
#include <algorithm>
#include <sstream>

#include "../../Core/DX12/D3D12RenderSystem.hpp"
#include "../BindlessResourceManager.hpp"
#include "Engine/Core/EngineCommon.hpp"

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
          , m_bindlessIndex(UINT32_MAX)
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
          , m_bindlessIndex(other.m_bindlessIndex)
    {
        // 清空源对象
        other.m_srvHandle     = {0};
        other.m_uavHandle     = {0};
        other.m_hasSRV        = false;
        other.m_hasUAV        = false;
        other.m_bindlessIndex = UINT32_MAX;
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
            m_textureType   = other.m_textureType;
            m_width         = other.m_width;
            m_height        = other.m_height;
            m_depth         = other.m_depth;
            m_mipLevels     = other.m_mipLevels;
            m_arraySize     = other.m_arraySize;
            m_format        = other.m_format;
            m_usage         = other.m_usage;
            m_srvHandle     = other.m_srvHandle;
            m_uavHandle     = other.m_uavHandle;
            m_hasSRV        = other.m_hasSRV;
            m_hasUAV        = other.m_hasUAV;
            m_bindlessIndex = other.m_bindlessIndex;

            // 清空源对象
            other.m_srvHandle     = {0};
            other.m_uavHandle     = {0};
            other.m_hasSRV        = false;
            other.m_hasUAV        = false;
            other.m_bindlessIndex = UINT32_MAX;
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

    // ==================== Bindless支持接口 ====================

    /**
     * 注册到Bindless资源管理器
     */
    uint32_t D12Texture::RegisterToBindlessManager(BindlessResourceManager* manager)
    {
        if (!manager || !m_hasSRV)
        {
            return UINT32_MAX;
        }

        // 注册为2D纹理资源
        // TODO: 根据纹理类型注册不同的资源类型
        std::shared_ptr<D12Texture> sharedThis(this, [](D12Texture*)
        {
        }); // 不删除，仅用于共享
        m_bindlessIndex = manager->RegisterTexture2D(sharedThis, GetDebugName());

        return m_bindlessIndex;
    }

    /**
     * 从Bindless资源管理器注销
     */
    bool D12Texture::UnregisterFromBindlessManager(BindlessResourceManager* manager)
    {
        if (!manager || m_bindlessIndex == UINT32_MAX)
        {
            return false;
        }

        bool result = manager->UnregisterResource(m_bindlessIndex);
        if (result)
        {
            m_bindlessIndex = UINT32_MAX;
        }

        return result;
    }

    // ==================== 纹理操作接口 ====================

    /**
     * 更新纹理数据
     */
    bool D12Texture::UpdateTextureData(
        ID3D12GraphicsCommandList* cmdList,
        const void*                data,
        size_t                     dataSize,
        uint32_t                   mipLevel,
        uint32_t                   arraySlice
    )
    {
        UNUSED(dataSize)
        UNUSED(mipLevel)
        UNUSED(arraySlice)
        if (!cmdList || !data || !IsValid())
        {
            return false;
        }

        // TODO: 实现纹理数据更新逻辑,使用CommandListManager的ID3D12GraphicsCommandList并且清除参数。
        // 需要创建上传缓冲区并使用CopyTextureRegion
        return true;
    }

    /**
     * 生成Mip贴图
     */
    bool D12Texture::GenerateMips(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList || !IsValid() || m_mipLevels <= 1)
        {
            return false;
        }

        // TODO: 实现Mip生成逻辑,使用CommandListManager的ID3D12GraphicsCommandList并且清除参数。
        // 通过Compute Shader实现
        return true;
    }

    // ==================== 调试支持 ====================

    /**
     * 获取调试信息
     */
    std::string D12Texture::GetDebugInfo() const
    {
        std::ostringstream oss;
        oss << "D12Texture[" << GetDebugName() << "] ";
        oss << "Type: " << static_cast<int>(m_textureType) << ", ";
        oss << "Size: " << m_width << "x" << m_height;
        if (m_depth > 1) oss << "x" << m_depth;
        oss << ", Format: " << m_format;
        oss << ", Mips: " << m_mipLevels;
        oss << ", Array: " << m_arraySize;
        oss << ", SRV: " << (m_hasSRV ? "Yes" : "No");
        oss << ", UAV: " << (m_hasUAV ? "Yes" : "No");
        oss << ", Bindless: " << (m_bindlessIndex != UINT32_MAX ? std::to_string(m_bindlessIndex) : "None");
        return oss.str();
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

        // 3. 确定初始资源状态
        D3D12_RESOURCE_STATES initialState = GetInitialState(m_usage);

        // 4. 创建提交资源
        // 使用D3D12RenderSystem统一接口创建资源
        // 符合分层架构原则：资源层通过API封装层访问DirectX
        ID3D12Resource* resource = nullptr;
        HRESULT         hr       = D3D12RenderSystem::CreateCommittedResource(
            heapProps, // 堆属性
            resourceDesc, // 资源描述
            initialState, // 初始状态
            &resource // 输出接口
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

        return flags;
    }

    /**
     * 获取初始资源状态
     */
    D3D12_RESOURCE_STATES D12Texture::GetInitialState(TextureUsage usage)
    {
        // 根据主要用途确定初始状态
        if (HasFlag(usage, TextureUsage::UnorderedAccess))
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
} // namespace enigma::graphic
