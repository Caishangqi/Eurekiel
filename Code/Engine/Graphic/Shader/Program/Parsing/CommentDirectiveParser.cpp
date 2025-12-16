/**
 * @file CommentDirectiveParser.cpp
 * @brief 着色器注释指令解析器实现
 * @date 2025-10-13
 * @author Caizii
 *
 * 实现说明:
 * 本文件实现 CommentDirectiveParser 的所有静态解析方法。
 * 解析逻辑严格遵循 Iris 的实现，包括:
 * 1. 查找最后一次出现的指令 (lastIndexOf)
 * 2. 验证注释格式
 * 3. 提取 KEY:VALUE 中的 VALUE 部分
 * 4. 记录指令在源码中的位置 (用于处理重复指令)
 *
 * 教学要点:
 * - 纯函数式解析 (Pure Function): 无副作用，相同输入总是产生相同输出
 * - 字符串查找算法: rfind (从后往前查找，对应 Java lastIndexOf)
 * - 边界条件处理: 处理未找到、注释未闭合等异常情况
 * - 空白字符处理: 去除前后空格、换行符
 */

#include "CommentDirectiveParser.hpp"
#include <algorithm>
#include <cctype>

namespace enigma::graphic::shader
{
    std::optional<CommentDirective> CommentDirectiveParser::FindDirective(
        const std::string&     haystack,
        CommentDirective::Type type
    )
    {
        // 1. 获取指令前缀 (例如 "DRAWBUFFERS:", "DEPTHTEST:")
        std::string prefix = GetDirectivePrefix(type);
        if (prefix.empty())
        {
            return std::nullopt; // 未知的指令类型
        }

        // 2. 查找最后一次出现的指令前缀 (对应 Iris lastIndexOf)
        // 为什么从后往前查找？因为后面的指令优先级更高，会覆盖前面的指令
        size_t lastPrefixPos = haystack.rfind(prefix);
        if (lastPrefixPos == std::string::npos)
        {
            return std::nullopt; // 未找到指令
        }

        // 3. 从指令前缀位置往回查找注释起始位置 "/*"
        // 这是为了验证指令确实在注释中，而不是在字符串字面量或其他位置
        size_t commentStart = haystack.rfind("/*", lastPrefixPos);
        if (commentStart == std::string::npos)
        {
            return std::nullopt; // 指令不在有效的注释中
        }

        // 4. 查找注释结束位置 "*/"
        size_t commentEnd = haystack.find("*/", lastPrefixPos);
        if (commentEnd == std::string::npos)
        {
            return std::nullopt; // 注释未闭合
        }

        // 5. 提取完整的注释字符串
        // 例如: "/* DRAWBUFFERS:01234567 */"
        std::string comment = haystack.substr(commentStart, commentEnd - commentStart + 2);

        // 6. 从注释中提取指令值
        // 例如: 从 "/* DRAWBUFFERS:01234567 */" 提取 "01234567"
        std::string value = ExtractValue(comment, prefix);
        if (value.empty())
        {
            return std::nullopt; // 指令值为空
        }

        // 7. 创建并返回 CommentDirective
        return CommentDirective(type, value, lastPrefixPos);
    }

    std::string CommentDirectiveParser::GetDirectivePrefix(CommentDirective::Type type)
    {
        // 将指令类型枚举转换为注释前缀字符串
        // 对应 Iris 的 Type.prefix 字段
        switch (type)
        {
        case CommentDirective::Type::DRAWBUFFERS:
            return "DRAWBUFFERS:";
        case CommentDirective::Type::RENDERTARGETS:
            return "RENDERTARGETS:";
        case CommentDirective::Type::BLEND:
            return "BLEND:";
        case CommentDirective::Type::DEPTHTEST:
            return "DEPTHTEST:";
        case CommentDirective::Type::CULLFACE:
            return "CULLFACE:";
        case CommentDirective::Type::DEPTHWRITE:
            return "DEPTHWRITE:";
        case CommentDirective::Type::ALPHATEST:
            return "ALPHATEST:";
        case CommentDirective::Type::FORMAT:
            return "FORMAT:";
        default:
            return ""; // 未知类型
        }
    }

    std::string CommentDirectiveParser::ExtractValue(const std::string& comment, const std::string& prefix)
    {
        // 从注释字符串中提取指令值
        // 例如: 从 "/* DRAWBUFFERS:01234567 */" 提取 "01234567"

        // 1. 查找前缀位置
        size_t prefixPos = comment.find(prefix);
        if (prefixPos == std::string::npos)
        {
            return ""; // 前缀不存在 (理论上不应该发生)
        }

        // 2. 计算值的起始位置 (前缀后的第一个字符)
        size_t valueStart = prefixPos + prefix.length();

        // 3. 查找值的结束位置 (注释结束符 "*/" 的位置)
        size_t valueEnd = comment.find("*/", valueStart);
        if (valueEnd == std::string::npos)
        {
            valueEnd = comment.length(); // 如果没有结束符，使用字符串结尾
        }

        // 4. 提取值字符串
        std::string value = comment.substr(valueStart, valueEnd - valueStart);

        // 5. 去除前后空白字符 (空格、制表符、换行符、回车符)
        // 对应 Iris 的 String.trim()
        value.erase(0, value.find_first_not_of(" \t\n\r"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);

        // 6. 处理多个指令之间的分隔符 (空格)
        // 例如: "/* DRAWBUFFERS:01 RENDERTARGETS:2,3 */"
        // 我们只需要第一个空格之前的内容
        size_t spacePos = value.find(' ');
        if (spacePos != std::string::npos)
        {
            value = value.substr(0, spacePos);
        }

        return value;
    }
} // namespace enigma::graphic::shader
