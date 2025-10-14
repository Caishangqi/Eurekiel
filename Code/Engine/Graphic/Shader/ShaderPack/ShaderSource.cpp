/**
 * @file ShaderSource.cpp
 * @brief 着色器源码容器实现
 * @date 2025-10-05
 *
 * 更新历史:
 * - 2025-10-05: 添加 Hull/Domain Shader 支持
 * - 2025-10-05: 添加 ProgramSet 反向引用
 * - 2025-10-14: 添加 HasNonEmptySource() 和行数统计方法 (Phase 4.4)
 */

#include "ShaderSource.hpp"
#include <algorithm> // std::any_of, std::count
#include <cctype>    // std::isspace

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
    // ❌ 不要在成员初始化列表中初始化 m_directives
    // 因为 ProgramDirectives 构造函数需要访问 *this，而此时对象还未完全构造
    // 正确做法：使用默认构造函数，然后在构造函数体内重新赋值
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
        // 解析 ProgramDirectives - 对应 Iris ProgramDirectives 构造
        // ========================================================================

        // 教学要点:
        // ProgramDirectives 构造函数会自动调用 CommentDirectiveParser 解析指令
        // 这里在所有成员都初始化完成后，再重新构造 m_directives
        //
        // 修复理由:
        // 1. 避免在成员初始化列表中使用 this 指针（未定义行为）
        // 2. 确保所有成员都已正确初始化后，再访问 *this
        // 3. 使用 默认构造 + 赋值 的两步初始化模式
        //
        // 对应编译错误:
        // - Error C2512: 'ProgramDirectives': no appropriate default constructor available
        //
        // 解决方案:
        // - 添加 ProgramDirectives 默认构造函数
        // - m_directives 先使用默认构造（成员初始化列表隐式调用）
        // - 然后在构造函数体内重新赋值为正确的值
        m_directives = shader::ProgramDirectives(*this);
    }

    // ========================================================================
    // Phase 4.4 - 三层验证方法实现
    // ========================================================================

    bool ShaderSource::HasNonEmptySource() const
    {
        // Lambda辅助函数：检查字符串是否包含非空白字符
        auto hasContent = [](const std::string& str) -> bool
        {
            return std::any_of(str.begin(), str.end(), [](unsigned char c)
            {
                return !std::isspace(c);
            });
        };

        // 两个着色器都必须有非空白内容
        return hasContent(m_vertexSource) && hasContent(m_pixelSource);
    }

    size_t ShaderSource::GetVertexLineCount() const
    {
        return std::count(m_vertexSource.begin(), m_vertexSource.end(), '\n');
    }

    size_t ShaderSource::GetPixelLineCount() const
    {
        return std::count(m_pixelSource.begin(), m_pixelSource.end(), '\n');
    }
} // namespace enigma::graphic
