/**
 * @file ShaderSource.cpp
 * @brief 着色器源码容器实现
 * @date 2025-10-05
 *
 * 更新历史:
 * - 2025-10-05: 添加 Hull/Domain Shader 支持
 * - 2025-10-05: 添加 ProgramSet 反向引用
 */

#include "ShaderSource.hpp"

namespace enigma::graphic
{
    ShaderSource::ShaderSource(
        const std::string& name,
        const std::string& vertexSource,
        const std::string& pixelSource,
        const std::string& geometrySource,
        const std::string& hullSource,
        const std::string& domainSource,
        const std::string& computeSource,
        ProgramSet*        parent
    )
        : m_name(name)
          , m_vertexSource(vertexSource)
          , m_pixelSource(pixelSource)
          , m_parent(parent)
    {
        // ========================================================================
        // 设置可选着色器源码
        // ========================================================================

        if (!geometrySource.empty())
        {
            m_geometrySource = geometrySource;
        }

        if (!hullSource.empty())
        {
            m_hullSource = hullSource;
        }

        if (!domainSource.empty())
        {
            m_domainSource = domainSource;
        }

        if (!computeSource.empty())
        {
            m_computeSource = computeSource;
        }

        // ========================================================================
        // 解析 ShaderDirectives - 对应 Iris ProgramDirectives 构造
        // ========================================================================

        // 教学要点:
        // Iris 的注释通常在片段着色器 (fragment/pixel) 中定义
        // 例如:
        //   /* RENDERTARGETS: 0,1,2 */
        //   /* DRAWBUFFERS: 012 */

        m_directives = ShaderDirectives::Parse(pixelSource);

        // 如果像素着色器没有指令，尝试从顶点着色器解析
        // (某些 Shader Pack 可能把注释放在顶点着色器)
        if (m_directives.renderTargets.empty() && m_directives.drawBuffers.empty())
        {
            ShaderDirectives vertexDirectives = ShaderDirectives::Parse(vertexSource);
            if (!vertexDirectives.renderTargets.empty() || !vertexDirectives.drawBuffers.empty())
            {
                m_directives = vertexDirectives;
            }
        }

        // 如果几何着色器存在，也尝试解析其指令
        if (m_geometrySource.has_value() && m_directives.renderTargets.empty())
        {
            ShaderDirectives geometryDirectives = ShaderDirectives::Parse(m_geometrySource.value());
            if (!geometryDirectives.renderTargets.empty() || !geometryDirectives.drawBuffers.empty())
            {
                m_directives = geometryDirectives;
            }
        }
    }
} // namespace enigma::graphic
