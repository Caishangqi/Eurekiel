#include "ConstDirectiveParser.hpp"
#include <algorithm>
#include <cctype>

/**
 * @file ConstDirectiveParser.cpp
 * @brief const constant instruction parser implementation
 *
 * **Implementation Reference**:
 * - Iris source code: OptionAnnotatedSource.java::parseConst() (lines 202-309)
 * - Core logic: Trim -> check "const" -> extract type -> extract name -> extract value -> parse and store
 *
 * **Key Design Decisions**:
 * 1. Use simple string operations and do not use regular expressions (KISS principle)
 * 2. Exception safety - all std::stoi/stof calls are wrapped in try-catch
 * 3. Type safety - use std::variant + std::holds_alternative to ensure correct type
 * 4. Robustness - failure to parse a single constant does not affect other constants
 *
 * @author Iris source code reference + DirectX 12 project implementation
 * @date 2025-10-13
 */

namespace enigma::graphic
{
    bool ConstDirectiveParser::Parse(const std::string& line)
    {
        // **Step 1: Trim first and last blank**
        //Purpose: Ignore indentation and trailing whitespace, unify format
        std::string trimmed = Trim(line);

        // **Step 2: Check if it starts with "const"**
        // Note: "const" contains spaces to avoid matching words such as "constant"
        if (trimmed.find("const ") != 0)
        {
            return false; // Not a const instruction
        }

        // **Step 3: Remove the "const" prefix**
        // "const " = 6 characters (including spaces)
        trimmed = trimmed.substr(6);
        trimmed = Trim(trimmed); // Remove extra whitespace after "const"

        // **Step 4: Extract type (first word)**
        // Find the first space, before the space is the type
        size_t spacePos = trimmed.find(' ');
        if (spacePos == std::string::npos)
        {
            return false; // Format error: no space separating type and name
        }

        std::string type = trimmed.substr(0, spacePos);
        trimmed          = trimmed.substr(spacePos + 1);
        trimmed          = Trim(trimmed);

        // **Step 5: Extract name and value**
        // Format: <name> = <value>;
        size_t equalPos = trimmed.find('=');
        if (equalPos == std::string::npos)
        {
            return false; // Format error: no '='
        }

        std::string name     = Trim(trimmed.substr(0, equalPos));
        std::string valueStr = Trim(trimmed.substr(equalPos + 1));

        // **Step 6: Remove the ';' at the end**
        // GLSL/HLSL constant declaration must end with ';'
        if (!valueStr.empty() && valueStr.back() == ';')
        {
            valueStr.pop_back();
            valueStr = Trim(valueStr);
        }

        // **Step 7: Call the corresponding parsing function according to the type**
        if (type == "int")
        {
            return ParseInt(name, valueStr);
        }
        else if (type == "float")
        {
            return ParseFloat(name, valueStr);
        }
        else if (type == "bool")
        {
            return ParseBool(name, valueStr);
        }
        // Vector type - supports GLSL (vec2/3/4) and HLSL (float2/3/4) syntax
        else if (type == "vec2" || type == "float2")
        {
            return ParseVec2(name, valueStr);
        }
        else if (type == "vec3" || type == "float3")
        {
            return ParseVec3(name, valueStr);
        }
        else if (type == "vec4" || type == "float4")
        {
            return ParseVec4(name, valueStr);
        }
        else if (type == "ivec2" || type == "int2")
        {
            return ParseIntVec2(name, valueStr);
        }
        else if (type == "ivec3" || type == "int3")
        {
            return ParseIntVec3(name, valueStr);
        }

        // unknown type
        return false;
    }

    size_t ConstDirectiveParser::ParseLines(const std::vector<std::string>& lines)
    {
        size_t successCount = 0;

        //Loop through all lines and try to parse each line
        for (const auto& line : lines)
        {
            if (Parse(line))
            {
                successCount++;
            }
        }

        return successCount;
    }

    // ========================================================================
    // Query method implementation
    // ========================================================================

    std::optional<int> ConstDirectiveParser::GetInt(const std::string& name) const
    {
        auto it = m_constants.find(name);
        if (it == m_constants.end())
        {
            return std::nullopt; // constant does not exist
        }

        // Use std::holds_alternative to check the type
        if (std::holds_alternative<int>(it->second))
        {
            return std::get<int>(it->second);
        }

        return std::nullopt; // type mismatch
    }

    std::optional<float> ConstDirectiveParser::GetFloat(const std::string& name) const
    {
        auto it = m_constants.find(name);
        if (it == m_constants.end())
        {
            return std::nullopt;
        }

        if (std::holds_alternative<float>(it->second))
        {
            return std::get<float>(it->second);
        }

        return std::nullopt;
    }

    std::optional<bool> ConstDirectiveParser::GetBool(const std::string& name) const
    {
        auto it = m_constants.find(name);
        if (it == m_constants.end())
        {
            return std::nullopt;
        }

        if (std::holds_alternative<bool>(it->second))
        {
            return std::get<bool>(it->second);
        }

        return std::nullopt;
    }

    std::optional<Vec2> ConstDirectiveParser::GetVec2(const std::string& name) const
    {
        auto it = m_constants.find(name);
        if (it == m_constants.end())
        {
            return std::nullopt;
        }

        if (std::holds_alternative<Vec2>(it->second))
        {
            return std::get<Vec2>(it->second);
        }

        return std::nullopt;
    }

    std::optional<Vec3> ConstDirectiveParser::GetVec3(const std::string& name) const
    {
        auto it = m_constants.find(name);
        if (it == m_constants.end())
        {
            return std::nullopt;
        }

        if (std::holds_alternative<Vec3>(it->second))
        {
            return std::get<Vec3>(it->second);
        }

        return std::nullopt;
    }

    std::optional<Vec4> ConstDirectiveParser::GetVec4(const std::string& name) const
    {
        auto it = m_constants.find(name);
        if (it == m_constants.end())
        {
            return std::nullopt;
        }

        if (std::holds_alternative<Vec4>(it->second))
        {
            return std::get<Vec4>(it->second);
        }

        return std::nullopt;
    }

    std::optional<IntVec2> ConstDirectiveParser::GetIntVec2(const std::string& name) const
    {
        auto it = m_constants.find(name);
        if (it == m_constants.end())
        {
            return std::nullopt;
        }

        if (std::holds_alternative<IntVec2>(it->second))
        {
            return std::get<IntVec2>(it->second);
        }

        return std::nullopt;
    }

    std::optional<IntVec3> ConstDirectiveParser::GetIntVec3(const std::string& name) const
    {
        auto it = m_constants.find(name);
        if (it == m_constants.end())
        {
            return std::nullopt;
        }

        if (std::holds_alternative<IntVec3>(it->second))
        {
            return std::get<IntVec3>(it->second);
        }

        return std::nullopt;
    }

    bool ConstDirectiveParser::HasConstant(const std::string& name) const
    {
        return m_constants.find(name) != m_constants.end();
    }

    std::vector<std::string> ConstDirectiveParser::GetAllConstantNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_constants.size());

        for (const auto& pair : m_constants)
        {
            names.push_back(pair.first);
        }

        return names;
    }

    void ConstDirectiveParser::Clear()
    {
        m_constants.clear();
    }

    // ========================================================================
    // Scalar type parsing implementation (Iris compatible)
    // ========================================================================

    bool ConstDirectiveParser::ParseInt(const std::string& name, const std::string& valueStr)
    {
        try
        {
            // std::stoi automatically handles leading whitespace and signs
            int value         = std::stoi(valueStr);
            m_constants[name] = value;
            return true;
        }
        catch (const std::invalid_argument&)
        {
            // Invalid format (for example: "abc")
            return false;
        }
        catch (const std::out_of_range&)
        {
            //out of int range
            return false;
        }
    }

    bool ConstDirectiveParser::ParseFloat(const std::string& name, const std::string& valueStr)
    {
        try
        {
            // **Remove 'f' or 'F' suffix (HLSL style) **
            //Example: "0.5f" -> "0.5"
            std::string cleaned = valueStr;
            if (!cleaned.empty() && (cleaned.back() == 'f' || cleaned.back() == 'F'))
            {
                cleaned.pop_back();
            }

            float value       = std::stof(cleaned);
            m_constants[name] = value;
            return true;
        }
        catch (const std::invalid_argument&)
        {
            return false;
        }
        catch (const std::out_of_range&)
        {
            return false;
        }
    }

    bool ConstDirectiveParser::ParseBool(const std::string& name, const std::string& valueStr)
    {
        // **Iris compatible logic** (see OptionAnnotatedSource.java lines 275-283)
        //Only accepts "true" and "false" strings, not "1" and "0"
        if (valueStr == "true")
        {
            m_constants[name] = true;
            return true;
        }
        else if (valueStr == "false")
        {
            m_constants[name] = false;
            return true;
        }

        // No other formats are accepted (consistent with Iris)
        return false;
    }

    // ========================================================================
    // Vector type parsing implementation (DirectX 12 extension)
    // ========================================================================

    bool ConstDirectiveParser::ParseVec2(const std::string& name, const std::string& valueStr)
    {
        //Extract the value in vec2(x, y)
        auto values = ExtractVectorValues(valueStr);
        if (values.size() != 2)
        {
            return false; // Must have exactly 2 components
        }

        try
        {
            float x           = std::stof(values[0]);
            float y           = std::stof(values[1]);
            m_constants[name] = Vec2(x, y);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ConstDirectiveParser::ParseVec3(const std::string& name, const std::string& valueStr)
    {
        // **Core vector parsing logic**
        // Format: "vec3(0.8, 0.9, 1.0)" -> {0.8, 0.9, 1.0}
        auto values = ExtractVectorValues(valueStr);
        if (values.size() != 3)
        {
            return false; // Must have exactly 3 components
        }

        try
        {
            float x           = std::stof(values[0]);
            float y           = std::stof(values[1]);
            float z           = std::stof(values[2]);
            m_constants[name] = Vec3(x, y, z);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ConstDirectiveParser::ParseVec4(const std::string& name, const std::string& valueStr)
    {
        auto values = ExtractVectorValues(valueStr);
        if (values.size() != 4)
        {
            return false;
        }

        try
        {
            float x           = std::stof(values[0]);
            float y           = std::stof(values[1]);
            float z           = std::stof(values[2]);
            float w           = std::stof(values[3]);
            m_constants[name] = Vec4(x, y, z, w);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ConstDirectiveParser::ParseIntVec2(const std::string& name, const std::string& valueStr)
    {
        auto values = ExtractVectorValues(valueStr);
        if (values.size() != 2)
        {
            return false;
        }

        try
        {
            int x             = std::stoi(values[0]);
            int y             = std::stoi(values[1]);
            m_constants[name] = IntVec2(x, y);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ConstDirectiveParser::ParseIntVec3(const std::string& name, const std::string& valueStr)
    {
        auto values = ExtractVectorValues(valueStr);
        if (values.size() != 3)
        {
            return false;
        }

        try
        {
            int x             = std::stoi(values[0]);
            int y             = std::stoi(values[1]);
            int z             = std::stoi(values[2]);
            m_constants[name] = IntVec3(x, y, z);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::vector<std::string> ConstDirectiveParser::ExtractVectorValues(const std::string& valueStr)
    {
        /**
         * **Vector constructor parsing logic**
         *
         * Input example: "vec3(0.8, 0.9, 1.0)"
         * Output: ["0.8", "0.9", "1.0"]
         *
         * **Analysis steps**:
         * 1. Find '(' and ')' - locate vector parameter list
         * 2. Extract the content within the brackets - "0.8, 0.9, 1.0"
         * 3. Split by ',' - ["0.8", " 0.9", " 1.0"]
         * 4. Trim each value - ["0.8", "0.9", "1.0"]
         */

        // **Step 1: Find '(' and ')'**
        size_t openParen  = valueStr.find('(');
        size_t closeParen = valueStr.find(')');

        if (openParen == std::string::npos || closeParen == std::string::npos)
        {
            return {}; // Format error: no brackets
        }

        if (openParen >= closeParen)
        {
            return {}; // Format error: Wrong order of brackets
        }

        // **Step 2: Extract the content within the brackets**
        std::string valuesStr = valueStr.substr(openParen + 1, closeParen - openParen - 1);

        // **Step 3 & 4: Press ',' to split and trim**
        std::vector<std::string> result;
        size_t                   start = 0;
        size_t                   comma = valuesStr.find(',');

        while (comma != std::string::npos)
        {
            std::string value = valuesStr.substr(start, comma - start);
            result.push_back(Trim(value));

            start = comma + 1;
            comma = valuesStr.find(',', start);
        }

        //Add the last value (the part after the comma)
        std::string lastValue = valuesStr.substr(start);
        result.push_back(Trim(lastValue));

        return result;
    }

    std::string ConstDirectiveParser::Trim(const std::string& str) const
    {
        /**
         * **Trim implementation - remove leading and trailing blanks**
         *
         * Whitespace characters include: spaces, tabs, line feeds, carriage returns, etc.
         * Use std::isspace() to determine
         */

        if (str.empty())
        {
            return str;
        }

        // **Find the first non-whitespace character**
        size_t start = 0;
        while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start])))
        {
            start++;
        }

        // **Find the last non-whitespace character**
        size_t end = str.size();
        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
        {
            end--;
        }

        // **Extract the middle part**
        return str.substr(start, end - start);
    }
} // namespace enigma::graphic
