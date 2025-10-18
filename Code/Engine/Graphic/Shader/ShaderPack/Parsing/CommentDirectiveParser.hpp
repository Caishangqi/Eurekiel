/**
 * @file CommentDirectiveParser.hpp
 * @brief 着色器注释指令解析器 (无状态，纯静态方法)
 * @date 2025-10-13
 * @author Caizii
 *
 * 架构说明:
 * CommentDirectiveParser 是一个 **无状态工具类**，负责从着色器源码字符串中查找并解析注释指令。
 * 它不存储任何状态，所有方法都是静态的，可以多次调用。
 *
 * 职责边界:
 * - + 解析字符串 → CommentDirective (中间数据)
 * - ❌ 不存储数据 (由 ProgramDirectives 负责)
 * - ❌ 不做数据转换 (由 ProgramDirectives 负责)
 *
 * 对应 Iris:
 * - net.irisshaders.iris.shaderpack.parsing.CommentDirectiveParser
 *
 * 设计理念:
 * - 无状态解析器：所有方法 static，不包含任何成员变量
 * - 单一职责：只负责字符串解析，不存储结果
 * - 可重用：可以被多次调用，每次返回独立的 CommentDirective
 * - 对齐 Iris：遵循 Iris 的 Parser-Container 分离原则
 *
 * 教学要点:
 * - 单一职责原则 (SRP): 只负责解析，不负责存储
 * - 无状态设计 (Stateless): 所有方法 static，无成员变量
 * - 纯函数式 (Pure Function): 相同输入总是产生相同输出，无副作用
 * - 工厂方法模式 (Factory Method): 通过静态方法创建 CommentDirective
 *
 * Iris 源码参考:
 * @code{.java}
 * public class CommentDirectiveParser {
 *     // 私有构造函数 - 不能被实例化
 *     private CommentDirectiveParser() {
 *         // cannot be constructed
 *     }
 *
 *     // 静态方法 - 无状态解析
 *     public static Optional<CommentDirective> findDirective(
 *         String haystack,
 *         CommentDirective.Type type
 *     ) {
 *         // 解析逻辑...
 *     }
 * }
 * @endcode
 *
 * 典型用法:
 * @code
 * std::string fragmentSource = " DRAWBUFFERS:01  void main() { ... }";
 *
 * // 查找 DRAWBUFFERS 指令
 * auto directive = CommentDirectiveParser::FindDirective(
 *     fragmentSource,
 *     CommentDirective::Type::DRAWBUFFERS
 * );
 *
 * if (directive) {
 *     std::string value = directive->value;  // "01"
 *     size_t location = directive->location; // 指令在源码中的位置
 * }
 * @endcode
 *
 * 错误类比:
 * ❌ 不要这样设计:
 * @code
 * class CommentDirectiveParser {
 *     std::string value;  // ❌ Parser 不应该存储数据
 *
 *     static CommentDirectiveParser Parse(const std::string& source) {
 *         CommentDirectiveParser result;
 *         result.value = extractData(source);  // ❌ 混合职责
 *         return result;
 *     }
 * };
 * @endcode
 *
 * + 正确设计:
 * @code
 * class CommentDirectiveParser {
 *     // 禁止实例化
 *     CommentDirectiveParser() = delete;
 *
 *     // 只有静态解析方法
 *     static std::optional<CommentDirective> FindDirective(...);
 * };
 * @endcode
 */

#pragma once

#include "CommentDirective.hpp"
#include <string>
#include <optional>

namespace enigma::graphic::shader
{
    /**
     * @brief 着色器注释指令解析器 (无状态，纯静态方法)
     *
     * CommentDirectiveParser 负责从着色器源码字符串中查找并解析注释指令。
     * 它不存储任何状态，所有方法都是静态的，可以多次调用。
     *
     * @note 对应 Iris 的 CommentDirectiveParser.java
     * @note 职责：解析字符串 → CommentDirective (中间数据)
     * @note 不负责：存储数据 (由 ProgramDirectives 负责)
     *
     * 设计理念:
     * - 无状态解析器：所有方法 static，不包含任何成员变量
     * - 单一职责：只负责字符串解析，不存储结果
     * - 可重用：可以被多次调用，每次返回独立的 CommentDirective
     * - 对齐 Iris：遵循 Iris 的 Parser-Container 分离原则
     *
     * 典型用法:
     * @code
     * std::string fragmentSource = " DRAWBUFFERS:01 void main() { ... }";
     * auto directive = CommentDirectiveParser::FindDirective(
     *     fragmentSource,
     *     CommentDirective::Type::DRAWBUFFERS
     * );
     * if (directive) {
     *     // 使用 directive->value == "01"
     * }
     * @endcode
     *
     * 类比理解:
     * - CommentDirectiveParser = JSON.parse() (解析器)
     * - CommentDirective = JSON 中间对象 (中间数据)
     * - ProgramDirectives = 你的数据类 (最终存储)
     *
     * 你不会让 JSON.parse() 存储数据，也不会让数据类去解析字符串。
     */
    class CommentDirectiveParser
    {
    public:
        // 禁止实例化 (纯静态工具类)
        CommentDirectiveParser()                                         = delete;
        CommentDirectiveParser(const CommentDirectiveParser&)            = delete;
        CommentDirectiveParser& operator=(const CommentDirectiveParser&) = delete;

        /**
         * @brief 在源码中查找指定类型的指令
         *
         * 从着色器源码中查找最后一次出现的指定指令。
         * 如果同一指令出现多次，返回最后一次出现的指令 (后面的优先级更高)。
         *
         * @param haystack 要搜索的源码字符串 (通常是 Fragment Shader 源码)
         * @param type 要查找的指令类型
         * @return std::optional<CommentDirective> 如果找到返回指令，否则返回空
         *
         * @note 对应 Iris 的 findDirective()
         *
         * 解析规则:
         * 1. 查找最后一次出现的指令 (lastIndexOf)
         * 2. 验证注释格式 
         * 3. 提取 `KEY:VALUE` 中的 VALUE 部分
         * 4. 记录指令在源码中的位置 (用于处理重复指令)
         *
         * 示例:
         * @code
         * // 源码中包含指令
         * std::string source = R"(
         *     DRAWBUFFERS:01234567 
         *     void main() {
         *         gl_FragData[0] = vec4(1.0);
         *     }
         * )";
         *
         * // 查找指令
         * auto directive = FindDirective(source, CommentDirective::Type::DRAWBUFFERS);
         *
         * // 使用结果
         * if (directive) {
         *     std::string value = directive->value;  // "01234567"
         *     size_t location = directive->location; // 指令在源码中的位置
         * }
         * @endcode
         *
         * @example 处理重复指令 (后面的优先级更高)
         * @code
         * std::string source = R"(
         *      DRAWBUFFERS:01 
         *     DRAWBUFFERS:0157   // 这个优先
         * )";
         *
         * auto directive = FindDirective(source, CommentDirective::Type::DRAWBUFFERS);
         * // directive->value == "0157" (最后一个)
         * @endcode
         */
        static std::optional<CommentDirective> FindDirective(
            const std::string&     haystack,
            CommentDirective::Type type
        );

    private:
        // 内部辅助方法

        /**
         * @brief 将指令类型转换为注释字符串前缀
         *
         * @param type 指令类型
         * @return std::string 注释前缀 (例如: "DRAWBUFFERS:", "DEPTHTEST:")
         *
         * 映射关系:
         * - DRAWBUFFERS  → "DRAWBUFFERS:"
         * - RENDERTARGETS → "RENDERTARGETS:"
         * - BLEND        → "BLEND:"
         * - DEPTHTEST    → "DEPTHTEST:"
         * - CULLFACE     → "CULLFACE:"
         * - DEPTHWRITE   → "DEPTHWRITE:"
         * - ALPHATEST    → "ALPHATEST:"
         * - FORMAT       → "FORMAT:"
         *
         * @note 内部方法，不对外暴露
         */
        static std::string GetDirectivePrefix(CommentDirective::Type type);

        /**
         * @brief 从注释字符串中提取值
         *
         * 从完整的注释字符串 (例如 " DRAWBUFFERS:01 ") 中提取指令值 (例如 "01")。
         *
         * @param comment 完整的注释字符串 (例如: " DRAWBUFFERS:01 ")
         * @param prefix 指令前缀 (例如: "DRAWBUFFERS:")
         * @return std::string 提取的值 (例如: "01")
         *
         * 提取规则:
         * 1. 查找前缀位置
         * 2. 提取前缀后的内容直到 "" 或字符串结束
         * 3. 去除前后空格和换行符
         * 4. 去除多个指令之间的分隔符 (空格)
         *
         * 示例:
         * @code
         * std::string comment = " DRAWBUFFERS:01234567";
         * std::string value = ExtractValue(comment, "DRAWBUFFERS:");
         * // value == "01234567"
         * @endcode
         *
         * @note 内部方法，不对外暴露
         */
        static std::string ExtractValue(const std::string& comment, const std::string& prefix);
    };
} // namespace enigma::graphic::shader
