/**
 * @file ShaderProgram.hpp
 * @brief 着色器程序 - 对应 Iris Program.java
 * @date 2025-10-03
 *
 * 职责:
 * 1. 持有 Root Signature (引用全局 Bindless Root Signature)
 * 2. 持有编译后的 CompiledShader
 * 3. 提供 Use() 方法设置 Root Signature
 * 4. PSO 由 PSOManager 统一管理
 *
 * 对应 Iris:
 * - Program.java (持有 OpenGL Program ID)
 * - GlResource (管理 OpenGL 对象生命周期)
 *
 * 设计决策:
 * - ❌ 不继承 D12Resource (PSO/Root Signature 不是 ID3D12Resource)
 * - + 使用 ComPtr 管理 DirectX 对象生命周期 (RAII)
 * - + 符合 Iris 架构 - Program 持有已编译的 OpenGL 程序
 */

#pragma once

#include "Engine/Graphic/Resource/CompiledShader.hpp"
#include "Engine/Graphic/Shader/Program/Properties/ProgramDirectives.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <optional>
#include <memory>

namespace enigma::graphic
{
    /**
     * @class ShaderProgram
     * @brief 着色器程序 - 持有编译后的着色器和Root Signature
     *
     * 教学要点:
     * - 对应 Iris Program.java
     * - PSO 由 PSOManager 统一管理
     * - 不继承 D12Resource (不是 GPU 资源)
     * - 使用 RAII 管理 DirectX 对象
     *
     * 架构对应:
     * ```
     * Iris                    DirectX 12
     * ----------------------------------------
     * int glProgramId    →    PSOManager管理的PSO
     * glUseProgram(id)   →    SetGraphicsRootSignature
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
        friend class PSOManager; // 允许PSOManager访问私有成员以创建PSO

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
         * @param directives 程序指令 (从 ShaderSource 解析)
         * @return 成功返回 true
         *
         * 教学要点:
         * 1. 存储编译后的着色器
         * 2. 获取全局 Bindless Root Signature
         * 3. 存储 ProgramDirectives (供 PSOManager 使用)
         * 4. PSO 创建由 PSOManager 负责
         */
        bool Create(
            CompiledShader&&                 vertexShader,
            CompiledShader&&                 pixelShader,
            std::optional<CompiledShader>&&  geometryShader,
            ShaderType                       type,
            const shader::ProgramDirectives& directives
        );

        /**
         * @brief 激活此着色器程序 (对应 Iris Program.use())
         * @param commandList 命令列表
         *
         * 教学要点:
         * - 对应 OpenGL glUseProgram(id)
         * - 仅设置 Root Signature
         * - PSO 由 PSOManager 在渲染时设置
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
        const shader::ProgramDirectives& GetDirectives() const { return m_directives; }

        /**
         * @brief 获取程序名称
         * @return 程序名称字符串
         */
        const std::string& GetName() const { return m_name; }

        /**
         * @brief 检查程序是否有效
         * @return 有效返回 true
         */
        bool IsValid() const { return m_rootSignature != nullptr; }

        /**
         * @brief 检查是否有几何着色器
         * @return 有几何着色器返回 true
         */
        bool HasGeometryShader() const { return m_geometryShader.has_value(); }

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
        // DirectX 12 对象
        // ========================================================================

        ID3D12RootSignature* m_rootSignature = nullptr; ///< Root Signature (裸指针，不持有所有权)
        // 注意: PSO 由 PSOManager 统一管理，不在此持有

        // ========================================================================
        // Iris 注释配置
        // ========================================================================

        shader::ProgramDirectives m_directives; ///< 合并后的着色器指令

        // ========================================================================
        // 注意: 不继承 D12Resource
        // ========================================================================
        // 原因:
        // 1. ShaderProgram 不持有 PSO (由 PSOManager 管理)
        // 2. D12Resource 管理纹理/缓冲区,不管理着色器程序
        // 3. 符合 Iris 架构 - Program 也不继承资源基类
    };
} // namespace enigma::graphic
