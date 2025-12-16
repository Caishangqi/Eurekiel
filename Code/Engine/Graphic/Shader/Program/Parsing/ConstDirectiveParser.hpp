#pragma once

#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include <string>
#include <unordered_map>
#include <optional>
#include <variant>
#include <vector>

/**
 * @file ConstDirectiveParser.hpp
 * @brief 着色器const常量指令解析器
 *
 * **功能概述**:
 * 解析着色器文件中的const常量定义，例如：
 * - `const int shadowMapResolution = 2048;`
 * - `const float sunPathRotation = 0.0f;`
 * - `const bool shadowHardwareFiltering = true;`
 * - `const vec3 ambientColor = vec3(0.8, 0.9, 1.0);`
 *
 * **Iris源码参考**:
 * - OptionAnnotatedSource.java::parseConst() - 核心const解析逻辑（第202-309行）
 * - 支持int, float, bool三种标量类型
 * - Iris不支持向量类型const（见第219行诊断信息）
 * - 本实现扩展支持vec2/3/4, ivec2/3等向量类型，用于DirectX 12项目特殊需求
 *
 * **设计原则**:
 * - KISS原则 - 简单的字符串解析，不使用复杂正则表达式
 * - SOLID原则 - 单一职责（只负责const常量解析）
 * - 教学导向 - 详细注释说明每个步骤的解析逻辑
 *
 * **支持的类型**:
 * 1. 标量类型（Iris兼容）:
 *    - int, float, bool
 * 2. 向量类型（DirectX 12扩展）:
 *    - vec2, vec3, vec4 (映射到项目的Vec2, Vec3, Vec4)
 *    - ivec2, ivec3, ivec4 (映射到IntVec2, IntVec3, IntVec4)
 *
 * **解析格式示例**:
 * ```hlsl
 * const int shadowMapResolution = 2048;
 * const float sunPathRotation = 0.0f;
 * const bool shadowHardwareFiltering = true;
 * const vec3 ambientColor = vec3(0.8, 0.9, 1.0);
 * const ivec2 atlasSize = ivec2(1024, 1024);
 * ```
 *
 * **错误处理**:
 * - 解析失败时返回false，不抛出异常
 * - 类型不匹配时GetXXX()方法返回std::nullopt
 * - 保证鲁棒性，不因为单个常量解析失败而中断整个文件的解析
 *
 * @author Iris源码参考 + DirectX 12项目扩展
 * @date 2025-10-13
 */

namespace enigma::graphic
{
    /**
     * @brief 常量值类型（使用std::variant存储多种类型）
     *
     * **设计说明**:
     * - 使用std::variant而非继承体系，符合现代C++最佳实践
     * - 避免虚函数开销，提供编译期类型安全
     * - 支持std::get<T>()和std::holds_alternative<T>()进行类型查询
     */
    using ConstantValue = std::variant<
        int,
        float,
        bool,
        Vec2,
        Vec3,
        Vec4,
        IntVec2,
        IntVec3
    >;

    /**
     * @brief ConstDirectiveParser - const常量指令解析器
     *
     * **核心职责**:
     * 1. 解析单行const指令 - Parse()
     * 2. 批量解析多行代码 - ParseLines()
     * 3. 查询常量值 - GetInt(), GetFloat(), GetVec3()等
     * 4. 管理常量存储 - 内部使用std::unordered_map
     *
     * **线程安全性**:
     * - 非线程安全，需要外部同步
     * - 解析阶段和查询阶段应分离（先Parse完成，再Query）
     *
     * **性能考虑**:
     * - 解析时一次性构建哈希表，查询时O(1)复杂度
     * - 不缓存解析结果，每次Parse()都重新解析（保证简单性）
     */
    class ConstDirectiveParser
    {
    public:
        /**
         * @brief 默认构造函数
         */
        ConstDirectiveParser() = default;

        // ========================================================================
        // 解析接口
        // ========================================================================

        /**
         * @brief 解析单行const指令
         * @param line 源代码行（例如："const int shadowMapResolution = 2048;"）
         * @return true 如果解析成功，false 如果不是const指令或解析失败
         *
         * **解析逻辑**（基于Iris OptionAnnotatedSource.java::parseConst()）:
         * 1. 检查行是否以"const"开头（Trim后）
         * 2. 提取类型（int, float, bool, vec2等）
         * 3. 提取变量名
         * 4. 提取值并解析
         * 5. 存储到内部映射表
         *
         * **Iris兼容性**:
         * - 完全兼容Iris的int/float/bool常量格式
         * - 扩展支持向量类型（Iris不支持）
         *
         * **示例**:
         * ```cpp
         * ConstDirectiveParser parser;
         * parser.Parse("const int shadowMapResolution = 2048;"); // true
         * parser.Parse("const vec3 color = vec3(1.0, 0.5, 0.2);"); // true
         * parser.Parse("#define SHADOWS"); // false (不是const)
         * ```
         */
        bool Parse(const std::string& line);

        /**
         * @brief 批量解析多行代码
         * @param lines 源代码行列表
         * @return 成功解析的常量数量
         *
         * **使用场景**:
         * - 解析整个着色器文件的所有const常量
         * - 与ShaderPackLoader配合使用
         *
         * **示例**:
         * ```cpp
         * std::vector<std::string> lines = {
         *     "const int shadowMapResolution = 2048;",
         *     "const float sunPathRotation = 0.0f;",
         *     "// 注释行",
         *     "const bool shadowHardwareFiltering = true;"
         * };
         * size_t count = parser.ParseLines(lines); // 返回3
         * ```
         */
        size_t ParseLines(const std::vector<std::string>& lines);

        // ========================================================================
        // 查询接口 - 获取常量值
        // ========================================================================

        /**
         * @brief 获取int常量
         * @param name 常量名
         * @return 常量值（如果存在且类型匹配）
         *
         * **类型安全**:
         * - 只有当常量存在且类型为int时，才返回值
         * - 类型不匹配时返回std::nullopt
         */
        std::optional<int> GetInt(const std::string& name) const;

        /**
         * @brief 获取float常量
         */
        std::optional<float> GetFloat(const std::string& name) const;

        /**
         * @brief 获取bool常量
         */
        std::optional<bool> GetBool(const std::string& name) const;

        /**
         * @brief 获取Vec2常量
         */
        std::optional<Vec2> GetVec2(const std::string& name) const;

        /**
         * @brief 获取Vec3常量
         */
        std::optional<Vec3> GetVec3(const std::string& name) const;

        /**
         * @brief 获取Vec4常量
         */
        std::optional<Vec4> GetVec4(const std::string& name) const;

        /**
         * @brief 获取IntVec2常量
         */
        std::optional<IntVec2> GetIntVec2(const std::string& name) const;

        /**
         * @brief 获取IntVec3常量
         */
        std::optional<IntVec3> GetIntVec3(const std::string& name) const;

        /**
         * @brief 检查常量是否存在
         * @param name 常量名
         * @return true 如果常量存在（不检查类型）
         */
        bool HasConstant(const std::string& name) const;

        /**
         * @brief 获取所有常量名称
         * @return 常量名称列表
         *
         * **使用场景**:
         * - 调试和诊断
         * - 生成常量列表文档
         */
        std::vector<std::string> GetAllConstantNames() const;

        /**
         * @brief 清空所有常量
         */
        void Clear();

        /**
         * @brief 获取常量总数
         * @return 存储的常量数量
         */
        size_t GetConstantCount() const { return m_constants.size(); }

    private:
        // ========================================================================
        // 内部解析辅助函数
        // ========================================================================

        /**
         * @brief 解析int常量
         * @param name 常量名
         * @param valueStr 值字符串（例如："2048"）
         * @return true 如果解析成功
         *
         * **实现细节**:
         * - 使用std::stoi()进行解析
         * - 捕获异常以处理无效格式
         */
        bool ParseInt(const std::string& name, const std::string& valueStr);

        /**
         * @brief 解析float常量
         * @param name 常量名
         * @param valueStr 值字符串（例如："0.5f" 或 "0.5"）
         *
         * **实现细节**:
         * - 自动移除'f'或'F'后缀（HLSL风格）
         * - 使用std::stof()进行解析
         */
        bool ParseFloat(const std::string& name, const std::string& valueStr);

        /**
         * @brief 解析bool常量
         * @param name 常量名
         * @param valueStr 值字符串（例如："true" 或 "false"）
         *
         * **Iris兼容性**:
         * - 与Iris完全一致，只接受"true"和"false"字符串
         * - 不接受"1"和"0"（与C++标准一致）
         */
        bool ParseBool(const std::string& name, const std::string& valueStr);

        /**
         * @brief 解析vec2常量
         * @param name 常量名
         * @param valueStr 值字符串（例如："vec2(0.5, 0.8)"）
         */
        bool ParseVec2(const std::string& name, const std::string& valueStr);

        /**
         * @brief 解析vec3常量
         * @param name 常量名
         * @param valueStr 值字符串（例如："vec3(0.8, 0.9, 1.0)"）
         */
        bool ParseVec3(const std::string& name, const std::string& valueStr);

        /**
         * @brief 解析vec4常量
         */
        bool ParseVec4(const std::string& name, const std::string& valueStr);

        /**
         * @brief 解析ivec2常量
         */
        bool ParseIntVec2(const std::string& name, const std::string& valueStr);

        /**
         * @brief 解析ivec3常量
         */
        bool ParseIntVec3(const std::string& name, const std::string& valueStr);

        /**
         * @brief 提取向量构造函数内的值
         * @param valueStr 值字符串（例如："vec3(0.8, 0.9, 1.0)"）
         * @return 值列表（例如：["0.8", "0.9", "1.0"]）
         *
         * **解析逻辑**:
         * 1. 查找'('和')'
         * 2. 提取括号内的内容
         * 3. 按','分割
         * 4. Trim每个值
         */
        std::vector<std::string> ExtractVectorValues(const std::string& valueStr);

        /**
         * @brief 去除字符串首尾空白
         * @param str 输入字符串
         * @return 去除空白后的字符串
         *
         * **实现细节**:
         * - 去除空格、制表符、换行符等所有空白字符
         * - 使用std::isspace()判断
         */
        std::string Trim(const std::string& str) const;

    private:
        // ========================================================================
        // 内部数据成员
        // ========================================================================

        /**
         * @brief 常量存储（名称 -> 值）
         *
         * **设计选择**:
         * - 使用std::unordered_map提供O(1)查询性能
         * - Key为常量名（std::string）
         * - Value为ConstantValue（std::variant支持多种类型）
         *
         * **内存考虑**:
         * - 典型着色器包含10-50个常量，内存占用可忽略
         * - 不需要预留容量优化（KISS原则）
         */
        std::unordered_map<std::string, ConstantValue> m_constants;
    };
} // namespace enigma::graphic
