/**
 * @file ShaderSource.hpp
 * @brief 着色器源码容器 - 对应 Iris ProgramSource
 * @date 2025-10-05
 *
 * 职责:
 * 1. 存储原始 HLSL 源码 (或 GLSL + 转换后的 HLSL)
 * 2. 存储解析后的 ShaderDirectives
 * 3. 提供验证方法
 * 4. 支持完整的 DirectX 12 着色器管线 (包括 Tessellation)
 *
 * 对应 Iris:
 * - ProgramSource.java
 * - 存储 vertex/fragment/geometry/tessControl/tessEval 等多个着色器源码
 *
 * 更新历史:
 * - 2025-10-05: 添加 Hull/Domain Shader 支持 (对应 Iris tessControl/tessEval)
 * - 2025-10-05: 添加 ProgramSet 反向引用 (对应 Iris parent 字段)
 */

#pragma once

#include "Engine/Graphic/Shader/ShaderPack/Properties/ProgramDirectives.hpp"
#include "Engine/Graphic/Resource/CompiledShader.hpp"
#include <string>
#include <optional>

namespace enigma::graphic
{
    // 前向声明
    class ProgramSet;

    /**
     * @class ShaderSource
     * @brief 着色器源码容器 - 完全对应 Iris ProgramSource
     *
     * 教学要点:
     * - 对应 Iris 的 ProgramSource.java
     * - 存储多个着色器阶段的源码 (vertex, pixel, geometry, hull, domain, compute)
     * - 存储解析后的 ShaderDirectives
     * - 提供验证方法 (vertex + pixel 必须存在)
     * - 支持 ProgramSet 反向引用
     *
     * 设计决策:
     * - 使用 std::optional 表示可选的着色器阶段
     * - ShaderDirectives 从源码中解析而来
     * - 支持热重载 (保留源码字符串)
     * - 支持 Tessellation (Hull/Domain Shader)
     *
     * DirectX 12 着色器管线:
     * - Vertex Shader (VS) - 必需 ⭐
     * - Pixel Shader (PS) - 必需 ⭐
     * - Geometry Shader (GS) - 可选
     * - Hull Shader (HS) - 可选 (对应 Iris tessControl)
     * - Domain Shader (DS) - 可选 (对应 Iris tessEval)
     * - Compute Shader (CS) - 可选 (独立管线)
     */
    class ShaderSource
    {
    public:
        ShaderSource()  = default;
        ~ShaderSource() = default;

        // 禁用拷贝
        ShaderSource(const ShaderSource&)            = delete;
        ShaderSource& operator=(const ShaderSource&) = delete;

        // 支持移动
        ShaderSource(ShaderSource&&) noexcept            = default;
        ShaderSource& operator=(ShaderSource&&) noexcept = default;

        /**
         * @brief 完整构造函数 - 对应 Iris ProgramSource 构造函数
         * @param name 着色器程序名称 (如 "gbuffers_terrain")
         * @param vertexSource 顶点着色器源码 (必需)
         * @param pixelSource 像素着色器源码 (必需)
         * @param geometrySource 几何着色器源码 (可选)
         * @param hullSource Hull 着色器源码 (可选, 对应 Iris tessControlSource)
         * @param domainSource Domain 着色器源码 (可选, 对应 Iris tessEvalSource)
         * @param computeSource 计算着色器源码 (可选)
         * @param parent 所属 ProgramSet (可选)
         *
         * 教学要点:
         * - 顶点和像素着色器是必需的
         * - 几何、Hull、Domain、计算着色器是可选的
         * - Hull 和 Domain 着色器必须成对出现 (Tessellation 管线)
         * - 从源码中自动解析 ShaderDirectives
         */
        ShaderSource(
            const std::string& name,
            const std::string& vertexSource,
            const std::string& pixelSource,
            const std::string& geometrySource = "",
            const std::string& hullSource     = "",
            const std::string& domainSource   = "",
            const std::string& computeSource  = "",
            ProgramSet*        parent         = nullptr
        );

        // ========================================================================
        // Getter 方法 - 对应 Iris ProgramSource 的 get 方法
        // ========================================================================

        const std::string& GetName() const { return m_name; }

        // 必需着色器
        const std::string& GetVertexSource() const { return m_vertexSource; }
        const std::string& GetPixelSource() const { return m_pixelSource; }

        // 可选着色器 - 使用 std::optional (对应 Iris Optional<String>)
        const std::optional<std::string>& GetGeometrySource() const { return m_geometrySource; }
        const std::optional<std::string>& GetHullSource() const { return m_hullSource; }
        const std::optional<std::string>& GetDomainSource() const { return m_domainSource; }
        const std::optional<std::string>& GetComputeSource() const { return m_computeSource; }

        // Directives 和 Parent
        const shader::ProgramDirectives& GetDirectives() const { return m_directives; }
        ProgramSet*                      GetParent() const { return m_parent; }

        // ========================================================================
        // 检查方法
        // ========================================================================

        /**
         * @brief 检查是否有几何着色器
         */
        bool HasGeometryShader() const { return m_geometrySource.has_value(); }

        /**
         * @brief 检查是否有 Hull 着色器 (Tessellation Control)
         */
        bool HasHullShader() const { return m_hullSource.has_value(); }

        /**
         * @brief 检查是否有 Domain 着色器 (Tessellation Evaluation)
         */
        bool HasDomainShader() const { return m_domainSource.has_value(); }

        /**
         * @brief 检查是否有计算着色器
         */
        bool HasComputeShader() const { return m_computeSource.has_value(); }

        /**
         * @brief 检查是否启用 Tessellation (Hull + Domain 成对出现)
         */
        bool HasTessellation() const
        {
            return m_hullSource.has_value() && m_domainSource.has_value();
        }

        /**
         * @brief 检查着色器源码是否有效
         * @return 如果有效返回 true
         *
         * 教学要点:
         * - 对应 Iris ProgramSource.isValid()
         * - 顶点和像素着色器必须都存在
         */
        bool IsValid() const
        {
            return !m_vertexSource.empty() && !m_pixelSource.empty();
        }

        /**
         * @brief 返回有效的着色器源码 (类似 Iris requireValid)
         * @return 如果有效返回 this，否则返回 nullptr
         *
         * 教学要点:
         * - 对应 Iris ProgramSource.requireValid()
         * - 返回 Optional.of(this) 或 Optional.empty()
         */
        const ShaderSource* RequireValid() const
        {
            return IsValid() ? this : nullptr;
        }

        // ========================================================================
        // Setter 方法 - 用于 ProgramSet 延迟设置
        // ========================================================================

        /**
         * @brief 设置 ProgramSet 父引用
         * @param parent 父 ProgramSet 指针
         *
         * 教学要点:
         * - Iris 在构造时传入 parent
         * - 我们在 ProgramSet 加载完成后设置
         */
        void SetParent(ProgramSet* parent) { m_parent = parent; }

    private:
        std::string m_name; ///< 着色器程序名称

        // 着色器源码 (必需)
        std::string m_vertexSource; ///< 顶点着色器源码
        std::string m_pixelSource; ///< 像素着色器源码

        // 着色器源码 (可选)
        std::optional<std::string> m_geometrySource; ///< 几何着色器源码
        std::optional<std::string> m_hullSource; ///< Hull 着色器源码 (对应 Iris tessControl)
        std::optional<std::string> m_domainSource; ///< Domain 着色器源码 (对应 Iris tessEval)
        std::optional<std::string> m_computeSource; ///< 计算着色器源码

        // Iris 注释解析结果
        shader::ProgramDirectives m_directives; ///< 解析的着色器指令

        // ProgramSet 反向引用 (对应 Iris parent 字段)
        ProgramSet* m_parent = nullptr; ///< 所属 ProgramSet (可为 nullptr)
    };
} // namespace enigma::graphic
