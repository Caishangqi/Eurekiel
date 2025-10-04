/**
 * @file ShaderSource.hpp
 * @brief 着色器源码容器 - 对应 Iris ProgramSource
 * @date 2025-10-03
 *
 * 职责:
 * 1. 存储原始 HLSL 源码 (或 GLSL + 转换后的 HLSL)
 * 2. 存储解析后的 ShaderDirectives
 * 3. 提供验证方法
 *
 * 对应 Iris:
 * - ProgramSource.java
 * - 存储 vertex/fragment/geometry 等多个着色器源码
 */

#pragma once

#include "Engine/Graphic/Resource/ShaderDirectives.hpp"
#include "Engine/Graphic/Resource/CompiledShader.hpp"
#include <string>
#include <optional>

namespace enigma::graphic
{
    /**
     * @class ShaderSource
     * @brief 着色器源码容器
     *
     * 教学要点:
     * - 对应 Iris 的 ProgramSource.java
     * - 存储多个着色器阶段的源码 (vertex, pixel, geometry, etc.)
     * - 存储解析后的 ShaderDirectives
     * - 提供验证方法 (vertex + pixel 必须存在)
     *
     * 设计决策:
     * - 使用 std::optional 表示可选的着色器阶段
     * - ShaderDirectives 从源码中解析而来
     * - 支持热重载 (保留源码字符串)
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
         * @brief 构造函数 - 从源码字符串创建
         * @param name 着色器程序名称 (如 "gbuffers_terrain")
         * @param vertexSource 顶点着色器源码 (必需)
         * @param pixelSource 像素着色器源码 (必需)
         * @param geometrySource 几何着色器源码 (可选)
         * @param computeSource 计算着色器源码 (可选)
         *
         * 教学要点:
         * - 顶点和像素着色器是必需的
         * - 几何和计算着色器是可选的
         * - 从源码中自动解析 ShaderDirectives
         */
        ShaderSource(
            const std::string& name,
            const std::string& vertexSource,
            const std::string& pixelSource,
            const std::string& geometrySource = "",
            const std::string& computeSource  = ""
        );

        // ========================================================================
        // Getter 方法
        // ========================================================================

        const std::string&                GetName() const { return m_name; }
        const std::string&                GetVertexSource() const { return m_vertexSource; }
        const std::string&                GetPixelSource() const { return m_pixelSource; }
        const std::optional<std::string>& GetGeometrySource() const { return m_geometrySource; }
        const std::optional<std::string>& GetComputeSource() const { return m_computeSource; }
        const ShaderDirectives&           GetDirectives() const { return m_directives; }

        /**
         * @brief 检查是否有几何着色器
         */
        bool HasGeometryShader() const { return m_geometrySource.has_value(); }

        /**
         * @brief 检查是否有计算着色器
         */
        bool HasComputeShader() const { return m_computeSource.has_value(); }

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
         */
        const ShaderSource* RequireValid() const
        {
            return IsValid() ? this : nullptr;
        }

    private:
        std::string m_name; ///< 着色器程序名称

        // 着色器源码 (必需)
        std::string m_vertexSource; ///< 顶点着色器源码
        std::string m_pixelSource; ///< 像素着色器源码

        // 着色器源码 (可选)
        std::optional<std::string> m_geometrySource; ///< 几何着色器源码
        std::optional<std::string> m_computeSource; ///< 计算着色器源码

        // Iris 注释解析结果
        ShaderDirectives m_directives; ///< 解析的着色器指令
    };
} // namespace enigma::graphic
