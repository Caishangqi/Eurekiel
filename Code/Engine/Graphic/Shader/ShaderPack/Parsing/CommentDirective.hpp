/**
 * @file CommentDirective.hpp
 * @brief 着色器注释指令的中间数据结构
 * @date 2025-10-13
 * @author Caizii
 *
 * 架构说明:
 * CommentDirective 是 CommentDirectiveParser 和 ProgramDirectives 之间的"桥梁"。
 * 这是一个轻量级的中间数据结构，用于在解析器和数据容器之间传递解析结果。
 *
 * 职责边界:
 * - ✅ 存储解析后的单条指令数据 (类型 + 值 + 位置)
 * - ❌ 不包含任何解析逻辑 (由 CommentDirectiveParser 负责)
 * - ❌ 不做任何数据转换 (由 ProgramDirectives 负责)
 *
 * 对应 Iris:
 * - net.irisshaders.iris.shaderpack.parsing.CommentDirective
 *
 * 设计理念:
 * - 纯数据结构，没有任何方法
 * - 不可变设计 (所有字段都是 const 或 final)
 * - 生命周期：临时对象，解析完即销毁
 * - 类比: JSON 解析中的中间 JSON Object
 *
 * 教学要点:
 * - 单一职责原则 (SRP): 只负责数据传递
 * - 数据传输对象 (DTO) 模式
 * - 不可变对象设计 (Immutable Object Pattern)
 *
 * 典型用法:
 * @code
 * // CommentDirectiveParser 创建 CommentDirective
 * auto directive = CommentDirectiveParser::FindDirective(
 *     source,
 *     CommentDirective::Type::DRAWBUFFERS
 * );
 *
 * // ProgramDirectives 消费 CommentDirective
 * if (directive) {
 *     std::string value = directive->value;  // "01234567"
 *     size_t location = directive->location; // 指令在源码中的位置
 * }
 * @endcode
 */

#pragma once

#include <string>
#include <optional>

namespace enigma::graphic::shader
{
    /**
     * @brief 着色器注释指令的中间数据结构
     *
     * CommentDirective 表示从着色器注释中解析出的单条指令。
     * 这是 CommentDirectiveParser 的输出，ProgramDirectives 的输入。
     *
     * @note 对应 Iris 的 CommentDirective.java
     * @note 生命周期：临时对象，解析完即销毁
     *
     * 设计理念:
     * - 作为解析器和数据容器之间的"桥梁"
     * - 只包含原始解析结果，不做任何转换
     * - 不包含任何解析逻辑
     *
     * Iris 源码参考:
     * @code{.java}
     * public class CommentDirective {
     *     private final Type type;
     *     private final String directive;
     *     private final int location;
     *
     *     public enum Type {
     *         DRAWBUFFERS,
     *         RENDERTARGETS
     *     }
     * }
     * @endcode
     */
    struct CommentDirective
    {
        /**
         * @brief 指令类型枚举
         *
         * 对应 Iris 和 OptiFine 支持的所有注释指令类型。
         *
         * 指令格式:
         * @code{.glsl}
         *  DRAWBUFFERS:01234567        // 输出到哪些 RT
         *  RENDERTARGETS:0,1,2         // 同上，更现代的语法
         *  BLEND:ADD                   // 混合模式
         *  DEPTHTEST:GREATER           // 深度测试模式
         *  CULLFACE:BACK               // 面剔除模式
         *  DEPTHWRITE:OFF              // 深度写入开关
         *  ALPHATEST:0.5 /              // Alpha 测试阈值
         *  FORMAT:0:RGBA16F          // RT0 格式覆盖
         * @endcode
         */
        enum class Type
        {
            DRAWBUFFERS, ///< /* DRAWBUFFERS:01234567 */ - 指定片段着色器输出到哪些 RT
            RENDERTARGETS, ///< /* RENDERTARGETS:0,1,2 */ - 同 DRAWBUFFERS，更现代的语法
            BLEND, ///< /* BLEND:ADD */ - 混合模式 (ADD, MULTIPLY, SUBTRACT 等)
            DEPTHTEST, ///< /* DEPTHTEST:GREATER */ - 深度测试模式 (LESS, GREATER, EQUAL 等)
            CULLFACE, ///< /* CULLFACE:BACK */ - 面剔除模式 (NONE, FRONT, BACK)
            DEPTHWRITE, ///< /* DEPTHWRITE:OFF */ - 深度写入开关 (ON, OFF)
            ALPHATEST, ///< /* ALPHATEST:0.5 */ - Alpha 测试阈值 (0.0 - 1.0)
            FORMAT ///< /* FORMAT:0:RGBA16F */ - RT 格式覆盖 (索引:格式)
        };

        Type        type; ///< 指令类型
        std::string value; ///< 解析后的值 (例如: "01234567", "GREATER", "OFF")
        size_t      location; ///< 在源码中的位置 (字符偏移量，用于处理重复指令)

        /**
         * @brief 构造函数
         *
         * @param t 指令类型
         * @param v 解析后的值字符串
         * @param loc 指令在源码中的位置 (字符偏移量)
         *
         * @note 仅由 CommentDirectiveParser 调用
         *
         * 示例:
         * @code
         * // CommentDirectiveParser 解析 " DRAWBUFFERS:0123 " 后创建:
         * CommentDirective directive(
         *     CommentDirective::Type::DRAWBUFFERS,
         *     "0123",
         *     42  // 假设指令从源码第42字节开始
         * );
         * @endcode
         */
        CommentDirective(Type t, const std::string& v, size_t loc)
            : type(t), value(v), location(loc)
        {
        }
    };
} // namespace enigma::graphic::shader
