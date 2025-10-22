/**
 * @file D12Texture.hpp
 * @brief DirectX 12纹理资源封装 - 通用纹理资源管理
 *
 * 教学重点:
 * 1. 通用纹理资源的封装和管理
 * 2. DirectX 12纹理创建和视图管理
 * 3. Bindless纹理绑定架构支持
 * 4. 与D12DepthTexture和D12RenderTarget的职责分离
 *
 * 架构设计:
 * - 继承D12Resource，提供统一的资源管理
 * - 专门用于非深度、非渲染目标纹理（着色器资源、计算纹理等）
 * - 支持Bindless资源绑定架构
 * - 对应Iris中通用纹理的概念
 *
 * 职责分离:
 * - D12Texture: 通用纹理资源（SRV、UAV）
 * - D12DepthTexture: 深度纹理资源（DSV、可选SRV）
 * - D12RenderTarget: 渲染目标纹理（RTV、SRV）
 */

#pragma once

#include "../D12Resources.hpp"
#include <memory>
#include <string>
#include <dxgi1_6.h>

#include "Engine/Resource/ResourceCommon.hpp"

namespace enigma::graphic
{
    // 前向声明
    class BindlessIndexAllocator;
    /**
     * @brief 纹理使用标志枚举
     * @details 决定纹理的用途和创建的视图类型
     *
     * 教学要点: 专注于通用纹理用途，不包含深度和渲染目标
     */
    enum class TextureUsage : uint32_t
    {
        ShaderResource = 0x1, ///< 着色器资源 (SRV) - 最常用
        UnorderedAccess = 0x2, ///< 无序访问 (UAV) - 用于Compute Shader
        CopySource = 0x4, ///< 复制源
        CopyDestination = 0x8, ///< 复制目标
        RenderTarget = 0x10, ///< 渲染目标 (RTV) - Milestone 3.0修复
        DepthStencil = 0x20, ///< 深度模板 (DSV) - Milestone 3.0修复

        // 组合标志
        Default = ShaderResource,
        Compute = ShaderResource | UnorderedAccess,
        RT_And_SRV = RenderTarget | ShaderResource, ///< 常用组合：RenderTarget + ShaderResource
        AllUsages = ShaderResource | UnorderedAccess | CopySource | CopyDestination | RenderTarget | DepthStencil
    };

    // ==================== 位运算操作符重载 (TextureUsage) ====================
    // 注意: 必须在TextureUsage枚举之后、D12Texture类之前定义，以便类内方法可以使用

    inline TextureUsage operator|(TextureUsage a, TextureUsage b)
    {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline TextureUsage operator&(TextureUsage a, TextureUsage b)
    {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    /// @brief 检查TextureUsage标志是否包含指定的标志位
    /// @param value 要检查的标志集合
    /// @param flag 要查找的标志位
    /// @return 如果value包含flag返回true，否则返回false
    inline bool HasFlag(TextureUsage value, TextureUsage flag)
    {
        return (value & flag) == flag;
    }

    /**
     * @brief 纹理类型枚举
     * @details 支持不同维度的纹理类型
     */
    enum class TextureType
    {
        Texture1D, ///< 1D纹理
        Texture2D, ///< 2D纹理 (最常用)
        Texture3D, ///< 3D纹理
        TextureCube, ///< 立方体纹理
        Texture1DArray, ///< 1D纹理数组
        Texture2DArray ///< 2D纹理数组
    };

    /**
     * @brief 纹理创建信息结构
     * @details 对应Iris纹理创建参数，但适配DirectX 12
     */
    struct TextureCreateInfo
    {
        // 基础属性
        TextureType type; ///< 纹理类型
        uint32_t    width; ///< 宽度
        uint32_t    height; ///< 高度 (1D纹理忽略)
        uint32_t    depth; ///< 深度 (3D纹理使用，其他忽略)
        uint32_t    mipLevels; ///< Mip层级数 (0表示自动生成)
        uint32_t    arraySize; ///< 数组大小 (非数组纹理为1)
        DXGI_FORMAT format; ///< 纹理格式
        Rgba8       color = Rgba8::WHITE; /// Texture color pure

        // 使用属性
        TextureUsage usage; ///< 使用标志

        // 数据属性
        const void* initialData; ///< 初始数据指针 (可为nullptr)
        size_t      dataSize; ///< 数据大小
        uint32_t    rowPitch; ///< 行间距 (字节)
        uint32_t    slicePitch; ///< 切片间距 (字节)

        // 调试属性
        const char* debugName; ///< 调试名称

        // 默认构造
        TextureCreateInfo()
            : type(TextureType::Texture2D)
              , width(0), height(0), depth(1)
              , mipLevels(1), arraySize(1)
              , format(DXGI_FORMAT_R8G8B8A8_UNORM)
              , usage(TextureUsage::Default)
              , initialData(nullptr), dataSize(0)
              , rowPitch(0), slicePitch(0)
              , debugName(nullptr)
        {
        }
    };

    /**
     * @brief DirectX 12纹理封装类
     * @details 专门用于通用纹理资源管理，与D12DepthTexture和D12RenderTarget分离
     *
     * 核心特性:
     * - 支持1D/2D/3D/Cube纹理
     * - 自动创建SRV和UAV视图
     * - Bindless资源绑定支持
     * - 高效的资源状态管理
     *
     * 职责边界:
     * - 着色器资源纹理 (SRV)
     * - 计算着色器纹理 (UAV)
     * 不处理
     * - 深度纹理 (由D12DepthTexture处理)
     * - 渲染目标 (由D12RenderTarget处理)
     */
    class D12Texture : public D12Resource
    {
    public:
        /**
         * @brief 构造函数
         * @param createInfo 纹理创建参数
         */
        explicit D12Texture(const TextureCreateInfo& createInfo);

        /**
         * @brief 析构函数
         * @details 自动清理纹理资源和描述符
         */
        virtual ~D12Texture();

        // 禁用拷贝构造和赋值（RAII原则）
        D12Texture(const D12Texture&)            = delete;
        D12Texture& operator=(const D12Texture&) = delete;

        // 支持移动语义
        D12Texture(D12Texture&& other) noexcept;
        D12Texture& operator=(D12Texture&& other) noexcept;

        // ==================== 纹理属性访问 ====================

        /**
         * @brief 获取纹理类型
         * @return 纹理类型枚举
         */
        TextureType GetTextureType() const { return m_textureType; }

        /**
         * @brief 获取纹理宽度
         * @return 宽度像素数
         */
        uint32_t GetWidth() const { return m_width; }

        /**
         * @brief 获取纹理高度
         * @return 高度像素数
         */
        uint32_t GetHeight() const { return m_height; }

        /**
         * @brief 获取纹理深度
         * @return 深度层数 (3D纹理)
         */
        uint32_t GetDepth() const { return m_depth; }

        /**
         * @brief 获取Mip层级数
         * @return Mip层级数
         */
        uint32_t GetMipLevels() const { return m_mipLevels; }

        /**
         * @brief 获取数组大小
         * @return 数组元素数量
         */
        uint32_t GetArraySize() const { return m_arraySize; }

        /**
         * @brief 获取纹理格式
         * @return DXGI格式
         */
        DXGI_FORMAT GetFormat() const { return m_format; }

        /**
         * @brief 获取使用标志
         * @return 纹理使用标志
         */
        TextureUsage GetUsage() const { return m_usage; }

        // ==================== 描述符访问接口 ====================

        /**
         * @brief 获取着色器资源视图句柄
         * @return SRV句柄，用于着色器读取
         *
         * DirectX 12 API: ID3D12Device::CreateShaderResourceView()
         * 参考: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createshaderresourceview
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() const;

        /**
         * @brief 获取无序访问视图句柄
         * @return UAV句柄，用于Compute Shader读写
         *
         * DirectX 12 API: ID3D12Device::CreateUnorderedAccessView()
         * 参考: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createunorderedaccessview
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetUAVHandle() const;

        /**
         * @brief 检查是否有着色器资源视图
         * @return 是否可以作为着色器资源
         */
        bool HasShaderResourceView() const { return m_hasSRV; }

        /**
         * @brief 检查是否有无序访问视图
         * @return 是否可以作为UAV
         */
        bool HasUnorderedAccessView() const { return m_hasUAV; }

        // ==================== Bindless支持说明 (Milestone 2.3更新) ====================
        //
        // Bindless注册功能已统一到D12Resource基类中：
        // - RegisterToBindlessManager() - 统一的便捷注册方法
        // - UnregisterFromBindlessManager() - 统一的注销方法
        //
        // 使用方式：
        //   auto texture = std::make_shared<D12Texture>(...);
        //   auto index = texture->RegisterToBindlessManager(manager);
        //
        // 这样设计的优势：
        // 1. DRY原则 - 避免在每个资源类型重复实现
        // 2. 一致性 - 所有资源类型使用相同接口
        // 3. 类型安全 - 基类自动进行类型检测和转换
        //
        // ==================== Texture特定接口 ====================


        // ==================== 纹理操作接口 ====================

        /**
         * @brief 更新纹理数据
         * @param data 新数据指针
         * @param dataSize 数据大小
         * @param mipLevel 要更新的Mip层级
         * @param arraySlice 要更新的数组切片
         * @return 是否成功更新
         *
         *  // TODO: Use CommandListManager
         * DirectX 12 API: ID3D12GraphicsCommandList::CopyTextureRegion()
         * 参考: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copytextureregion
         */
        bool UpdateTextureData(
            const void* data,
            size_t      dataSize,
            uint32_t    mipLevel   = 0,
            uint32_t    arraySlice = 0
        );

        /**
         * @brief 生成Mip贴图
         * @return 是否成功生成
         *
         * 教学要点: Mip贴图提升纹理采样性能和质量
         * 通过Compute Shader实现Mip生成
         */
        bool GenerateMips();

        // ==================== 调试支持 ====================

        /**
         * @brief 重写虚函数：设置调试名称并添加纹理特定逻辑
         * @param name 调试名称
         *
         * 教学要点: 重写基类虚函数，在设置纹理名称时同时更新SRV和UAV的调试名称
         */
        void SetDebugName(const std::string& name) override;

        /**
         * @brief 重写虚函数：获取包含纹理尺寸信息的调试名称
         * @return 格式化的调试名称字符串
         *
         * 教学要点: 重写基类虚函数，返回包含纹理特定信息的名称
         * 格式: "TextureName (2048x1024, RGBA8, Mip:4)"
         */
        const std::string& GetDebugName() const override;

        /**
         * @brief 重写虚函数：获取纹理的详细调试信息
         * @return 包含尺寸、格式、类型的调试字符串
         *
         * 教学要点: 提供纹理特定的详细调试信息
         * 包括尺寸、格式、Mip层级、使用标志、Bindless索引等信息
         */
        std::string GetDebugInfo() const override;

        // ==================== 静态辅助方法 (公共访问) ====================

        /**
         * @brief 获取格式的字节数
         * @param format DXGI格式
         * @return 每像素字节数
         */
        static uint32_t GetFormatBytesPerPixel(DXGI_FORMAT format);

    protected:
        /**
         * @brief 分配Bindless纹理索引（重写基类纯虚函数）
         * @param allocator 索引分配器
         * @return 成功返回纹理索引(0-999999)，失败返回INVALID_BINDLESS_INDEX
         *
         * 教学要点:
         * 1. Template Method模式 - 基类管理流程，子类决定索引类型
         * 2. 调用allocator->AllocateTextureIndex()分配纹理专用索引
         * 3. 纹理索引范围: 0 - 999,999
         */
        uint32_t AllocateBindlessIndexInternal(BindlessIndexAllocator* allocator) const override;

        /**
         * @brief 释放Bindless纹理索引（重写基类纯虚函数）
         * @param allocator 索引分配器
         * @param index 要释放的索引
         * @return 成功返回true
         *
         * 教学要点:
         * 1. Template Method模式 - 子类实现具体释放逻辑
         * 2. 调用allocator->FreeTextureIndex(index)释放纹理索引
         * 3. 索引范围验证: 必须在0-999999范围内
         */
        bool FreeBindlessIndexInternal(BindlessIndexAllocator* allocator, uint32_t index) const override;

        void CreateDescriptorInGlobalHeap(ID3D12Device* device, GlobalDescriptorHeapManager* heapManager) override;

        // ==================== GPU资源上传(Milestone 2.7) ====================

        /**
         * @brief 重写基类方法: 实现纹理特定的上传逻辑
         * @param commandList Copy队列命令列表
         * @param uploadContext Upload Heap上下文
         * @return 上传成功返回true
         *
         * 教学要点:
         * 1. 使用UploadContext::UploadTextureData()执行纹理上传
         * 2. 计算rowPitch和slicePitch(行间距和切片间距)
         * 3. 只上传Mip 0层级(其他Mip由GenerateMips()生成)
         * 4. 资源状态转换由D12Resource::Upload()基类处理
         */
        bool UploadToGPU(
            ID3D12GraphicsCommandList* commandList,
            class UploadContext&       uploadContext
        ) override;

        /**
         * @brief 重写基类方法: 获取纹理上传后的目标状态
         * @return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE(像素着色器资源)
         *
         * 教学要点: 纹理通常作为像素着色器资源使用
         */
        D3D12_RESOURCE_STATES GetUploadDestinationState() const override;

        /**
         * @brief 重写基类方法: 检查纹理是否需要CPU数据上传
         * @return RenderTarget/DepthStencil返回false，其他纹理返回true
         *
         * 教学要点:
         * 1. RenderTarget和DepthStencil是输出纹理，由GPU渲染写入
         * 2. 普通纹理(贴图)是输入纹理，需要从CPU加载数据
         * 3. 这个方法允许Upload()跳过对输出纹理的CPU数据检查
         */
        bool RequiresCPUData() const override
        {
            // 输出纹理(RenderTarget/DepthStencil)不需要CPU数据
            return !HasFlag(m_usage, TextureUsage::RenderTarget) &&
                !HasFlag(m_usage, TextureUsage::DepthStencil);
        }

        // ==================== 静态辅助方法 (受保护访问) ====================

        /**
         * @brief 计算纹理大小
         * @param width 宽度
         * @param height 高度
         * @param format 格式
         * @return 所需字节数
         */
        static size_t CalculateTextureSize(uint32_t width, uint32_t height, DXGI_FORMAT format);

        /**
         * @brief 检查格式是否支持UAV
         * @param format DXGI格式
         * @return 是否支持UAV
         */
        static bool IsUAVCompatibleFormat(DXGI_FORMAT format);

    private:
        // ==================== 纹理属性 ====================
        TextureType  m_textureType; ///< 纹理类型
        uint32_t     m_width; ///< 宽度
        uint32_t     m_height; ///< 高度
        uint32_t     m_depth; ///< 深度
        uint32_t     m_mipLevels; ///< Mip层级数
        uint32_t     m_arraySize; ///< 数组大小
        DXGI_FORMAT  m_format; ///< 纹理格式
        TextureUsage m_usage; ///< 使用标志

        // ==================== 描述符管理 ====================
        D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle; ///< 着色器资源视图句柄
        D3D12_CPU_DESCRIPTOR_HANDLE m_uavHandle; ///< 无序访问视图句柄
        bool                        m_hasSRV; ///< 是否有SRV
        bool                        m_hasUAV; ///< 是否有UAV

        mutable std::string m_formattedDebugName; ///< 格式化的调试名称（用于GetDebugName重写）

        // ==================== 内部辅助方法 ====================

        /**
         * @brief 创建DirectX 12纹理资源
         * @param createInfo 创建参数
         * @return 是否创建成功
         *
         * 教学要点: 使用D3D12RenderSystem统一API，遵循分层架构
         */
        bool CreateD3D12Resource(const TextureCreateInfo& createInfo);

        /**
         * @brief 创建着色器资源视图
         * @return 是否创建成功
         *
         * DirectX 12 API: ID3D12Device::CreateShaderResourceView()
         */
        bool CreateShaderResourceView();

        /**
         * @brief 创建无序访问视图
         * @return 是否创建成功
         *
         * DirectX 12 API: ID3D12Device::CreateUnorderedAccessView()
         */
        bool CreateUnorderedAccessView();

        /**
         * @brief 获取资源描述
         * @param createInfo 创建信息
         * @return DirectX 12资源描述
         */
        static D3D12_RESOURCE_DESC GetResourceDesc(const TextureCreateInfo& createInfo);

        /**
         * @brief 获取资源标志
         * @param usage 使用标志
         * @return DirectX 12资源标志
         */
        static D3D12_RESOURCE_FLAGS GetResourceFlags(TextureUsage usage);

        /**
         * @brief 获取初始资源状态
         * @param usage 使用标志
         * @return 初始资源状态
         */
        static D3D12_RESOURCE_STATES GetInitialState(TextureUsage usage);
    };

    // ==================== 类型别名 ====================
    using TexturePtr = std::unique_ptr<D12Texture>;
} // namespace enigma::graphic
