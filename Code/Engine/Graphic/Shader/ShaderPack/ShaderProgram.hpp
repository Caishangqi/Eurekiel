/**
 * @file ShaderProgram.hpp
 * @brief 着色器程序 - 对应 Iris Program.java
 * @date 2025-10-03
 *
 * 职责:
 * 1. 持有 PSO (ID3D12PipelineState) - 对应 OpenGL Program ID
 * 2. 持有 Root Signature (或引用全局 Bindless Root Signature)
 * 3. 持有编译后的 CompiledShader
 * 4. 提供 Use() 方法激活程序 (对应 glUseProgram)
 *
 * 对应 Iris:
 * - Program.java (持有 OpenGL Program ID)
 * - GlResource (管理 OpenGL 对象生命周期)
 *
 * 设计决策:
 * - ❌ 不继承 D12Resource (PSO/Root Signature 不是 ID3D12Resource)
 * - ✅ 使用 ComPtr 管理 DirectX 对象生命周期 (RAII)
 * - ✅ 符合 Iris 架构 - Program 持有已编译的 OpenGL 程序
 */

#pragma once

#include "Engine/Graphic/Resource/CompiledShader.hpp"
#include "Engine/Graphic/Resource/ShaderDirectives.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <optional>
#include <memory>

namespace enigma::graphic
{
    /**
     * @class ShaderProgram
     * @brief 完整的可执行着色器程序
     *
     * 教学要点:
     * - 对应 Iris Program.java
     * - 持有 PSO (对应 OpenGL Program ID)
     * - 不继承 D12Resource (PSO 不是 GPU 资源)
     * - 使用 RAII 管理 DirectX 对象
     *
     * 架构对应:
     * ```
     * Iris                    DirectX 12
     * ----------------------------------------
     * int glProgramId    →    ID3D12PipelineState*
     * glUseProgram(id)   →    SetPipelineState(pso)
     * ProgramUniforms    →    Root Constants / UniformBuffer
     * ProgramSamplers    →    Bindless 采样器索引
     * ```
     *
     * 与 D12Resource 的区别:
     * - D12Resource: 管理 ID3D12Resource* (纹理、缓冲区)
     * - ShaderProgram: 管理 ID3D12PipelineState* (不是资源)
     */
    class ShaderProgram
    {
    public:
        ShaderProgram() = default;
        ~ShaderProgram();

        // 禁用拷贝
        ShaderProgram(const ShaderProgram&)            = delete;
        ShaderProgram& operator=(const ShaderProgram&) = delete;

        // 支持移动
        ShaderProgram(ShaderProgram&&) noexcept            = default;
        ShaderProgram& operator=(ShaderProgram&&) noexcept = default;

        /**
         * @brief 从编译后的着色器创建程序
         * @param vertexShader 顶点着色器
         * @param pixelShader 像素着色器
         * @param geometryShader 几何着色器 (可选)
         * @param type 着色器类型
         * @return 成功返回 true
         *
         * 教学要点:
         * 1. 调用 D3D12RenderSystem::CreateGraphicsPSO() 创建 PSO
         * 2. 根据 ShaderDirectives 配置 PSO 描述符
         * 3. 使用全局 Bindless Root Signature
         * 4. 合并 VS 和 PS 的 ShaderDirectives
         */
        bool Create(
            CompiledShader&&                vertexShader,
            CompiledShader&&                pixelShader,
            std::optional<CompiledShader>&& geometryShader,
            ShaderType                      type
        );

        /**
         * @brief 激活此着色器程序 (对应 Iris Program.use())
         * @param commandList 命令列表
         *
         * 教学要点:
         * - 对应 OpenGL glUseProgram(id)
         * - 设置 PSO: commandList->SetPipelineState(m_pipelineState.Get())
         * - 设置 Root Signature: commandList->SetGraphicsRootSignature(...)
         *
         * Iris 对应:
         * ```java
         * public void use() {
         *     ProgramManager.glUseProgram(getGlId());
         *     uniforms.update();
         *     samplers.update();
         *     images.update();
         * }
         * ```
         */
        void Use(ID3D12GraphicsCommandList* commandList);

        /**
         * @brief 解绑当前着色器程序 (对应 Iris Program.unbind())
         * @param commandList 命令列表
         *
         * 教学要点:
         * - 对应 OpenGL glUseProgram(0)
         * - DirectX 12 中通常不需要显式解绑
         * - 保留此方法是为了与 Iris 架构一致
         */
        static void Unbind(ID3D12GraphicsCommandList* commandList);

        // ========================================================================
        // Getter 方法
        // ========================================================================

        /**
         * @brief 获取 PSO (对应 Iris getProgramId())
         * @return ID3D12PipelineState 指针
         */
        ID3D12PipelineState* GetPSO() const { return m_pipelineState.Get(); }

        /**
         * @brief 获取 Root Signature
         * @return ID3D12RootSignature 指针
         *
         * 教学要点:
         * - 不使用 ComPtr 管理全局 Root Signature (避免引用计数问题)
         * - 全局 Root Signature 由 D3D12RenderSystem 持有所有权
         * - ShaderProgram 只持有裸指针引用
         */
        ID3D12RootSignature* GetRootSignature() const { return m_rootSignature; }

        /**
         * @brief 获取着色器类型
         * @return ShaderType 枚举
         */
        ShaderType GetType() const { return m_type; }

        /**
         * @brief 获取着色器指令
         * @return ShaderDirectives 引用
         */
        const ShaderDirectives& GetDirectives() const { return m_directives; }

        /**
         * @brief 获取程序名称
         * @return 程序名称字符串
         */
        const std::string& GetName() const { return m_name; }

        /**
         * @brief 检查程序是否有效
         * @return 有效返回 true
         */
        bool IsValid() const { return m_pipelineState != nullptr; }

        /**
         * @brief 检查是否有几何着色器
         * @return 有几何着色器返回 true
         */
        bool HasGeometryShader() const { return m_geometryShader.has_value(); }

    private:
        /**
         * @brief 创建管线状态对象 (PSO)
         * @return 成功返回 true
         *
         * 教学要点:
         * 1. 根据 ShaderDirectives 配置 PSO 描述符
         * 2. 设置 Blend 模式 (根据 directives.blendMode)
         * 3. 设置 Depth Test (根据 directives.depthTest)
         * 4. 设置 Cull Mode (根据 directives.cullFace)
         * 5. 使用全局 Bindless Root Signature
         */
        bool CreatePipelineState();

        /**
         * @brief Configure PSO Blend State
         * @param blendDesc Blend state descriptor
         *
         * Teaching Points:
         * - Configure Blend mode based on Iris annotations
         * - Example: DRAWBUFFERS:012 BLEND:0:ADD configures RT0 as ADD blending
         */
        void ConfigureBlendState(D3D12_BLEND_DESC& blendDesc);

        /**
         * @brief Configure PSO Depth Stencil State
         * @param depthStencilDesc Depth stencil descriptor
         *
         * Teaching Points:
         * - Configure depth test based on Iris annotations
         * - Example: DEPTHTEST:LESS configures depth comparison function as LESS
         */
        void ConfigureDepthStencilState(D3D12_DEPTH_STENCIL_DESC& depthStencilDesc);

        /**
         * @brief Configure PSO Rasterizer State
         * @param rasterDesc Rasterizer descriptor
         *
         * Teaching Points:
         * - Configure cull mode based on Iris annotations
         * - Example: CULLFACE:BACK configures back-face culling
         */
        void ConfigureRasterizerState(D3D12_RASTERIZER_DESC& rasterDesc);

    private:
        std::string m_name; ///< 程序名称 (如 "gbuffers_terrain")
        ShaderType  m_type; ///< 着色器类型

        // ========================================================================
        // 编译后的着色器 (对应 Iris ProgramSource)
        // ========================================================================

        CompiledShader                m_vertexShader; ///< 顶点着色器 (必需)
        CompiledShader                m_pixelShader; ///< 像素着色器 (必需)
        std::optional<CompiledShader> m_geometryShader; ///< 几何着色器 (可选)

        // ========================================================================
        // DirectX 12 对象 (对应 Iris OpenGL Program ID)
        // ========================================================================

        Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState; ///< PSO (对应 glProgramId)
        ID3D12RootSignature*                        m_rootSignature = nullptr; ///< Root Signature (裸指针，不持有所有权)

        // ========================================================================
        // Iris 注释配置
        // ========================================================================

        ShaderDirectives m_directives; ///< 合并后的着色器指令

        // ========================================================================
        // 注意: 不继承 D12Resource
        // ========================================================================
        // 原因:
        // 1. PSO 不是 ID3D12Resource (不是 GPU 内存资源)
        // 2. D12Resource 管理纹理/缓冲区,不管理管线状态
        // 3. 符合 Iris 架构 - Program 也不继承资源基类
    };
} // namespace enigma::graphic
