#include "D12DepthTexture.hpp"
#include <cassert>
#include <sstream>

#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    // ===== D12DepthTexture类实现 - 对应Iris DepthTexture.java =====

    /**
     * 构造函数实现 - 对应Iris DepthTexture构造函数
     *
     * Iris参考实现:
     * ```java
     * public DepthTexture(String name, int width, int height, DepthBufferFormat format) {
     *     super(IrisRenderSystem.createTexture(GL_TEXTURE_2D));
     *     int texture = getGlId();
     *     resize(width, height, format);
     *     GLDebug.nameObject(GL43C.GL_TEXTURE, texture, name);
     *     // 设置纹理参数 (MIN_FILTER, MAG_FILTER, WRAP_S, WRAP_T)
     * }
     * ```
     */
    D12DepthTexture::D12DepthTexture(const DepthTextureCreateInfo& createInfo)
        : D12Resource() // 调用基类构造函数，对应Iris的super(IrisRenderSystem.createTexture())
          , m_dsvHandle{0}
          , m_srvHandle{0}
          , m_hasSRV(false)
          , m_name(createInfo.name)
          , m_width(createInfo.width)
          , m_height(createInfo.height)
          , m_depthFormat(GetDXGIFormat(createInfo.depthFormat))
          , m_format(createInfo.depthFormat)
          , m_clearDepth(createInfo.clearDepth)
          , m_clearStencil(createInfo.clearStencil)
          , m_supportSampling(createInfo.depthFormat == DepthFormat::D32_FLOAT) // D32_FLOAT 支持采样（对应旧 ShadowMap）
          , m_hasValidDSV(false)
          , m_hasValidSRV(false)
          , m_formattedDebugName() // 初始化格式化调试名称
    {
        // 教学注释：验证输入参数的有效性
        assert(createInfo.width > 0 && "Depth texture width must be greater than 0");
        assert(createInfo.height > 0 && "Depth texture height must be greater than 0");
        assert(!createInfo.name.empty() && "Depth texture name cannot be empty");

        // 1. 创建深度纹理资源 - 对应Iris的resize()调用
        if (!CreateDepthResource())
        {
            // 创建失败，保持无效状态
            return;
        }

        // 2. 创建深度模板视图 - DirectX 12特有，Iris中OpenGL自动处理
        if (!CreateDepthStencilView())
        {
            return;
        }

        // 3. 如果支持采样（D32_FLOAT格式），创建SRV - 对应Iris中可采样深度纹理
        if (m_supportSampling)
        {
            CreateShaderResourceView();
        }

        // 4. 设置调试名称 - 对应Iris的GLDebug.nameObject()
        SetDebugName(createInfo.name);

        // 资源创建成功后，基类的SetResource方法已经设置了有效状态
        // 不需要手动设置m_isValid
    }

    /**
     * 析构函数实现 - 对应Iris的destroyInternal()
     *
     * Iris参考实现:
     * ```java
     * protected void destroyInternal() {
     *     GlStateManager._deleteTexture(getGlId());
     * }
     * ```
     */
    D12DepthTexture::~D12DepthTexture()
    {
        // ComPtr会自动释放DirectX 12资源
        // 描述符会在描述符堆销毁时自动释放
        // 基类析构函数会处理资源释放
    }

    /**
     * 移动构造函数
     */
    D12DepthTexture::D12DepthTexture(D12DepthTexture&& other) noexcept
        : D12Resource(std::move(other)) // 调用基类移动构造函数
          , m_dsvHandle(other.m_dsvHandle)
          , m_srvHandle(other.m_srvHandle)
          , m_hasSRV(other.m_hasSRV)
          , m_name(std::move(other.m_name))
          , m_width(other.m_width)
          , m_height(other.m_height)
          , m_depthFormat(other.m_depthFormat)
          , m_format(other.m_format)
          , m_clearDepth(other.m_clearDepth)
          , m_clearStencil(other.m_clearStencil)
          , m_supportSampling(other.m_supportSampling)
          , m_hasValidDSV(other.m_hasValidDSV)
          , m_hasValidSRV(other.m_hasValidSRV)
          , m_formattedDebugName(std::move(other.m_formattedDebugName))
    {
        // 清空源对象
        other.m_dsvHandle       = {0};
        other.m_srvHandle       = {0};
        other.m_hasSRV          = false;
        other.m_width           = 0;
        other.m_height          = 0;
        other.m_clearDepth      = 1.0f;
        other.m_clearStencil    = 0;
        other.m_supportSampling = false;
        other.m_hasValidDSV     = false;
        other.m_hasValidSRV     = false;
    }

    /**
     * 移动赋值操作符
     */
    D12DepthTexture& D12DepthTexture::operator=(D12DepthTexture&& other) noexcept
    {
        if (this != &other)
        {
            // 调用基类移动赋值
            D12Resource::operator=(std::move(other));

            // 移动D12DepthTexture特有成员
            m_dsvHandle          = other.m_dsvHandle;
            m_srvHandle          = other.m_srvHandle;
            m_hasSRV             = other.m_hasSRV;
            m_name               = std::move(other.m_name);
            m_width              = other.m_width;
            m_height             = other.m_height;
            m_depthFormat        = other.m_depthFormat;
            m_format             = other.m_format;
            m_clearDepth         = other.m_clearDepth;
            m_clearStencil       = other.m_clearStencil;
            m_supportSampling    = other.m_supportSampling;
            m_hasValidDSV        = other.m_hasValidDSV;
            m_hasValidSRV        = other.m_hasValidSRV;
            m_formattedDebugName = std::move(other.m_formattedDebugName);

            // 清空源对象
            other.m_dsvHandle       = {0};
            other.m_srvHandle       = {0};
            other.m_hasSRV          = false;
            other.m_width           = 0;
            other.m_height          = 0;
            other.m_clearDepth      = 1.0f;
            other.m_clearStencil    = 0;
            other.m_supportSampling = false;
            other.m_hasValidDSV     = false;
            other.m_hasValidSRV     = false;
        }
        return *this;
    }

    /**
     * @brief 在全局描述符堆中创建深度纹理的SRV描述符
     *
     * 教学要点:
     * 1. 深度纹理需要创建SRV用于着色器采样（延迟渲染中读取深度值）
     * 2. 使用GetTypedFormat()获取深度格式对应的类型化格式
     * 3. 深度纹理的SRV允许在像素着色器中读取深度信息
     * 4. DSV（深度模板视图）由单独的DSV堆管理,不在全局堆中
     *
     * DirectX 12 API参考:
     * - ID3D12Device::CreateShaderResourceView()
     * - D3D12_SHADER_RESOURCE_VIEW_DESC
     */
    void D12DepthTexture::CreateDescriptorInGlobalHeap(ID3D12Device* device, class GlobalDescriptorHeapManager* heapManager)
    {
        if (!device || !heapManager)
        {
            return;
        }

        if (!IsValid())
        {
            return;
        }

        // 1. 获取深度格式对应的类型化格式（用于SRV）
        // 例如: D32_FLOAT -> R32_FLOAT, D24_UNORM_S8_UINT -> R24_UNORM_X8_TYPELESS
        DXGI_FORMAT srvFormat = GetTypedFormat(m_depthFormat);
        if (srvFormat == DXGI_FORMAT_UNKNOWN)
        {
            // 不支持的深度格式，无法创建SRV
            return;
        }

        // 2. 创建着色器资源视图描述符
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                          = srvFormat;
        srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip       = 0;
        srvDesc.Texture2D.MipLevels             = 1; // 深度纹理通常没有Mipmap
        srvDesc.Texture2D.PlaneSlice            = 0;
        srvDesc.Texture2D.ResourceMinLODClamp   = 0.0f;

        // 3. 在全局描述符堆中创建SRV
        // 使用m_bindlessIndex作为堆中的索引位置
        heapManager->CreateShaderResourceView(
            device,
            GetResource(),
            &srvDesc,
            GetBindlessIndex()
        );

        // 教学要点:
        // - 深度纹理的SRV允许在着色器中读取深度值
        // - 这对于延迟渲染、阴影贴图、后处理效果至关重要
        // - DSV用于深度测试，SRV用于着色器采样，是两个独立的视图
    }

    // ==================== 资源访问接口 ====================

    /**
     * 获取着色器资源视图句柄
     */
    D3D12_CPU_DESCRIPTOR_HANDLE D12DepthTexture::GetSRVHandle() const
    {
        assert(m_hasSRV && "Depth texture does not have a shader resource view");
        return m_srvHandle;
    }

    bool D12DepthTexture::UploadToGPU(ID3D12GraphicsCommandList* commandList, class UploadContext& uploadContext)
    {
        UNUSED(commandList)
        UNUSED(uploadContext)
        ERROR_AND_DIE("D12DepthTexture::UploadToGPU not implemented")
    }

    D3D12_RESOURCE_STATES D12DepthTexture::GetUploadDestinationState() const
    {
        ERROR_AND_DIE("D12DepthTexture::GetUploadDestinationState() not implemented")
    }

    // ==================== 深度操作接口 ====================

    /**
     * 调整深度纹理尺寸 - 对应Iris的resize方法
     *
     * Iris参考实现:
     * ```java
     * void resize(int width, int height, DepthBufferFormat format) {
     *     IrisRenderSystem.texImage2D(getTextureId(), GL_TEXTURE_2D, 0,
     *         format.getGlInternalFormat(), width, height, 0,
     *         format.getGlType(), format.getGlFormat(), null);
     * }
     * ```
     */
    bool D12DepthTexture::Resize(uint32_t newWidth, uint32_t newHeight)
    {
        if (newWidth == m_width && newHeight == m_height)
        {
            return true; // 尺寸未变化，无需重建
        }

        // DirectX 12需要重新创建资源，不像OpenGL可以直接调用texImage2D
        m_width  = newWidth;
        m_height = newHeight;

        // 重新创建深度资源
        if (!CreateDepthResource())
        {
            return false;
        }

        // 重新创建DSV
        if (!CreateDepthStencilView())
        {
            return false;
        }

        // 如果之前有SRV，重新创建
        if (m_hasSRV)
        {
            CreateShaderResourceView();
        }

        // 重新设置调试名称
        SetDebugName(m_name);

        return true;
    }

    /**
     * 清除深度缓冲区
     */
    void D12DepthTexture::Clear(
        ID3D12GraphicsCommandList* cmdList,
        float                      depthValue,
        uint8_t                    stencilValue
    )
    {
        if (!cmdList || !IsValid())
        {
            return;
        }

        // 使用默认值（如果传入了-1.0f等无效值）
        float   actualDepthValue   = (depthValue < 0.0f) ? m_clearDepth : depthValue;
        uint8_t actualStencilValue = (stencilValue == 255) ? m_clearStencil : stencilValue;

        // DirectX 12 API: ID3D12GraphicsCommandList::ClearDepthStencilView()
        D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH;
        if (m_format == DepthFormat::D24_UNORM_S8_UINT) // 只有D24格式支持模板
        {
            clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
        }

        cmdList->ClearDepthStencilView(
            m_dsvHandle, // DSV句柄
            clearFlags, // 清除标志
            actualDepthValue, // 深度清除值
            actualStencilValue, // 模板清除值
            0, // 矩形数量（0表示整个视图）
            nullptr // 矩形列表
        );
    }

    /**
     * 绑定为深度目标
     */
    void D12DepthTexture::BindAsDepthTarget(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList || !IsValid())
        {
            return;
        }

        // DirectX 12 API: ID3D12GraphicsCommandList::OMSetRenderTargets()
        // 这里只设置深度目标，不设置颜色目标
        cmdList->OMSetRenderTargets(
            0, // 颜色目标数量
            nullptr, // 颜色目标描述符
            FALSE, // 颜色目标是否连续
            &m_dsvHandle // 深度模板目标描述符
        );
    }

    // ==================== 调试支持 ====================

    /**
     * @brief 重写虚函数：设置调试名称并添加深度纹理特定逻辑
     *
     * 教学要点: 重写基类虚函数，在设置深度纹理名称时同时更新DSV和SRV的调试名称
     * 对应Iris中的GLDebug.nameObject()功能
     */
    void D12DepthTexture::SetDebugName(const std::string& name)
    {
        // 调用基类实现设置基本调试名称
        D12Resource::SetDebugName(name);

        // 清空格式化名称，让GetDebugName()重新生成
        m_formattedDebugName.clear();
    }

    /**
     * @brief 重写虚函数：获取包含深度纹理信息的调试名称
     *
     * 教学要点: 重写基类虚函数，返回格式化的深度纹理调试信息
     * 格式: "DepthTextureName (1024x768, D24S8, SampleRead:Yes)"
     */
    const std::string& D12DepthTexture::GetDebugName() const
    {
        // 如果格式化名称为空，重新生成
        if (m_formattedDebugName.empty())
        {
            const std::string& baseName = D12Resource::GetDebugName();
            if (baseName.empty())
            {
                m_formattedDebugName = "[Unnamed DepthTexture]";
            }
            else
            {
                m_formattedDebugName = baseName;
            }

            // 添加深度纹理尺寸信息
            m_formattedDebugName += " (";
            m_formattedDebugName += std::to_string(m_width) + "x" + std::to_string(m_height);

            // 添加深度格式信息
            switch (m_format)
            {
            case DepthFormat::D32_FLOAT:
                m_formattedDebugName += m_supportSampling ? ", D32F-Shadow" : ", D32F";
                break;
            case DepthFormat::D24_UNORM_S8_UINT:
                m_formattedDebugName += ", D24S8";
                break;
            case DepthFormat::D16_UNORM:
                m_formattedDebugName += ", D16";
                break;
            default:
                m_formattedDebugName += ", UnknownDepth";
                break;
            }

            // 添加采样支持信息
            m_formattedDebugName += ", SampleRead:" + std::string(m_supportSampling ? "Yes" : "No");

            m_formattedDebugName += ")";
        }

        return m_formattedDebugName;
    }

    /**
     * @brief 重写虚函数：获取深度纹理的详细调试信息
     *
     * 教学要点: 提供深度纹理特定的详细调试信息
     * 包括深度类型、格式、是否支持采样、DSV/SRV状态等
     */
    std::string D12DepthTexture::GetDebugInfo() const
    {
        std::string info = "D12DepthTexture Debug Info:\n";
        info += "  Name: " + GetDebugName() + "\n";
        info += "  Size: " + std::to_string(m_width) + "x" + std::to_string(m_height) + "\n";
        info += "  GPU Address: 0x" + std::to_string(GetGPUVirtualAddress()) + "\n";

        // 深度格式信息
        info += "  Depth Format: ";
        switch (m_format)
        {
        case DepthFormat::D32_FLOAT:
            info += m_supportSampling ? "32-bit Float Depth (D32_FLOAT) - Shadow Map Support\n" : "32-bit Float Depth (D32_FLOAT)\n";
            break;
        case DepthFormat::D24_UNORM_S8_UINT:
            info += "24-bit Depth + 8-bit Stencil (D24_UNORM_S8_UINT)\n";
            break;
        case DepthFormat::D16_UNORM:
            info += "16-bit Depth (D16_UNORM)\n";
            break;
        default:
            info += "Unknown Depth Format\n";
            break;
        }

        // 功能支持
        info += "  Support Sampling: " + std::string(m_supportSampling ? "Yes" : "No") + "\n";
        info += "  Has DSV: " + std::string(m_hasValidDSV ? "Yes" : "No") + "\n";
        info += "  Has SRV: " + std::string(m_hasValidSRV ? "Yes" : "No") + "\n";

        // 资源状态
        info += "  Current State: " + std::to_string(static_cast<int>(GetCurrentState())) + "\n";
        info += "  Valid: " + std::string(IsValid() ? "Yes" : "No");

        return info;
    }

    /**
     * @brief 分配深度纹理专属的Bindless索引
     *
     * 教学要点:
     * 1. 深度纹理索引范围: 0 - 999,999 (1M个纹理容量)
     * 2. 调用BindlessIndexAllocator::AllocateTextureIndex()
     * 3. 深度纹理作为可采样资源，使用纹理索引而非缓冲区索引
     * 4. 返回值是全局唯一的索引,用于在全局描述符堆中定位
     */
    uint32_t D12DepthTexture::AllocateBindlessIndexInternal(BindlessIndexAllocator* allocator) const
    {
        if (!allocator)
        {
            return BindlessIndexAllocator::INVALID_INDEX;
        }

        // 调用纹理索引分配器
        // 深度纹理作为可采样资源使用纹理索引范围
        return allocator->AllocateTextureIndex();
    }

    /**
     * @brief 释放深度纹理专属的Bindless索引
     *
     * 教学要点:
     * 1. 必须使用FreeTextureIndex()释放深度纹理索引
     * 2. 不能使用FreeBufferIndex()释放纹理索引(会导致索引泄漏)
     * 3. FreeList架构保证O(1)时间复杂度
     * 4. 深度纹理在延迟渲染中作为着色器可读资源,使用纹理索引
     */
    bool D12DepthTexture::FreeBindlessIndexInternal(BindlessIndexAllocator* allocator, uint32_t index) const
    {
        if (!allocator)
        {
            return false;
        }

        // 调用纹理索引释放器
        return allocator->FreeTextureIndex(index);
    }

    // ==================== 静态辅助方法 ====================

    /**
     * 将深度格式转换为DXGI格式
     *
     * 注意: 方法从 GetFormatFromDepthType 重命名为 GetDXGIFormat
     *       参数从 DepthType 改为 DepthFormat
     */
    DXGI_FORMAT D12DepthTexture::GetDXGIFormat(DepthFormat format)
    {
        switch (format)
        {
        case DepthFormat::D32_FLOAT:
            return DXGI_FORMAT_D32_FLOAT; // 32位浮点深度，高精度（对应旧 DepthOnly 和 ShadowMap）
        case DepthFormat::D24_UNORM_S8_UINT:
            return DXGI_FORMAT_D24_UNORM_S8_UINT; // 24位深度 + 8位模板，标准配置（对应旧 DepthStencil）
        case DepthFormat::D16_UNORM:
            return DXGI_FORMAT_D16_UNORM; // 16位深度，性能优化
        default:
            return DXGI_FORMAT_D24_UNORM_S8_UINT; // 默认值
        }
    }

    /**
     * 获取对应的类型化格式 (用于SRV)
     */
    DXGI_FORMAT D12DepthTexture::GetTypedFormat(DXGI_FORMAT depthFormat)
    {
        switch (depthFormat)
        {
        case DXGI_FORMAT_D32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT; // 深度作为单通道浮点纹理读取
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // 深度部分作为24位归一化值
        case DXGI_FORMAT_D16_UNORM:
            return DXGI_FORMAT_R16_UNORM; // 16位深度
        default:
            return DXGI_FORMAT_UNKNOWN; // 不支持的格式
        }
    }

    // ==================== 内部辅助方法 ====================

    /**
     * 创建深度纹理资源 - 对应Iris的IrisRenderSystem.createTexture()
     *
     * Iris调用分析:
     * ```java
     * super(IrisRenderSystem.createTexture(GL_TEXTURE_2D));
     * ```
     *
     * 我们的实现使用D3D12RenderSystem::CreateCommittedResource()
     */
    bool D12DepthTexture::CreateDepthResource()
    {
        // 1. 设置堆属性（深度纹理通常使用DEFAULT堆）
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type                  = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask      = 1;
        heapProps.VisibleNodeMask       = 1;

        // 2. 创建资源描述符
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2D深度纹理
        resourceDesc.Alignment           = 0; // 让DirectX自动选择对齐
        resourceDesc.Width               = m_width; // 深度纹理宽度
        resourceDesc.Height              = m_height; // 深度纹理高度
        resourceDesc.DepthOrArraySize    = 1; // 深度固定为1
        resourceDesc.MipLevels           = 1; // 深度纹理通常不需要Mip
        resourceDesc.Format              = m_depthFormat; // 深度格式
        resourceDesc.SampleDesc.Count    = 1; // 无多重采样
        resourceDesc.SampleDesc.Quality  = 0; // 质量级别0
        resourceDesc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN; // 让DirectX优化

        // 设置资源标志
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // 允许作为深度模板
        if (m_supportSampling)
        {
            // 支持采样的深度纹理需要着色器资源访问，不设置DENY_SHADER_RESOURCE
            // resourceDesc.Flags |= 额外标志；
        }

        // 3. 确定初始资源状态
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE; // 深度写入状态

        // 4. 设置清除值优化
        D3D12_CLEAR_VALUE clearValue    = {};
        clearValue.Format               = m_depthFormat;
        clearValue.DepthStencil.Depth   = m_clearDepth;
        clearValue.DepthStencil.Stencil = m_clearStencil;

        // 5. 使用D3D12RenderSystem统一接口创建资源
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
            assert(false && "Failed to create D3D12 depth texture resource");
            return false;
        }

        // 计算深度纹理大小 (深度纹理通常比颜色纹理小)
        size_t depthTextureSize = static_cast<size_t>(m_width) * m_height;
        switch (m_depthFormat)
        {
        case DXGI_FORMAT_D32_FLOAT:
            depthTextureSize *= 4; // 32位 = 4字节
            break;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            depthTextureSize *= 4; // 24+8位 = 4字节
            break;
        case DXGI_FORMAT_D16_UNORM:
            depthTextureSize *= 2; // 16位 = 2字节
            break;
        default:
            depthTextureSize *= 4; // 默认4字节
            break;
        }

        // 使用基类方法设置资源
        SetResource(resource, initialState, depthTextureSize);

        return true;
    }

    /**
     * 创建深度模板视图
     */
    bool D12DepthTexture::CreateDepthStencilView()
    {
        // 1. 获取全局描述符堆管理器并分配DSV句柄
        auto* heapManager = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
        if (!heapManager)
        {
            LogError(LogRenderer, "[D12DepthTexture] Failed to get GlobalDescriptorHeapManager for '%s'", m_name.c_str());
            return false;
        }

        // 分配DSV描述符
        auto dsvAlloc = heapManager->AllocateDsv();
        if (!dsvAlloc.isValid)
        {
            LogError(LogRenderer, "[D12DepthTexture] Failed to allocate DSV for '%s'", m_name.c_str());
            return false;
        }

        m_dsvHandle = dsvAlloc.cpuHandle;

        // 2. 配置DSV描述符
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format                        = m_depthFormat;
        dsvDesc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags                         = D3D12_DSV_FLAG_NONE;
        dsvDesc.Texture2D.MipSlice            = 0;

        // 3. 创建DSV
        auto device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            LogError(LogRenderer, "[D12DepthTexture] Failed to get D3D12 device for '%s'", m_name.c_str());
            return false;
        }

        heapManager->CreateDepthStencilView(
            device,
            GetResource(),
            &dsvDesc,
            dsvAlloc.heapIndex
        );

        // 4. 设置有效标志
        m_hasValidDSV = true;

        // 5. 添加成功日志
        const char* formatStr = "";
        switch (m_format)
        {
        case DepthFormat::D32_FLOAT:
            formatStr = "D32_FLOAT";
            break;
        case DepthFormat::D24_UNORM_S8_UINT:
            formatStr = "D24_UNORM_S8_UINT";
            break;
        case DepthFormat::D16_UNORM:
            formatStr = "D16_UNORM";
            break;
        default:
            formatStr = "UNKNOWN";
            break;
        }

        LogInfo(LogRenderer, "[D12DepthTexture] Created DSV for '%s': %ux%u, Format=%s",
                m_name.c_str(),
                m_width,
                m_height,
                formatStr);

        return true;
    }

    /**
     * 创建着色器资源视图 (如果支持)
     */
    bool D12DepthTexture::CreateShaderResourceView()
    {
        if (!m_supportSampling)
        {
            return true; // 不需要SRV
        }

        // TODO: 实现SRV创建
        // 需要使用GetTypedFormat()获取类型化格式
        //
        // 伪代码:
        // auto device = D3D12RenderSystem::GetDevice();
        // auto srvHeap = D3D12RenderSystem::GetSRVDescriptorHeap();
        // m_srvHandle = D3D12RenderSystem::AllocateSRVDescriptor();
        //
        // DXGI_FORMAT srvFormat = GetTypedFormat(m_depthFormat);
        // D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        // srvDesc.Format = srvFormat;
        // srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        // srvDesc.Texture2D.MipLevels = 1;
        // srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        //
        // device->CreateShaderResourceView(GetResource(), &srvDesc, m_srvHandle);

        // 临时实现：标记为已创建
        m_hasSRV = true;
        return true;
    }
} // namespace enigma::graphic
