/**
 * @file ShaderSource.cpp
 * @brief 着色器源码容器实现
 * @date 2025-10-03
 */

#include "ShaderSource.hpp"

namespace enigma::graphic
{
    ShaderSource::ShaderSource(
        const std::string& name,
        const std::string& vertexSource,
        const std::string& pixelSource,
        const std::string& geometrySource,
        const std::string& computeSource
    )
        : m_name(name)
          , m_vertexSource(vertexSource)
          , m_pixelSource(pixelSource)
    {
        // 设置可选着色器源码
        if (!geometrySource.empty())
        {
            m_geometrySource = geometrySource;
        }

        if (!computeSource.empty())
        {
            m_computeSource = computeSource;
        }

        // 从像素着色器源码解析 ShaderDirectives
        // (Iris 的注释通常在片段着色器中)
        m_directives = ShaderDirectives::Parse(pixelSource);

        // 如果像素着色器没有指令，尝试从顶点着色器解析
        if (m_directives.renderTargets.empty() && m_directives.drawBuffers.empty())
        {
            ShaderDirectives vertexDirectives = ShaderDirectives::Parse(vertexSource);
            if (!vertexDirectives.renderTargets.empty() || !vertexDirectives.drawBuffers.empty())
            {
                m_directives = vertexDirectives;
            }
        }
    }
} // namespace enigma::graphic
