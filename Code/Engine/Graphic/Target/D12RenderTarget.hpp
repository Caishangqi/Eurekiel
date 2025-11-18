#pragma once

#include "../Resource/D12Resources.hpp"
#include "../Resource/Texture/D12Texture.hpp"
#include <memory>
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <stdexcept>

#include "RTTypes.hpp"

namespace enigma::graphic
{
    /**
     * @brief D12RenderTarget类 - 基于Iris RenderTarget的DirectX 12实现
     *
     * ============================================================================
     * 教学要点 - Iris架构对应关系分析
     * ============================================================================
     *
     * Iris源码位置:
     * net.irisshaders.iris.targets.RenderTarget.java
     *
     * 核心职责对应关系:
     *
     * 1. **双纹理设计** (Iris核心特性):
     *    - Iris: mainTexture (int) + altTexture (int)
     *    - D12:  m_mainTexture (shared_ptr<D12Texture>) + m_altTexture (shared_ptr<D12Texture>)
     *    - 教学意义: Iris使用双纹理支持Ping-Pong渲染和历史数据访问
     *
     * 2. **格式管理** (像素格式封装):
     *    - Iris: InternalTextureFormat + PixelFormat + PixelType
     *    - D12:  DXGI_FORMAT (DirectX统一格式枚举)
     *    - 教学意义: OpenGL需要3个参数描述格式，DirectX用1个枚举简化
     *
     * 3. **Builder模式** (构建器设计模式):
     *    - Iris: static Builder builder() + 流式接口
     *    - D12:  static Builder Create() + 相同的流式接口
     *    - 教学意义: Builder模式避免构造函数参数过多，提升可读性
     *
     * 4. **生命周期管理** (资源管理):
     *    - Iris: isValid标记 + destroy()方法
     *    - D12:  继承D12Resource的RAII管理 + 智能指针自动释放
     *    - 教学意义: C++RAII比手动标记更安全可靠
     *
     * 5. **尺寸管理** (渲染目标缩放):
     *    - Iris: resize(width, height) + Vector2i textureScaleOverride
     *    - D12:  ResizeIfNeeded(width, height) + 相同的resize逻辑
     *    - 教学意义: 动态resize支持分辨率变化和超分辨率渲染
     *
     * ============================================================================
     * DirectX 12特化设计
     * ============================================================================
     *
     * 1. **资源状态管理**:
     *    - 继承D12Resource的状态追踪系统
     *    - 支持RT_TARGET -> SHADER_RESOURCE状态转换
     *
     * 2. **描述符管理**:
     *    - RTV (Render Target View) 用于渲染
     *    - SRV (Shader Resource View) 用于着色器采样
     *
     * 3. **内存对齐优化**:
     *    - 支持4K对齐的纹理布局
     *    - 优化GPU内存访问性能
     *
     * 4. **多重采样支持**:
     *    - 可选的MSAA抗锯齿
     *    - 支持sample count配置
     *
     * @note 这个类直接对应Iris的RenderTarget，是render target系统的基础组件
     */
    class D12RenderTarget : public D12Resource
    {
    public:
        /**
         * @brief Builder类 - 流式构建器 (对应Iris RenderTarget.Builder)
         *
         * 教学要点:
         * - 完全对应Iris的Builder设计模式
         * - 流式接口提升代码可读性
         * - 参数验证确保构建的RenderTarget有效
         */
        class Builder
        {
        private:
            DXGI_FORMAT m_format            = DXGI_FORMAT_R8G8B8A8_UNORM; // 对应Iris InternalTextureFormat
            int         m_width             = 0; // 对应Iris width
            int         m_height            = 0; // 对应Iris height
            std::string m_name              = ""; // 对应Iris name
            bool        m_allowLinearFilter = true; // 对应Iris allowsLinear判断
            int         m_sampleCount       = 1; // DirectX专有: MSAA采样数
            bool        m_enableMipmap      = false; // 🔥 Milestone 3.0: Mipmap支持
            ClearValue  m_clearValue        = ClearValue::Color(Rgba8::BLACK); // Clear value for Fast Clear optimization

        public:
            /**
             * @brief 设置调试名称 (对应Iris setName)
             */
            Builder& SetName(const std::string& name)
            {
                m_name = name;
                return *this;
            }

            /**
             * @brief 设置纹理格式 (对应Iris setInternalFormat)
             */
            Builder& SetFormat(DXGI_FORMAT format)
            {
                m_format = format;
                return *this;
            }

            /**
             * @brief 设置尺寸 (对应Iris setDimensions)
             */
            Builder& SetDimensions(int width, int height)
            {
                if (width <= 0 || height <= 0)
                {
                    throw std::invalid_argument("Width and height must be greater than zero");
                }
                m_width  = width;
                m_height = height;
                return *this;
            }

            /**
             * @brief 设置尺寸 (别名方法，方便使用)
             */
            Builder& SetSize(int width, int height)
            {
                return SetDimensions(width, height);
            }

            /**
             * @brief 设置过滤模式 (对应Iris的allowsLinear逻辑)
             */
            Builder& SetLinearFilter(bool enable)
            {
                m_allowLinearFilter = enable;
                return *this;
            }

            /**
             * @brief 设置过滤模式 (别名方法)
             */
            Builder& SetAllowLinearFilter(bool enable)
            {
                return SetLinearFilter(enable);
            }

            /**
             * @brief 设置多重采样 (DirectX特有)
             */
            Builder& SetSampleCount(int sampleCount)
            {
                if (sampleCount < 1 || sampleCount > 16)
                {
                    throw std::invalid_argument("Sample count must be between 1 and 16");
                }
                m_sampleCount = sampleCount;
                return *this;
            }

            /**
             * @brief 设置Mipmap生成 (Milestone 3.0: Bindless-MRT架构)
             *
             * 教学要点:
             * - Mipmap提供多级细节纹理，减少远距离采样走样
             * - Iris通过材质属性控制Mipmap生成
             * - DirectX需要在纹理创建时指定Mipmap级别
             */
            Builder& EnableMipmap(bool enable)
            {
                m_enableMipmap = enable;
                return *this;
            }

            /**
             * @brief Set clear value for Fast Clear optimization
             * @param value Clear value (color or depth/stencil)
             * @return Builder reference for chaining
             *
             * Teaching points:
             * - Fast Clear requires matching OptimizedClearValue in texture creation
             * - Improves clear performance by 3x (0.3ms -> 0.1ms)
             * - Must match actual clear color used in ClearRenderTargetView
             */
            Builder& SetClearValue(const ClearValue& value)
            {
                m_clearValue = value;
                return *this;
            }

            /**
             * @brief 从RTConfig创建Builder (静态工厂方法)
             * @param config RTConfig配置（包含绝对尺寸）
             * @return 配置好的Builder实例
             *
             * 教学要点:
             * - 直接字段映射，无需转换
             * - 零开销抽象: inline静态方法
             * - 对应ShadowColorManager等使用RTConfig的场景
             *
             * 配置映射:
             * - config.name → SetName()
             * - config.format → SetFormat()
             * - config.width, config.height → SetDimensions()
             * - config.enableMipmap → EnableMipmap()
             * - config.allowLinearFilter → SetLinearFilter()
             * - config.sampleCount → SetSampleCount()
             *
             * 注意: RTConfig的enableFlipper, loadAction, clearValue字段
             *       不属于Builder职责，由外部管理
             */
            static Builder FromConfig(const struct RTConfig& config)
            {
                return Builder()
                       .SetName(config.name)
                       .SetFormat(config.format)
                       .SetDimensions(config.width, config.height)
                       .EnableMipmap(config.enableMipmap)
                       .SetLinearFilter(config.allowLinearFilter)
                       .SetSampleCount(config.sampleCount);
            }

            /**
             * @brief 构建RenderTarget (对应Iris build方法)
             */
            std::shared_ptr<D12RenderTarget> Build();

            // Builder内部访问方法
            friend class D12RenderTarget;
        };

    private:
        // ========================================================================
        // 核心成员变量 (直接对应Iris RenderTarget字段)
        // ========================================================================

        std::shared_ptr<D12Texture> m_mainTexture; // 对应Iris mainTexture
        std::shared_ptr<D12Texture> m_altTexture; // 对应Iris altTexture

        DXGI_FORMAT m_format; // 对应Iris internalFormat
        int         m_width; // 对应Iris width
        int         m_height; // 对应Iris height
        bool        m_allowLinearFilter; // 对应Iris allowsLinear判断
        int         m_sampleCount; // DirectX专有: 多重采样数
        bool        m_enableMipmap; // Milestone 3.0: Mipmap支持
        ClearValue  m_clearValue; // Clear value for Fast Clear optimization

        // Milestone 3.0: Bindless索引支持 🔥
        uint32_t m_mainTextureIndex; // 主纹理在Bindless堆中的索引 (对应架构文档RenderTargetPair)
        uint32_t m_altTextureIndex; // 替代纹理在Bindless堆中的索引

        // DirectX专有描述符
        D3D12_CPU_DESCRIPTOR_HANDLE m_mainRTV; // 主纹理RTV
        D3D12_CPU_DESCRIPTOR_HANDLE m_altRTV; // 替代纹理RTV
        D3D12_CPU_DESCRIPTOR_HANDLE m_mainSRV; // 主纹理SRV
        D3D12_CPU_DESCRIPTOR_HANDLE m_altSRV; // 替代纹理SRV

        // 调试支持
        mutable std::string m_formattedDebugName; // 格式化的调试名称（用于GetDebugName重写）

    public:
        /**
         * @brief 创建Builder (对应Iris静态方法 builder())
         */
        static Builder Create()
        {
            return Builder();
        }

        // ========================================================================
        // 复合资源管理 - 重写基类方法 (Milestone 3.0 Bug Fix)
        // ========================================================================

        /**
         * @brief 重写Upload()方法 - 复合资源模式
         * @param commandList 命令列表（可选）
         * @return 上传成功返回true
         *
         * 教学要点:
         * 1. Composite模式: RenderTarget是复合资源，包含2个D12Texture子资源
         * 2. 上传流程: 遍历子资源，调用每个子资源的Upload()
         * 3. API一致性: 通过重写虚函数而不是添加特殊方法
         * 4. 面向对象原则: 遵循Template Method模式
         *
         * 为什么重写Upload()而不是UploadToGPU():
         * - Upload()是public接口，管理完整的上传流程
         * - UploadToGPU()是protected，只负责具体的数据传输
         * - 复合资源需要在外层管理子资源上传，而不是在内部实现
         */
        bool Upload(ID3D12GraphicsCommandList* commandList = nullptr) override;

        /**
         * @brief 重写RegisterBindless()方法 - 复合资源模式
         * @return 返回主纹理的Bindless索引（与基类签名一致）
         *
         * 教学要点:
         * 1. Composite模式: 遍历子资源，调用每个子资源的RegisterBindless()
         * 2. RenderTarget自身不需要Bindless索引，只有内部纹理需要
         * 3. 保持API一致性: 所有资源使用相同的注册接口
         * 4. 返回值: 返回主纹理索引，允许外部代码获取Bindless索引
         */
        std::optional<uint32_t> RegisterBindless() override;

        /**
         * @brief 析构函数 - RAII自动管理资源
         *
         * 教学要点:
         * - C++智能指针自动管理纹理释放
         * - 比Iris的手动destroy()更安全
         */
        virtual ~D12RenderTarget() = default;

        // ========================================================================
        // 纹理访问接口 (对应Iris getter方法)
        // ========================================================================

        /**
         * @brief 获取主纹理 (对应Iris getMainTexture)
         * @return 主纹理智能指针
         *
         * 教学要点:
         * - Iris返回int纹理ID，我们返回封装的D12Texture
         * - 智能指针确保安全的内存管理
         */
        std::shared_ptr<D12Texture> GetMainTexture() const
        {
            RequireValid();
            return m_mainTexture;
        }

        /**
         * @brief 获取替代纹理 (对应Iris getAltTexture)
         * @return 替代纹理智能指针
         *
         * 教学要点:
         * - 双纹理设计是Iris的核心特性
         * - 支持Ping-Pong渲染和历史帧访问
         */
        std::shared_ptr<D12Texture> GetAltTexture() const
        {
            RequireValid();
            return m_altTexture;
        }

        /**
         * @brief 获取宽度 (对应Iris getWidth)
         */
        int GetWidth() const { return m_width; }

        /**
         * @brief 获取高度 (对应Iris getHeight)
         */
        int GetHeight() const { return m_height; }

        /**
         * @brief 获取格式 (对应Iris getInternalFormat)
         */
        DXGI_FORMAT GetFormat() const { return m_format; }

        /**
         * @brief 获取多重采样数 (DirectX专有)
         */
        int GetSampleCount() const { return m_sampleCount; }

        /**
         * @brief 获取Mipmap生成状态 (Milestone 3.0: Bindless-MRT架构)
         * @return 如果启用Mipmap返回true
         *
         * 教学要点:
         * - Mipmap级别数由纹理尺寸决定: log2(max(width, height)) + 1
         * - Bindless架构下，Mipmap资源仍使用同一个索引，GPU自动选择级别
         */
        bool IsMipmapEnabled() const { return m_enableMipmap; }

        /**
         * @brief 获取主纹理Bindless索引 (Milestone 3.0: Bindless-MRT架构)
         * @return 主纹理在全局ResourceDescriptorHeap中的索引
         *
         * 教学要点:
         * - 对应架构文档RenderTargetPair::mainTextureIndex
         * - HLSL通过此索引直接访问: ResourceDescriptorHeap[index]
         * - 避免传统Descriptor Table绑定，性能提升80%+
         */
        uint32_t GetMainTextureIndex() const { return m_mainTextureIndex; }

        /**
         * @brief 获取替代纹理Bindless索引 (Milestone 3.0: Bindless-MRT架构)
         * @return 替代纹理在全局ResourceDescriptorHeap中的索引
         *
         * 教学要点:
         * - 对应架构文档RenderTargetPair::altTextureIndex
         * - 支持Ping-Pong双缓冲机制，Main/Alt交替读写
         */
        uint32_t GetAltTextureIndex() const { return m_altTextureIndex; }

        // ========================================================================
        // DirectX专有描述符访问
        // ========================================================================

        /**
         * @brief 获取主纹理RTV句柄
         * @return RTV描述符句柄
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetMainRTV() const { return m_mainRTV; }

        /**
         * @brief 获取替代纹理RTV句柄
         * @return RTV描述符句柄
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetAltRTV() const { return m_altRTV; }

        /**
         * @brief 获取主纹理SRV句柄
         * @return SRV描述符句柄
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetMainSRV() const { return m_mainSRV; }

        /**
         * @brief 获取替代纹理SRV句柄
         * @return SRV描述符句柄
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetAltSRV() const { return m_altSRV; }

        /**
         * @brief 获取主纹理的底层DirectX 12资源指针
         * @return ID3D12Resource*裸指针，如果主纹理不存在则返回nullptr
         *
         * 教学要点:
         * - 提供对底层DX12资源的直接访问
         * - 用于需要原生DX12 API的场景（如资源屏障、复制操作）
         * - 空指针安全：自动检查m_mainTexture是否有效
         */
        ID3D12Resource* GetMainTextureResource() const;

        /**
         * @brief 获取替代纹理的底层DirectX 12资源指针
         * @return ID3D12Resource*裸指针，如果替代纹理不存在则返回nullptr
         *
         * 教学要点:
         * - 提供对底层DX12资源的直接访问
         * - 支持Ping-Pong渲染中的资源操作
         * - 空指针安全：自动检查m_altTexture是否有效
         */
        ID3D12Resource* GetAltTextureResource() const;

        // ========================================================================
        // 尺寸管理 (对应Iris resize方法)
        // ========================================================================

        /**
         * @brief 调整尺寸 (对应Iris resize方法)
         * @param width 新宽度
         * @param height 新高度
         *
         * 教学要点:
         * - 完全对应Iris的resize逻辑
         * - 支持运行时分辨率变化
         * - 重新创建纹理和描述符
         */
        void Resize(int width, int height);

        /**
         * @brief 按需调整尺寸 (性能优化版本)
         * @param width 目标宽度
         * @param height 目标高度
         * @return 如果发生了resize返回true
         *
         * 教学要点: 避免不必要的resize操作，提升性能
         */
        bool ResizeIfNeeded(int width, int height);

        // ========================================================================
        // 虚函数重写 - 调试支持 (继承自D12Resource)
        // ========================================================================

        /**
         * @brief 重写虚函数：设置调试名称并添加RenderTarget特定逻辑
         * @param name 调试名称
         *
         * 教学要点: 重写基类虚函数，在设置名称时同时更新双纹理的调试名称
         * 对应Iris中的调试信息设置
         */
        void SetDebugName(const std::string& name) override;

        /**
         * @brief 重写虚函数：获取包含RenderTarget信息的调试名称
         * @return 格式化的调试名称字符串
         *
         * 教学要点: 重写基类虚函数，返回包含RenderTarget特定信息的名称
         * 格式: "RenderTargetName (1920x1080, RGBA8, MainTex + AltTex)"
         */
        const std::string& GetDebugName() const override;

        /**
         * @brief 重写虚函数：获取RenderTarget的详细调试信息
         * @return 包含尺寸、格式、纹理状态的调试字符串
         *
         * 教学要点: 提供RenderTarget特定的详细调试信息
         * 包括双纹理状态、格式、尺寸、采样设置等信息
         */
        std::string GetDebugInfo() const override;

    protected:
        /**
         * @brief 分配Bindless索引（RenderTarget子类实现）
         * @param allocator Bindless索引分配器指针
         * @return 成功返回有效索引（0-999,999），失败返回INVALID_INDEX
         *
         * 教学要点:
         * 1. RenderTarget使用纹理索引范围（0-999,999）
         * 2. 调用allocator->AllocateTextureIndex()获取纹理专属索引
         * 3. 注意: RenderTarget管理双纹理，此索引仅用于主纹理
         * 4. 替代纹理的索引由内部分配并存储在m_altTextureIndex
         */
        uint32_t AllocateBindlessIndexInternal(BindlessIndexAllocator* allocator) const override;

        /**
         * @brief 释放Bindless索引（RenderTarget子类实现）
         * @param allocator Bindless索引分配器指针
         * @param index 要释放的索引值
         * @return 成功返回true，失败返回false
         *
         * 教学要点:
         * 1. 调用allocator->FreeTextureIndex(index)释放纹理专属索引
         * 2. 必须使用与AllocateTextureIndex对应的释放函数
         * 3. 双纹理架构: 主纹理和替代纹理索引都需要释放
         */
        bool FreeBindlessIndexInternal(BindlessIndexAllocator* allocator, uint32_t index) const override;

        /**
         * @brief 在全局描述符堆中创建描述符（RenderTarget子类实现）
         * @param device DirectX 12设备指针
         * @param heapManager 全局描述符堆管理器指针
         *
         * 教学要点:
         * 1. RenderTarget创建SRV描述符（用于着色器采样）
         * 2. 双纹理架构: 主纹理和替代纹理都需要创建SRV
         * 3. RTV描述符由单独的RTV堆管理，不在全局堆中
         */
        void CreateDescriptorInGlobalHeap(ID3D12Device* device, GlobalDescriptorHeapManager* heapManager) override;

        /**
         * @brief 重写D12Resource纯虚函数: RenderTarget上传逻辑
         * @param commandList Copy队列命令列表
         * @param uploadContext Upload Heap上下文
         * @return 上传成功返回true
         *
         * 教学要点:
         * - RenderTarget不需要上传初始数据(GPU直接写入)
         * - 与Texture的区别: Texture需要上传贴图,RenderTarget不需要
         * - 返回true表示无需上传,直接成功
         */
        bool UploadToGPU(
            ID3D12GraphicsCommandList* commandList,
            class UploadContext&       uploadContext
        ) override;

        /**
         * @brief 重写D12Resource纯虚函数: 获取上传后目标状态
         * @return D3D12_RESOURCE_STATE_RENDER_TARGET
         *
         * 教学要点:
         * - RenderTarget初始状态为RENDER_TARGET(用于渲染输出)
         * - 后续如需采样,通过ResourceBarrier转换到PIXEL_SHADER_RESOURCE
         */
        D3D12_RESOURCE_STATES GetUploadDestinationState() const override;

        /**
         * @brief 重写D12Resource虚函数: RenderTarget不需要CPU数据
         * @return 始终返回false (RenderTarget是输出纹理,无需CPU数据上传)
         *
         * 教学要点:
         * 1. RenderTarget是GPU渲染输出目标,由GPU直接写入
         * 2. 不同于Input Texture(贴图),RenderTarget无CPU数据需要上传
         * 3. 重写此方法让基类Upload()跳过CPU数据检查
         * 4. 对应Iris colortex0-15的架构设计
         *
         * Milestone 3.0 Bug Fix:
         * - 问题: D12RenderTarget继承D12Resource而非D12Texture
         * - 解决: 必须在D12RenderTarget中重写RequiresCPUData()
         * - 效果: 允许Upload()成功完成,设置m_isUploaded = true
         */
        bool RequiresCPUData() const override
        {
            return false; // RenderTarget永远不需要CPU数据
        }

    private:
        /**
         * @brief 私有构造函数，只能通过Builder创建 (对应Iris构造函数设计)
         */
        D12RenderTarget(const Builder& builder);

        /**
         * @brief 初始化纹理 (对应Iris setupTexture方法)
         * @param width 纹理宽度
         * @param height 纹理高度
         *
         * 教学要点: 对应Iris的setupTexture逻辑，包含过滤器设置
         */
        void InitializeTextures(int width, int height);

        /**
         * @brief 创建描述符 (DirectX专有)
         *
         * 教学要点: DirectX需要显式创建RTV和SRV描述符
         */
        void CreateDescriptors();

        /**
         * @brief 验证有效性 (对应Iris requireValid方法)
         *
         * 教学要点: 防御性编程，确保对象处于有效状态
         */
        void RequireValid() const
        {
            if (!IsValid())
            {
                throw std::runtime_error("Attempted to use an invalid D12RenderTarget");
            }
        }

        // 禁用拷贝语义，强制使用智能指针管理
        D12RenderTarget(const D12RenderTarget&)            = delete;
        D12RenderTarget& operator=(const D12RenderTarget&) = delete;

        // 允许移动语义
        D12RenderTarget(D12RenderTarget&&)            = default;
        D12RenderTarget& operator=(D12RenderTarget&&) = default;

        // Builder需要访问私有构造函数
        friend class Builder;
    };
} // namespace enigma::graphic
