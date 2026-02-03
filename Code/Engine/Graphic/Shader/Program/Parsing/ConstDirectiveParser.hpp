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
     * @brief constant value type (use std::variant to store multiple types)
     *
     * **Design Description**:
     * - Use std::variant instead of inheritance system, in line with modern C++ best practices
     * - Avoid virtual function overhead and provide compile-time type safety
     * - Support std::get<T>() and std::holds_alternative<T>() for type query
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
     * @brief ConstDirectiveParser - const constant directive parser
     *
     * **Core Responsibilities**:
     * 1. Parse single-line const instructions - Parse()
     * 2. Parse multiple lines of code in batches - ParseLines()
     * 3. Query constant values - GetInt(), GetFloat(), GetVec3(), etc.
     * 4. Manage constant storage - internally use std::unordered_map
     *
     * **Thread Safety**:
     * - Not thread safe, requires external synchronization
     * - The parsing phase and the query phase should be separated (Parse is completed first, then Query)
     *
     * **Performance Considerations**:
     * - Construct the hash table once during parsing, O(1) complexity during query
     * - Do not cache the parsing results, re-parse each time Parse() (ensuring simplicity)
     */
    class ConstDirectiveParser
    {
    public:
        ConstDirectiveParser() = default;

        // ========================================================================
        // parse interface
        // ========================================================================

        /**
         * @brief parses single-line const instructions
         * @param line source code line (for example: "const int shadowMapResolution = 2048;")
         * @return true if parsing is successful, false if it is not a const directive or parsing fails
         *
         * **Parse logic** (based on Iris OptionAnnotatedSource.java::parseConst()):
         * 1. Check whether the line starts with "const" (after Trim)
         * 2. Extraction type (int, float, bool, vec2, etc.)
         * 3. Extract variable name
         * 4. Extract the value and parse it
         * 5. Store in internal mapping table
         *
         * **Iris Compatibility**:
         * - Fully compatible with Iris' int/float/bool constant format
         * - Extended support for vector types (not supported by Iris)
         *
         * **Example**:
         * ```cpp
         * ConstDirectiveParser parser;
         * parser.Parse("const int shadowMapResolution = 2048;"); // true
         * parser.Parse("const vec3 color = vec3(1.0, 0.5, 0.2);"); // true
         * parser.Parse("#define SHADOWS"); // false (not const)
         * ```
         */
        bool Parse(const std::string& line);

        /**
          * @brief Batch parsing of multiple lines of code
          * @param lines source code line list
          * @return The number of successfully parsed constants
          *
          * **Usage Scenario**:
          * - parse all const constants throughout the shader file
          * - Used with ShaderPackLoader
          *
          * **Example**:
          * ```cpp
          * std::vector<std::string> lines = {
          * "const int shadowMapResolution = 2048;",
          * "const float sunPathRotation = 0.0f;",
          * "//Comment line",
          * "const bool shadowHardwareFiltering = true;"
          * };
          * size_t count = parser.ParseLines(lines); // Return 3
          * ```
          */
        size_t ParseLines(const std::vector<std::string>& lines);

        // ========================================================================
        // Query interface - get constant value
        // ========================================================================
        /**
         * @brief Get int constant
         * @param name constant name
         * @return constant value (if exists and type matches)
         *
         * **Type safety**:
         * - Return a value only if the constant exists and is of type int
         * - Returns std::nullopt if type does not match
         */
        std::optional<int> GetInt(const std::string& name) const;

        /**
          * @brief Get float constant
          */
        std::optional<float> GetFloat(const std::string& name) const;

        /**
                 * @brief Get bool constant
                 */
        std::optional<bool> GetBool(const std::string& name) const;

        /**
                 * @brief Get Vec2 constant
                 */
        std::optional<Vec2> GetVec2(const std::string& name) const;

        /**
         * @brief Get Vec3 constant
         */
        std::optional<Vec3> GetVec3(const std::string& name) const;

        /**
         * @brief Get Vec4 constants
         */
        std::optional<Vec4> GetVec4(const std::string& name) const;

        /**
         * @brief Get IntVec2 constant
         */
        std::optional<IntVec2> GetIntVec2(const std::string& name) const;

        /**
         * @brief Get IntVec3 constant
         */
        std::optional<IntVec3> GetIntVec3(const std::string& name) const;

        /**
         * @brief Check whether the constant exists
         * @param name constant name
         * @return true if constant exists (does not check type)
         */
        bool HasConstant(const std::string& name) const;

        /**
         * @brief Get all constant names
         * @return constant name list
         *
         * **Usage Scenario**:
         * - Debugging and Diagnostics
         * - Generate constant list document
         */
        std::vector<std::string> GetAllConstantNames() const;

        /**
         * @brief Clear all constants
         */
        void Clear();

        /**
         * @brief Get the total number of constants
         * @return the number of stored constants
         */
        size_t GetConstantCount() const { return m_constants.size(); }

    private:
        /**
                 * @brief parse int constant
                 * @param name constant name
                 * @param valueStr value string (for example: "2048")
                 * @return true if parsing is successful
                 *
                 * **Implementation details**:
                 * - Use std::stoi() for parsing
                 * - Catch exceptions to handle invalid formats
                 */
        bool ParseInt(const std::string& name, const std::string& valueStr);

        /**
         * @brief parse float constants
         * @param name constant name
         * @param valueStr value string (for example: "0.5f" or "0.5")
         *
         * **Implementation details**:
         * - Automatically remove 'f' or 'F' suffix (HLSL style)
         * - Use std::stof() for parsing
         */
        bool ParseFloat(const std::string& name, const std::string& valueStr);

        /**
         * @brief parse bool constants
         * @param name constant name
         * @param valueStr value string (for example: "true" or "false")
         *
         * **Iris Compatibility**:
         * - exactly the same as Iris, only accepts "true" and "false" strings
         * - "1" and "0" are not accepted (consistent with the C++ standard)
         */
        bool ParseBool(const std::string& name, const std::string& valueStr);

        /**
         * @brief parse vec2 constant
         * @param name constant name
         * @param valueStr value string (for example: "vec2(0.5, 0.8)")
         */
        bool ParseVec2(const std::string& name, const std::string& valueStr);

        /**
         * @brief parse vec3 constants
         * @param name constant name
         * @param valueStr value string (for example: "vec3(0.8, 0.9, 1.0)")
         */
        bool ParseVec3(const std::string& name, const std::string& valueStr);

        /**
         * @brief parse vec4 constants
         */
        bool ParseVec4(const std::string& name, const std::string& valueStr);

        /**
         * @brief parse ivec2 constants
         */
        bool ParseIntVec2(const std::string& name, const std::string& valueStr);

        /**
         * @brief parse ivec3 constants
         */
        bool ParseIntVec3(const std::string& name, const std::string& valueStr);
        /**
                 * @brief Extract the value in the vector constructor
                 * @param valueStr value string (for example: "vec3(0.8, 0.9, 1.0)")
                 * @return value list (for example: ["0.8", "0.9", "1.0"])
                 *
                 * **Analysis logic**:
                 * 1. Find '(' and ')'
                 * 2. Extract the content within the brackets
                 * 3. Press ',' to split
                 * 4. Trim each value
                 */
        std::vector<std::string> ExtractVectorValues(const std::string& valueStr);

        /**
         * @brief Remove leading and trailing blanks from the string
         * @param str input string
         * @return the string after whitespace removal
         *
         * **Implementation details**:
         * - Remove all whitespace characters such as spaces, tabs, newlines, etc.
         * - Use std::isspace() to determine
         */
        std::string Trim(const std::string& str) const;

    private:
        /**
         * @brief constant storage (name -> value)
         *
         * **Design Choice**:
         * - Use std::unordered_map to provide O(1) query performance
         * - Key is a constant name (std::string)
         * - Value is ConstantValue (std::variant supports multiple types)
         *
         * **Memory Considerations**:
         * - A typical shader contains 10-50 constants and has a negligible memory footprint
         * - No reserved capacity optimization required (KISS principle)
         */
        std::unordered_map<std::string, ConstantValue> m_constants;
    };
} // namespace enigma::graphic
