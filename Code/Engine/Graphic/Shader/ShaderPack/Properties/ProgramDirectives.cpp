/**
 * @file ProgramDirectives.cpp
 * @brief 单个Shader Program的渲染指令数据容器实现
 * @date 2025-10-13
 * @author Caizii
 *
 * 实现说明:
 * 本文件实现 ProgramDirectives 的构造函数和内部解析方法。
 * 构造函数调用 CommentDirectiveParser 解析所有注释指令，并存储结果。
 *
 * 教学要点:
 * - 依赖倒置原则 (DIP): 依赖 CommentDirectiveParser 的抽象接口 (静态方法)
 * - 构造时解析 (Construction-time Parsing): 所有解析在构造函数中完成
 * - 数据转换: 将 CommentDirective 转换为类型安全的数据成员
 * - 默认值处理: 对未指定的指令应用默认值
 */

#include "ProgramDirectives.hpp"
#include "Engine/Graphic/Shader/ShaderPack/Parsing/CommentDirectiveParser.hpp"
#include "Engine/Graphic/Shader/ShaderPack/ShaderSource.hpp"
#include <algorithm>
#include <cctype>

namespace enigma::graphic::shader
{
    /**
     * @brief 默认构造函数实现 - 应用默认配置
     *
     * 创建一个使用默认值的 ProgramDirectives 对象。
     * 不解析任何着色器源码，直接应用 Iris 兼容的默认值。
     *
     * 教学要点:
     * - 延迟初始化: 允许先构造，后赋值
     * - 默认值语义: drawBuffers = {0} (只输出到 RT0)
     * - 解决编译错误: 提供默认构造函数，使 ShaderSource 和 BuildResult 可编译
     *
     * 对应 Iris 行为:
     * - Iris 中，未指定 DRAWBUFFERS 时默认为 {0}
     * - 其他指令未指定时，使用 Optional.empty()
     *
     * 修复的编译错误:
     * - Error C2512: 'ProgramDirectives': no appropriate default constructor available
     * - Error C2280: BuildResult::BuildResult(void)': attempting to reference a deleted function
     */
    ProgramDirectives::ProgramDirectives()
    {
        // 应用默认值（不解析任何着色器源码）
        ApplyDefaults();
    }

    ProgramDirectives::ProgramDirectives(const enigma::graphic::ShaderSource& source)
    {
        // 1. 获取片段着色器源码
        // 注释指令通常出现在片段着色器中 (对应 Iris fragmentSource)
        if (source.GetPixelSource().empty())
        {
            // 如果没有片段着色器源码，应用默认值并返回
            ApplyDefaults();
            return;
        }

        const std::string& fragmentSource = source.GetPixelSource();

        // 2. 解析注释指令
        ParseCommentDirectives(fragmentSource);

        // 3. 应用默认值 (对未指定的指令)
        ApplyDefaults();
    }

    void ProgramDirectives::ParseCommentDirectives(const std::string& fragmentSource)
    {
        // ========================================================================
        // 解析 DRAWBUFFERS 指令
        // ========================================================================

        auto drawBuffersDirective = CommentDirectiveParser::FindDirective(
            fragmentSource,
            CommentDirective::Type::DRAWBUFFERS
        );

        if (drawBuffersDirective)
        {
            // 解析 "01234567" → {0, 1, 2, 3, 4, 5, 6, 7}
            for (char c : drawBuffersDirective->value)
            {
                if (std::isdigit(c))
                {
                    m_drawBuffers.push_back(static_cast<uint32_t>(c - '0'));
                }
            }
        }

        // ========================================================================
        // 解析 RENDERTARGETS 指令 (替代 DRAWBUFFERS)
        // ========================================================================

        // 如果 DRAWBUFFERS 未指定，尝试 RENDERTARGETS
        if (m_drawBuffers.empty())
        {
            auto renderTargetsDirective = CommentDirectiveParser::FindDirective(
                fragmentSource,
                CommentDirective::Type::RENDERTARGETS
            );

            if (renderTargetsDirective)
            {
                // 解析 "0,1,2" → {0, 1, 2}
                std::string value = renderTargetsDirective->value;
                size_t      start = 0;
                size_t      end   = value.find(',');

                while (end != std::string::npos)
                {
                    std::string token = value.substr(start, end - start);
                    // 去除前后空格
                    token.erase(0, token.find_first_not_of(" \t\n\r"));
                    token.erase(token.find_last_not_of(" \t\n\r") + 1);

                    if (!token.empty() && std::isdigit(token[0]))
                    {
                        m_drawBuffers.push_back(static_cast<uint32_t>(std::stoi(token)));
                    }

                    start = end + 1;
                    end   = value.find(',', start);
                }

                // 处理最后一个 token
                std::string token = value.substr(start);
                token.erase(0, token.find_first_not_of(" \t\n\r"));
                token.erase(token.find_last_not_of(" \t\n\r") + 1);

                if (!token.empty() && std::isdigit(token[0]))
                {
                    m_drawBuffers.push_back(static_cast<uint32_t>(std::stoi(token)));
                }
            }
        }

        // ========================================================================
        // 解析 BLEND 指令
        // ========================================================================

        auto blendDirective = CommentDirectiveParser::FindDirective(
            fragmentSource,
            CommentDirective::Type::BLEND
        );

        if (blendDirective)
        {
            m_blendMode = blendDirective->value;
        }

        // ========================================================================
        // 解析 DEPTHTEST 指令
        // ========================================================================

        auto depthTestDirective = CommentDirectiveParser::FindDirective(
            fragmentSource,
            CommentDirective::Type::DEPTHTEST
        );

        if (depthTestDirective)
        {
            m_depthTest = depthTestDirective->value;
        }

        // ========================================================================
        // 解析 CULLFACE 指令
        // ========================================================================

        auto cullFaceDirective = CommentDirectiveParser::FindDirective(
            fragmentSource,
            CommentDirective::Type::CULLFACE
        );

        if (cullFaceDirective)
        {
            m_cullFace = cullFaceDirective->value;
        }

        // ========================================================================
        // 解析 DEPTHWRITE 指令
        // ========================================================================

        auto depthWriteDirective = CommentDirectiveParser::FindDirective(
            fragmentSource,
            CommentDirective::Type::DEPTHWRITE
        );

        if (depthWriteDirective)
        {
            std::string value = depthWriteDirective->value;
            // 转换为大写
            // 修复警告 C4244: '=': conversion from 'int' to 'char'
            // std::toupper 返回 int，需要显式转换为 char
            // 使用 lambda 表达式避免隐式转换警告
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            // 解析布尔值
            m_depthWrite = (value == "ON" || value == "TRUE");
        }

        // ========================================================================
        // 解析 ALPHATEST 指令
        // ========================================================================

        auto alphaTestDirective = CommentDirectiveParser::FindDirective(
            fragmentSource,
            CommentDirective::Type::ALPHATEST
        );

        if (alphaTestDirective)
        {
            try
            {
                m_alphaTest = std::stof(alphaTestDirective->value);
            }
            catch (...)
            {
                // 解析失败，忽略
            }
        }

        // ========================================================================
        // 解析 FORMAT 指令
        // ========================================================================

        auto formatDirective = CommentDirectiveParser::FindDirective(
            fragmentSource,
            CommentDirective::Type::FORMAT
        );

        if (formatDirective)
        {
            // 解析 "0:RGBA16F" → {["0"] = "RGBA16F"}
            std::string value    = formatDirective->value;
            size_t      colonPos = value.find(':');

            if (colonPos != std::string::npos)
            {
                std::string rtIndex = value.substr(0, colonPos);
                std::string format  = value.substr(colonPos + 1);

                // 去除前后空格
                rtIndex.erase(0, rtIndex.find_first_not_of(" \t\n\r"));
                rtIndex.erase(rtIndex.find_last_not_of(" \t\n\r") + 1);
                format.erase(0, format.find_first_not_of(" \t\n\r"));
                format.erase(format.find_last_not_of(" \t\n\r") + 1);

                if (!rtIndex.empty() && !format.empty())
                {
                    m_rtFormats[rtIndex] = format;
                }
            }
        }
    }

    void ProgramDirectives::ApplyDefaults()
    {
        // ========================================================================
        // 应用默认值
        // ========================================================================

        // 默认 DrawBuffers 为 {0} (只输出到 RT0)
        // 对应 Iris 的默认行为
        if (m_drawBuffers.empty())
        {
            m_drawBuffers.push_back(0);
        }

        // 其他指令使用 std::optional 的默认行为 (nullopt 表示未指定)
        // 这些指令没有默认值，由 PSO 创建逻辑决定最终行为
    }
} // namespace enigma::graphic::shader
