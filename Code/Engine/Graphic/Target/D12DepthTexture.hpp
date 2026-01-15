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
     * @brief 深度纹理创建信息结构
     * @details 对应Iris DepthTexture构造参数，但适配DirectX 12
     *
     * 注意: 统一使用 DXGI_FORMAT 作为深度格式类型
     */
    struct DepthTextureCreateInfo
    {
        // 基础属性
        std::string name; ///< 深度纹理名称 - 对应Iris的name参数
        uint32_t    width; ///< 宽度 - 对应Iris的width参数
        uint32_t    height; ///< 高度 - 对应Iris的height参数
        DXGI_FORMAT depthFormat; ///< 深度格式 - 使用DXGI_FORMAT

        // 清除值设置
        float   clearDepth; ///< 默认清除深度值
        uint8_t clearStencil; ///< 默认清除模板值

        // 调试属性
        const char* debugName; ///< 调试名称

        // 默认构造
        DepthTextureCreateInfo()
            : name("")
              , width(0), height(0)
              , depthFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)
              , clearDepth(1.0f), clearStencil(0)
              , debugName(nullptr)
        {
        }

        // 便利构造函数
        DepthTextureCreateInfo(
            const std::string& texName,
            uint32_t           w, uint32_t h,
            DXGI_FORMAT        format  = DXGI_FORMAT_D24_UNORM_S8_UINT,
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
        DXGI_FORMAT m_depthFormat; ///< DXGI深度格式

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

        /**
         * @brief Override D12Resource virtual function: depth texture does not require CPU data
         * @return always returns false
         *
         *Teaching points:
         * 1. The depth texture is the GPU rendering output target and is written directly by the GPU.
         * 2. Unlike Input Texture (texture), depth texture does not require CPU data to be uploaded.
         * 3. Override this method to let the base class Upload() skip CPU data checking
         * 4. Consistent with the design of D12RenderTarget::RequiresCPUData()
         */
        bool RequiresCPUData() const override { return false; }

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
         * @brief 获取对应的类型化格式 (用于SRV)
         * @param depthFormat 深度格式
         * @return 类型化格式 (如果支持)
         */
        static DXGI_FORMAT GetTypedFormat(DXGI_FORMAT depthFormat);

        /**
         * @brief [NEW] 获取对应的 TYPELESS 格式 (用于资源创建)
         * @param depthFormat 深度格式
         * @return TYPELESS 格式
         *
         * D3D12 规则: 同时作为 DSV 和 SRV 使用，必须用 TYPELESS 格式创建资源
         * - D32_FLOAT -> R32_TYPELESS
         * - D24_UNORM_S8_UINT -> R24G8_TYPELESS
         * - D16_UNORM -> R16_TYPELESS
         */
        static DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT depthFormat);

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
    //   - 使用 DXGI_FORMAT_D32_FLOAT
    //   - 仅用于深度测试，不支持采样
    //
    // - static DepthTexturePtr CreateDepthStencilTexture(uint32_t width, uint32_t height, const std::string& name);
    //   - 使用 DXGI_FORMAT_D24_UNORM_S8_UINT
    //   - 支持深度+模板操作
    //
    // - static DepthTexturePtr CreateShadowMapTexture(uint32_t width, uint32_t height, const std::string& name);
    //   - 使用 DXGI_FORMAT_D32_FLOAT
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
