#pragma once

#include "../D12Resources.hpp"
#include <vector>
#include <d3d12.h>

namespace enigma::graphic
{
    // 前向声明
    class D12RenderTarget;
    class D12DepthTexture;

    /**
     * @brief D12Framebuffer类 - DirectX 12帧缓冲区封装
     *
     * 教学要点:
     * 1. FrameBuffer ≠ RenderTarget - FrameBuffer是渲染配置器，RenderTarget是纹理容器
     * 2. 对应Iris的GlFramebuffer - 组织多个RenderTarget进行渲染
     * 3. 支持多渲染目标(MRT) - 同时输出到多个colortex
     * 4. 管理颜色附件和深度附件的绑定关系
     *
     * 设计原则:
     * - 不拥有纹理资源，只引用和组织它们
     * - 提供DirectX 12的渲染目标配置功能
     * - 支持main/alt纹理的选择
     *
     * 对应Iris架构:
     * - GlFramebuffer.addColorAttachment() → AddColorAttachment()
     * - GlFramebuffer.addDepthAttachment() → AddDepthAttachment()
     * - GlFramebuffer.bind() → Bind()
     * - GlFramebuffer.drawBuffers() → SetDrawBuffers()
     */
    class D12Framebuffer : public D12Resource
    {
    public:
        /**
         * @brief 颜色附件信息
         */
        struct ColorAttachment
        {
            D12RenderTarget* renderTarget; // RenderTarget引用 (不拥有)
            bool             useAltTexture; // 是否使用alt纹理
            uint32_t         attachmentIndex; // 附件索引 (GL_COLOR_ATTACHMENT0 + index)
        };

    private:
        // ==================== 附件管理 ====================
        std::vector<ColorAttachment> m_colorAttachments; // 颜色附件列表
        D12DepthTexture*             m_depthAttachment; // 深度附件引用 (不拥有)

        // ==================== DirectX 12资源 ====================
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_rtvHandles; // 渲染目标视图句柄
        D3D12_CPU_DESCRIPTOR_HANDLE              m_dsvHandle; // 深度模板视图句柄
        bool                                     m_hasDSV; // 是否有深度附件

        // ==================== 绘制缓冲区配置 ====================
        std::vector<uint32_t> m_drawBuffers; // 绘制缓冲区索引 (对应Iris drawBuffers)
        uint32_t              m_maxDrawBuffers; // 最大绘制缓冲区数量
        uint32_t              m_maxColorAttachments; // 最大颜色附件数量

        // ==================== 状态管理 ====================
        bool        m_isDirty; // 是否需要重新配置
        std::string m_debugName; // 调试名称

    public:
        /**
         * @brief 构造函数
         * @param debugName 调试名称 (用于GPU调试)
         *
         * 教学要点: 获取硬件限制，初始化状态
         */
        D12Framebuffer(const std::string& debugName = "");

        /**
         * @brief 析构函数
         *
         * 教学要点: 清理资源引用，不拥有纹理所以不需要释放纹理
         */
        virtual ~D12Framebuffer();

        // ========================================================================
        // 附件管理接口 (对应Iris GlFramebuffer接口)
        // ========================================================================

        /**
         * @brief 添加颜色附件
         * @param index 附件索引 (对应GL_COLOR_ATTACHMENT0 + index)
         * @param renderTarget RenderTarget引用
         * @param useAlt 是否使用alt纹理 (默认使用main纹理)
         *
         * 教学要点:
         * - 对应Iris: framebuffer.addColorAttachment(index, texture)
         * - 支持main/alt纹理选择 (Iris特色功能)
         * - 验证索引范围和硬件限制
         *
         * @note 不拥有RenderTarget，只保存引用
         */
        void AddColorAttachment(uint32_t index, D12RenderTarget* renderTarget, bool useAlt = false);

        /**
         * @brief 添加深度附件
         * @param depthTexture 深度纹理引用
         *
         * 教学要点:
         * - 对应Iris: framebuffer.addDepthAttachment(texture)
         * - 只能有一个深度附件
         * - 自动检测深度格式 (DEPTH24_STENCIL8 或 DEPTH32F_STENCIL8)
         *
         * @note 不拥有DepthTexture，只保存引用
         */
        void AddDepthAttachment(D12DepthTexture* depthTexture);

        /**
         * @brief 移除颜色附件
         * @param index 附件索引
         *
         * 教学要点: 动态修改framebuffer配置
         */
        void RemoveColorAttachment(uint32_t index);

        /**
         * @brief 移除深度附件
         *
         * 教学要点: 某些pass不需要深度测试
         */
        void RemoveDepthAttachment();

        // ========================================================================
        // 绘制缓冲区配置 (对应Iris drawBuffers功能)
        // ========================================================================

        /**
         * @brief 设置绘制缓冲区
         * @param buffers 绘制缓冲区索引数组
         *
         * 教学要点:
         * - 对应Iris: framebuffer.drawBuffers(int[] buffers)
         * - 控制fragment shader输出到哪些RT
         * - 支持MRT (Multiple Render Targets)
         *
         * 示例: SetDrawBuffers({0, 1, 2}) → 输出到colortex0, colortex1, colortex2
         */
        void SetDrawBuffers(const std::vector<uint32_t>& buffers);

        /**
         * @brief 禁用所有绘制缓冲区
         *
         * 教学要点:
         * - 对应Iris: framebuffer.noDrawBuffers()
         * - 用于只需要深度写入的pass (如shadow pass)
         */
        void NoDrawBuffers();

        // ========================================================================
        // 渲染操作接口
        // ========================================================================

        /**
         * @brief 绑定framebuffer为渲染目标
         *
         * 教学要点:
         * - 对应Iris: framebuffer.bind()
         * - 配置GPU渲染管线的输出目标
         * - 使用D3D12RenderSystem的抽象接口，避免直接暴露DirectX对象
         *
         * 架构设计:
         * - 内部通过D3D12RenderSystem::GetCurrentCommandList()获取命令列表
         * - 调用OMSetRenderTargets设置RTV和DSV
         * - 根据drawBuffers配置输出目标
         *
         * Bindless兼容性:
         * - 当前实现传统绑定模式，为Bindless架构预留扩展空间
         * - 未来可扩展为返回Bindless索引: GetBindlessRTVIndices()
         */
        void Bind();

        /**
         * @brief 绑定为读取源framebuffer
         *
         * 教学要点:
         * - 对应Iris: framebuffer.bindAsReadBuffer()
         * - 用于纹理拷贝操作的源
         * - 通过D3D12RenderSystem抽象，避免直接操作命令列表
         *
         * 架构设计:
         * - 内部通过D3D12RenderSystem处理资源状态转换
         * - 自动处理RENDER_TARGET -> SHADER_RESOURCE状态转换
         *
         * Bindless兼容性:
         * - 传统模式：配置拷贝操作的源缓冲区
         * - Bindless模式：未来可改为返回SRV索引数组
         */
        void BindAsReadBuffer();

        /**
         * @brief 绑定为写入目标framebuffer
         *
         * 教学要点:
         * - 对应Iris: framebuffer.bindAsDrawBuffer()
         * - 用于纹理拷贝操作的目标
         * - 通过D3D12RenderSystem抽象，避免直接操作命令列表
         *
         * 架构设计:
         * - 内部通过D3D12RenderSystem处理资源状态转换
         * - 自动处理SHADER_RESOURCE -> RENDER_TARGET状态转换
         *
         * Bindless兼容性:
         * - 传统模式：配置拷贝操作的目标缓冲区
         * - Bindless模式：未来可改为返回RTV索引数组
         */
        void BindAsDrawBuffer();

        // ========================================================================
        // 状态查询接口
        // ========================================================================

        /**
         * @brief 检查framebuffer完整性
         * @return framebuffer状态
         *
         * 教学要点:
         * - 对应Iris: framebuffer.getStatus()
         * - 验证所有附件格式兼容性
         * - 检查硬件限制
         */
        bool IsComplete() const;

        /**
         * @brief 获取颜色附件
         * @param index 附件索引
         * @return 颜色附件信息，nullptr表示不存在
         *
         * 教学要点: 对应Iris getColorAttachment功能
         */
        const ColorAttachment* GetColorAttachment(uint32_t index) const;

        /**
         * @brief 是否有深度附件
         * @return true表示有深度附件
         *
         * 教学要点: 对应Iris hasDepthAttachment功能
         */
        bool HasDepthAttachment() const { return m_depthAttachment != nullptr; }

        /**
         * @brief 获取附件数量
         */
        uint32_t GetColorAttachmentCount() const { return static_cast<uint32_t>(m_colorAttachments.size()); }

        /**
         * @brief 获取调试名称
         */
        const std::string& GetDebugName() const { return m_debugName; }

    private:
        /**
         * @brief 更新RTV句柄数组
         *
         * 教学要点: 根据当前附件配置更新DirectX 12描述符句柄
         */
        void UpdateRTVHandles();

        /**
         * @brief 验证附件索引有效性
         * @param index 附件索引
         * @return true表示有效
         *
         * 教学要点: 硬件限制检查
         */
        bool ValidateAttachmentIndex(uint32_t index) const;

        /**
         * @brief 验证绘制缓冲区配置
         * @param buffers 绘制缓冲区数组
         * @return true表示有效
         *
         * 教学要点:
         * - 检查是否超过硬件限制
         * - 验证索引是否对应已添加的附件
         */
        bool ValidateDrawBuffers(const std::vector<uint32_t>& buffers) const;

        /**
         * @brief 销毁内部资源
         *
         * 教学要点: 实现D12Resource的纯虚函数
         */
        virtual void destroyInternal() override;

        // ========================================================================
        // 静态工厂方法 (使用D3D12RenderSystem抽象)
        // ========================================================================
    public:
        /**
         * @brief 创建基础framebuffer
         * @param debugName 调试名称
         * @return 创建的framebuffer
         *
         * 教学要点: 使用D3D12RenderSystem而不是直接调用DirectX API
         */
        static std::unique_ptr<D12Framebuffer> Create(const std::string& debugName = "");

        /**
         * @brief 创建用于GBuffer的framebuffer
         * @param colorTargets 颜色目标数组
         * @param depthTarget 深度目标
         * @param stageWritesToAlt 使用alt纹理的阶段集合
         * @return 创建的GBuffer framebuffer
         *
         * 教学要点:
         * - 对应RenderTargets.CreateGbufferFramebuffer
         * - 自动配置多渲染目标
         * - 支持main/alt纹理选择
         */
        static std::unique_ptr<D12Framebuffer> CreateForGBuffer(
            const std::vector<D12RenderTarget*>& colorTargets,
            D12DepthTexture*                     depthTarget,
            const std::vector<uint32_t>&         stageWritesToAlt = {});
    };

    // ========================================================================
    // 辅助类型定义
    // ========================================================================

    /**
     * @brief FrameBuffer智能指针类型
     *
     * 教学要点: 现代C++内存管理最佳实践
     */
    using D12FramebufferPtr = std::unique_ptr<D12Framebuffer>;
} // namespace enigma::graphic
