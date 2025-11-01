/**
 * @file D12DepthTexture.hpp
 * @brief DirectX 12深度纹理封装 - 基于Iris DepthTexture.java的DirectX实现
 *
 * 教学重点:
 * 1. 对比学习Iris DepthTexture.java的继承架构设计
 * 2. DirectX 12深度缓冲区与OpenGL深度纹理的差异
 * 3. 深度纹理格式选择和性能考虑 (D24_UNORM_S8_UINT vs D32_FLOAT)
 * 4. DSV/SRV双视图管理，支持深度采样和阴影贴图
 *
 * Iris架构对应关系:
 * - Iris: DepthTexture extends GlResource
 * - Ours: D12DepthTexture extends D12Resource
 * - Iris: 使用IrisRenderSystem.createTexture()
 * - Ours: 使用D3D12RenderSystem::CreateCommittedResource()
 * - Iris: getTextureId()返回OpenGL纹理ID
 * - Ours: GetResource()返回ID3D12Resource*
 *
 * 架构设计原则:
 * - 严格继承D12Resource，而非组合模式
 * - 专门处理深度纹理，职责单一
 * - 通过D3D12RenderSystem统一API访问DirectX
 * - 对应Iris中深度纹理的完整功能
 */

#pragma once

#include "../Resource/D12Resources.hpp"
#include <memory>
#include <string>
#include <cstdint>
#include <dxgi1_6.h>

namespace enigma::graphic
{
    /**
     * @brief 深度纹理格式枚举
     *
     * **行业标准**:
     * - D32_FLOAT: 最高精度，用于主深度和精确计算
     * - D24_UNORM_S8_UINT: 平衡精度和性能，带模板缓冲
     * - D16_UNORM: 性能优化，用于低精度场景
     *
     * 教学要点:
     * - 深度格式的精度 vs 性能权衡
     * - 模板缓冲的用途（轮廓、蒙版）
     *
     * 注意: 这个枚举从 DepthTextureConfig.hpp 移动到这里，统一深度格式定义
     */
    enum class DepthFormat : uint8_t
    {
        D32_FLOAT, ///< 32位浮点深度（最高精度）- 对应旧 DepthType::DepthOnly 和 ShadowMap
        D24_UNORM_S8_UINT, ///< 24位深度 + 8位模板（标准配置）- 对应旧 DepthType::DepthStencil
        D16_UNORM ///< 16位深度（性能优化）
    };

    /**
     * @brief 深度纹理配置结构体
     *
     * **M3.1核心数据结构**:
     * - 支持每个深度纹理独立配置分辨率
     * - 支持不同深度格式（D32/D24/D16）
     * - 语义化命名（depthtex0/1/2，Iris兼容）
     *
     * **行业标准实践**:
     * - 主深度（depthtex0）: 必须=渲染分辨率
     * - 辅助深度（depthtex1-N）: 可降低分辨率优化内存
     *
     * 教学要点:
     * - C++20聚合初始化（.width = 1920语法）
     * - 配置结构体的设计原则
     * - 深度纹理的语义化命名
     *
     * 注意: 这个结构体从 DepthTextureConfig.hpp 移动到这里
     */
    struct DepthTextureConfig
    {
        int         width        = 0; ///< 深度纹理宽度（像素）
        int         height       = 0; ///< 深度纹理高度（像素）
        DepthFormat format       = DepthFormat::D32_FLOAT; ///< 深度格式（默认最高精度）
        std::string semanticName = ""; ///< 语义化名称（如"depthtex0"）

        /**
         * @brief 验证配置是否有效
         * @return 宽高>0且名称非空则有效
         *
         * 教学要点:
         * - noexcept保证（简单验证逻辑）
         * - 配置验证的设计模式
         */
        [[nodiscard]] bool IsValid() const noexcept
        {
            return width > 0 && height > 0 && !semanticName.empty();
        }
    };

    /**
     * @brief 深度纹理配置预设函数集合
     *
     * **设计模式**: Preset Pattern（预设模式）
     * - 提供常用配置的便捷创建方法
     * - 减少用户配置负担
     * - 确保配置的合法性和一致性
     *
     * **使用示例**:
     * ```cpp
     * // 全分辨率
     * auto config1 = DepthTexturePresets::FullResolution(1920, 1080, "depthtex1");
     *
     * // 半分辨率（节省75%内存）
     * auto config2 = DepthTexturePresets::HalfResolution(1920, 1080, "depthtex2");
     * ```
     *
     * 教学要点:
     * - 静态工厂方法模式
     * - 便捷API的设计原则
     * - 性能 vs 质量的权衡策略
     *
     * 注意: 这个类从 DepthTextureConfig.hpp 移动到这里
     */
    class DepthTexturePresets
    {
    public:
        /**
         * @brief 创建全分辨率深度纹理配置
         * @param renderWidth 渲染宽度（1:1匹配）
         * @param renderHeight 渲染高度（1:1匹配）
         * @param name 语义化名称
         * @return 全分辨率配置
         *
         * **适用场景**:
         * - 主深度纹理（depthtex0）- 强制使用
         * - 需要精确屏幕空间效果的辅助深度
         */
        [[nodiscard]] static DepthTextureConfig FullResolution(
            int                renderWidth,
            int                renderHeight,
            const std::string& name
        )
        {
            DepthTextureConfig config;
            config.width        = renderWidth;
            config.height       = renderHeight;
            config.format       = DepthFormat::D32_FLOAT;
            config.semanticName = name;
            return config;
        }

        /**
         * @brief 创建半分辨率深度纹理配置
         * @param renderWidth 渲染宽度（除以2）
         * @param renderHeight 渲染高度（除以2）
         * @param name 语义化名称
         * @return 半分辨率配置
         *
         * **内存节省**: 75%（单个纹理）
         *
         * **适用场景**:
         * - 辅助深度纹理（depthtex1/2）
         * - 不需要精确像素级对应的屏幕空间效果
         */
        [[nodiscard]] static DepthTextureConfig HalfResolution(
            int                renderWidth,
            int                renderHeight,
            const std::string& name
        )
        {
            DepthTextureConfig config;
            config.width        = renderWidth / 2;
            config.height       = renderHeight / 2;
            config.format       = DepthFormat::D32_FLOAT;
            config.semanticName = name;
            return config;
        }

        /**
         * @brief 创建四分之一分辨率深度纹理配置
         * @param renderWidth 渲染宽度（除以4）
         * @param renderHeight 渲染高度（除以4）
         * @param name 语义化名称
         * @return 四分之一分辨率配置
         *
         * **内存节省**: 93.75%（单个纹理）
         */
        [[nodiscard]] static DepthTextureConfig QuarterResolution(
            int                renderWidth,
            int                renderHeight,
            const std::string& name
        )
        {
            DepthTextureConfig config;
            config.width        = renderWidth / 4;
            config.height       = renderHeight / 4;
            config.format       = DepthFormat::D32_FLOAT;
            config.semanticName = name;
            return config;
        }

        /**
         * @brief 创建自定义分辨率深度纹理配置
         * @param width 自定义宽度
         * @param height 自定义高度
         * @param format 深度格式
         * @param name 语义化名称
         * @return 自定义配置
         */
        [[nodiscard]] static DepthTextureConfig Custom(
            int                width,
            int                height,
            DepthFormat        format,
            const std::string& name
        )
        {
            DepthTextureConfig config;
            config.width        = width;
            config.height       = height;
            config.format       = format;
            config.semanticName = name;
            return config;
        }
    };

    /**
     * @brief 深度纹理创建信息结构
     * @details 对应Iris DepthTexture构造参数，但适配DirectX 12
     *
     * 注意: depthType 已重命名为 depthFormat，使用 DepthFormat 枚举
     */
    struct DepthTextureCreateInfo
    {
        // 基础属性
        std::string name; ///< 深度纹理名称 - 对应Iris的name参数
        uint32_t    width; ///< 宽度 - 对应Iris的width参数
        uint32_t    height; ///< 高度 - 对应Iris的height参数
        DepthFormat depthFormat; ///< 深度格式 - 对应Iris的DepthBufferFormat（原 depthType）

        // 清除值设置
        float   clearDepth; ///< 默认清除深度值
        uint8_t clearStencil; ///< 默认清除模板值

        // 调试属性
        const char* debugName; ///< 调试名称

        // 默认构造
        DepthTextureCreateInfo()
            : name("")
              , width(0), height(0)
              , depthFormat(DepthFormat::D24_UNORM_S8_UINT) // 对应旧 DepthType::DepthStencil
              , clearDepth(1.0f), clearStencil(0)
              , debugName(nullptr)
        {
        }

        // 便利构造函数
        DepthTextureCreateInfo(
            const std::string& texName,
            uint32_t           w, uint32_t h,
            DepthFormat        format  = DepthFormat::D24_UNORM_S8_UINT, // 对应旧 DepthType::DepthStencil
            float              depth   = 1.0f,
            uint8_t            stencil = 0
        )
            : name(texName)
              , width(w), height(h)
              , depthFormat(format)
              , clearDepth(depth), clearStencil(stencil)
              , debugName(texName.c_str())
        {
        }
    };

    /**
     * @brief DirectX 12深度纹理封装类 - 对应Iris DepthTexture.java
     * @details 直接继承D12Resource，专门用于深度测试、阴影贴图等深度相关渲染
     *
     * Iris对应关系:
     * - Iris: public class DepthTexture extends GlResource
     * - Ours: class D12DepthTexture : public D12Resource
     *
     * 核心特性:
     * - 专用深度格式支持 (D24_UNORM_S8_UINT、D32_FLOAT等)
     * - 深度模板视图(DSV)自动管理
     * - 支持深度纹理采样 (SRV) - 对应Iris的getTextureId()
     * - 阴影贴图支持
     *
     * 设计原则:
     * - IS-A关系: D12DepthTexture IS-A D12Resource (如同Iris)
     * - 单一职责: 专门处理深度纹理，不依赖通用纹理
     * - 零间接: 直接管理DirectX深度资源，无中间包装层
     */
    class D12DepthTexture : public D12Resource
    {
    public:

    private:
        // ==================== 视图管理 ====================
        // 注意: 不再包含D12Texture成员，直接继承D12Resource管理DirectX资源

        D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle; ///< 深度模板视图句柄
        D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle; ///< 着色器资源视图句柄 (可选)
        bool                        m_hasSRV; ///< 是否支持着色器采样

        // ==================== 属性管理 ====================
        std::string m_name; ///< 深度纹理名称 - 对应Iris构造函数的name参数
        uint32_t    m_width; ///< 宽度 - 对应Iris构造函数的width参数
        uint32_t    m_height; ///< 高度 - 对应Iris构造函数的height参数
        DXGI_FORMAT m_depthFormat; ///< DXGI深度格式 - 从 DepthFormat 转换而来
        DepthFormat m_format; ///< 深度格式枚举 - 对应Iris的DepthBufferFormat（原 m_depthType）

        // ==================== 状态管理 ====================
        float               m_clearDepth; ///< 默认清除深度值
        uint8_t             m_clearStencil; ///< 默认清除模板值
        bool                m_supportSampling; ///< 是否支持深度采样
        bool                m_hasValidDSV; ///< 是否有有效的DSV
        bool                m_hasValidSRV; ///< 是否有有效的SRV
        mutable std::string m_formattedDebugName; ///< 格式化的调试名称（用于GetDebugName重写）

    public:
        /**
         * @brief 构造函数 - 对应Iris DepthTexture构造函数
         * @param createInfo 深度纹理创建参数
         *
         * Iris对应调用:
         * ```java
         * public DepthTexture(String name, int width, int height, DepthBufferFormat format) {
         *     super(IrisRenderSystem.createTexture(GL_TEXTURE_2D));
         * }
         * ```
         *
         * 我们的实现:
         * ```cpp
         * D12DepthTexture(createInfo) : D12Resource() {
         *     // 使用D3D12RenderSystem::CreateDepthTexture创建深度纹理
         * }
         * ```
         */
        explicit D12DepthTexture(const DepthTextureCreateInfo& createInfo);

        /**
         * @brief 析构函数
         * @details 自动清理深度纹理资源和视图
         *
         * 对应Iris的destroyInternal():
         * ```java
         * protected void destroyInternal() {
         *     GlStateManager._deleteTexture(getGlId());
         * }
         * ```
         */
        virtual ~D12DepthTexture();

        // 禁用拷贝构造和赋值（RAII原则）
        D12DepthTexture(const D12DepthTexture&)            = delete;
        D12DepthTexture& operator=(const D12DepthTexture&) = delete;

        // 支持移动语义
        D12DepthTexture(D12DepthTexture&& other) noexcept;
        D12DepthTexture& operator=(D12DepthTexture&& other) noexcept;

        /**
         * @brief 获取深度纹理ID - 对应Iris的getTextureId()
         * @return DirectX资源指针 (等价于Iris的OpenGL纹理ID)
         *
         * Iris对应方法:
         * ```java
         * public int getTextureId() {
         *     return getGlId();  // 返回OpenGL纹理ID
         * }
         * ```
         *
         * 我们的实现:
         * ```cpp
         * ID3D12Resource* GetDepthTextureResource() const {
         *     return GetResource();  // 继承自D12Resource
         * }
         * ```
         */
        ID3D12Resource* GetDepthTextureResource() const { return GetResource(); }

        /**
         * @brief 获取深度模板视图句柄
         * @return DSV句柄
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return m_dsvHandle; }

        /**
         * @brief 获取着色器资源视图句柄
         * @return SRV句柄 (如果支持采样)
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() const;

        /**
         * @brief 检查是否支持着色器采样
         * @return 是否可以作为纹理采样
         */
        bool HasShaderResourceView() const { return m_hasSRV; }

        bool                  UploadToGPU(ID3D12GraphicsCommandList* commandList, class UploadContext& uploadContext) override;
        D3D12_RESOURCE_STATES GetUploadDestinationState() const override;
        // ==================== 尺寸和属性管理 ====================

        /**
         * @brief 获取宽度
         * @return 宽度像素数
         */
        uint32_t GetWidth() const { return m_width; }

        /**
         * @brief 获取高度
         * @return 高度像素数
         */
        uint32_t GetHeight() const { return m_height; }

        /**
         * @brief 获取深度格式
         * @return DXGI深度格式
         */
        DXGI_FORMAT GetDepthFormat() const { return m_depthFormat; }

        /**
         * @brief 获取深度格式
         * @return 深度格式枚举
         */
        DepthFormat GetFormat() const { return m_format; }

        /**
         * @brief 获取名称
         * @return 深度纹理名称
         */
        const std::string& GetName() const { return m_name; }

        // ==================== 深度操作接口 ====================

        /**
         * @brief 调整深度纹理尺寸 - 对应Iris的resize方法
         * @param newWidth 新宽度
         * @param newHeight 新高度
         * @return 是否成功调整
         *
         * Iris对应方法:
         * ```java
         * void resize(int width, int height, DepthBufferFormat format) {
         *     IrisRenderSystem.texImage2D(getTextureId(), GL_TEXTURE_2D, 0,
         *         format.getGlInternalFormat(), width, height, 0,
         *         format.getGlType(), format.getGlFormat(), null);
         * }
         * ```
         *
         * 我们的实现需要重新创建DirectX资源和视图
         */
        bool Resize(uint32_t newWidth, uint32_t newHeight);

        /**
         * @brief 清除深度缓冲区
         * @param cmdList 命令列表
         * @param depthValue 深度清除值 (默认使用构造时设置的值)
         * @param stencilValue 模板清除值 (默认使用构造时设置的值)
         */
        void Clear(
            ID3D12GraphicsCommandList* cmdList,
            float                      depthValue   = -1.0f,
            uint8_t                    stencilValue = 255
        );

        /**
         * @brief 绑定为深度目标
         * @param cmdList 命令列表
         * @details 设置当前深度纹理为渲染目标的深度缓冲区
         */
        void BindAsDepthTarget(ID3D12GraphicsCommandList* cmdList);

        // ==================== 调试支持 ====================

        /**
         * @brief 重写虚函数：设置调试名称并添加深度纹理特定逻辑
         * @param name 调试名称
         *
         * 教学要点: 重写基类虚函数，在设置深度纹理名称时同时更新DSV和SRV的调试名称
         * 对应Iris中的GLDebug.nameObject()功能
         */
        void SetDebugName(const std::string& name) override;

        /**
         * @brief 重写虚函数：获取包含深度纹理信息的调试名称
         * @return 格式化的调试名称字符串
         *
         * 教学要点: 重写基类虚函数，返回包含深度纹理特定信息的名称
         * 格式: "DepthTextureName (1024x768, D24S8, SampleRead:Yes)"
         */
        const std::string& GetDebugName() const override;

        /**
         * @brief 重写虚函数：获取深度纹理的详细调试信息
         * @return 包含尺寸、格式、类型的调试字符串
         *
         * 教学要点: 提供深度纹理特定的详细调试信息
         * 包括深度类型、格式、是否支持采样、DSV/SRV状态等
         */
        std::string GetDebugInfo() const override;

    protected:
        /**
         * @brief 分配Bindless索引（DepthTexture子类实现）
         * @param allocator Bindless索引分配器指针
         * @return 成功返回有效索引（0-999,999），失败返回INVALID_INDEX
         *
         * 教学要点:
         * 1. 深度纹理使用纹理索引范围（0-999,999）
         * 2. 调用allocator->AllocateTextureIndex()获取纹理专属索引
         * 3. FreeList O(1)分配策略
         */
        uint32_t AllocateBindlessIndexInternal(BindlessIndexAllocator* allocator) const override;

        /**
         * @brief 释放Bindless索引（DepthTexture子类实现）
         * @param allocator Bindless索引分配器指针
         * @param index 要释放的索引值
         * @return 成功返回true，失败返回false
         *
         * 教学要点:
         * 1. 调用allocator->FreeTextureIndex(index)释放纹理专属索引
         * 2. 必须使用与AllocateTextureIndex对应的释放函数
         */
        bool FreeBindlessIndexInternal(BindlessIndexAllocator* allocator, uint32_t index) const override;

        /**
         * @brief 在全局描述符堆中创建描述符（DepthTexture子类实现）
         * @param device DirectX 12设备指针
         * @param heapManager 全局描述符堆管理器指针
         *
         * 教学要点:
         * 1. 深度纹理创建SRV描述符（用于着色器采样）
         * 2. 使用m_bindlessIndex作为堆中的索引位置
         * 3. DSV描述符由单独的DSV堆管理，不在全局堆中
         */
        void CreateDescriptorInGlobalHeap(ID3D12Device* device, class GlobalDescriptorHeapManager* heapManager) override;

        // ==================== 静态辅助方法 ====================

        /**
         * @brief 将深度格式转换为DXGI格式
         * @param format 深度格式枚举
         * @return 对应的DXGI格式
         *
         * 注意: 方法从 GetFormatFromDepthType 重命名为 GetDXGIFormat
         *       参数从 DepthType 改为 DepthFormat
         */
        static DXGI_FORMAT GetDXGIFormat(DepthFormat format);

        /**
         * @brief 获取对应的类型化格式 (用于SRV)
         * @param depthFormat 深度格式
         * @return 类型化格式 (如果支持)
         */
        static DXGI_FORMAT GetTypedFormat(DXGI_FORMAT depthFormat);

    private:
        // ==================== 内部辅助方法 ====================

        /**
         * @brief 创建深度纹理资源 - 对应Iris的IrisRenderSystem.createTexture()
         * @return 是否创建成功
         *
         * 使用D3D12RenderSystem::CreateCommittedResource()创建深度纹理
         * 对应Iris构造函数中的super(IrisRenderSystem.createTexture())调用
         */
        bool CreateDepthResource();

        /**
         * @brief 创建深度模板视图
         * @return 是否创建成功
         */
        bool CreateDepthStencilView();

        /**
         * @brief 创建着色器资源视图 (如果支持)
         * @return 是否创建成功
         */
        bool CreateShaderResourceView();

        /**
         * @brief 内部设置调试名称辅助方法 - 对应Iris的GLDebug.nameObject()
         *
         * Iris对应调用:
         * ```java
         * GLDebug.nameObject(GL43C.GL_TEXTURE, texture, name);
         * ```
         */
        void SetInternalDebugName();
    };

    // ==================== 未来扩展：基于用途的快捷构建函数 ====================
    // TODO: 添加基于用途的快捷构建函数，提供更好的易用性
    //
    // 建议的函数：
    // - static DepthTexturePtr CreateDepthOnlyTexture(uint32_t width, uint32_t height, const std::string& name);
    //   - 使用 DepthFormat::D32_FLOAT
    //   - 仅用于深度测试，不支持采样
    //
    // - static DepthTexturePtr CreateDepthStencilTexture(uint32_t width, uint32_t height, const std::string& name);
    //   - 使用 DepthFormat::D24_UNORM_S8_UINT
    //   - 支持深度+模板操作
    //
    // - static DepthTexturePtr CreateShadowMapTexture(uint32_t width, uint32_t height, const std::string& name);
    //   - 使用 DepthFormat::D32_FLOAT
    //   - 支持着色器采样（用于阴影贴图）
    //
    // - static DepthTexturePtr CreateFromConfig(const DepthTextureConfig& config);
    //   - 使用配置结构体创建深度纹理
    //   - 支持自定义分辨率和格式
    //
    // 这些函数将简化常见深度纹理创建场景，提供更友好的API

    // ==================== 类型别名 ====================
    using DepthTexturePtr = std::unique_ptr<D12DepthTexture>;
} // namespace enigma::graphic
