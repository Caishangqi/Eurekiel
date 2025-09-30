#pragma once

#include "../D12Resources.hpp"
#include "../Texture/D12Texture.hpp"
#include <memory>
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <stdexcept>

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
             * @brief 设置过滤模式 (对应Iris的allowsLinear逻辑)
             */
            Builder& SetLinearFilter(bool enable)
            {
                m_allowLinearFilter = enable;
                return *this;
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
         * @brief 获取RenderTarget的默认Bindless资源类型
         * @return RenderTarget作为可读取纹理返回BindlessResourceType::Texture2D
         *
         * 教学要点: RenderTarget的主要纹理作为可采样资源使用Texture2D类型
         * 对应Iris中将RenderTarget绑定为GL_TEXTURE_2D供着色器采样
         */
        BindlessResourceType GetDefaultBindlessResourceType() const override;

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

    // ============================================================================
    // 📖 使用示例 (对应Iris使用模式)
    // ============================================================================
    /*

    // Iris Java代码风格:
    // RenderTarget colorTarget = RenderTarget.builder()
    //     .setName("colorTarget0")
    //     .setInternalFormat(InternalTextureFormat.RGBA8)
    //     .setDimensions(1920, 1080)
    //     .build();

    // 对应的DirectX 12 C++代码:
    auto colorTarget = D12RenderTarget::Create()
        .SetName("colorTarget0")
        .SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM)
        .SetDimensions(1920, 1080)
        .SetLinearFilter(true)
        .Build();

    // 访问纹理 (对应Iris getMainTexture()):
    auto mainTex = colorTarget->GetMainTexture();
    auto altTex = colorTarget->GetAltTexture();

    // 调整尺寸 (对应Iris resize):
    colorTarget->ResizeIfNeeded(2560, 1440);

    */
} // namespace enigma::graphic
