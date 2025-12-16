#include "ConstDirectiveParser.hpp"
#include <algorithm>
#include <cctype>

/**
 * @file ConstDirectiveParser.cpp
 * @brief const常量指令解析器实现
 *
 * **实现参考**:
 * - Iris源码: OptionAnnotatedSource.java::parseConst() (第202-309行)
 * - 核心逻辑: Trim -> 检查"const" -> 提取类型 -> 提取名称 -> 提取值 -> 解析并存储
 *
 * **关键设计决策**:
 * 1. 使用简单字符串操作，不使用正则表达式（KISS原则）
 * 2. 异常安全 - 所有std::stoi/stof调用都包裹在try-catch中
 * 3. 类型安全 - 使用std::variant + std::holds_alternative保证类型正确
 * 4. 鲁棒性 - 单个常量解析失败不影响其他常量
 *
 * @author Iris源码参考 + DirectX 12项目实现
 * @date 2025-10-13
 */

namespace enigma::graphic
{
    // ========================================================================
    // 核心解析方法实现
    // ========================================================================

    bool ConstDirectiveParser::Parse(const std::string& line)
    {
        // **Step 1: Trim首尾空白**
        // 目的: 忽略缩进和尾部空白，统一格式
        std::string trimmed = Trim(line);

        // **Step 2: 检查是否以"const "开头**
        // 注意: "const "包含空格，避免匹配"constant"等单词
        if (trimmed.find("const ") != 0)
        {
            return false; // 不是const指令
        }

        // **Step 3: 移除"const "前缀**
        // "const " = 6个字符（包含空格）
        trimmed = trimmed.substr(6);
        trimmed = Trim(trimmed); // 移除"const"后的额外空白

        // **Step 4: 提取类型（第一个单词）**
        // 查找第一个空格，空格之前是类型
        size_t spacePos = trimmed.find(' ');
        if (spacePos == std::string::npos)
        {
            return false; // 格式错误：没有空格分隔类型和名称
        }

        std::string type = trimmed.substr(0, spacePos);
        trimmed          = trimmed.substr(spacePos + 1);
        trimmed          = Trim(trimmed);

        // **Step 5: 提取名称和值**
        // 格式: <name> = <value>;
        size_t equalPos = trimmed.find('=');
        if (equalPos == std::string::npos)
        {
            return false; // 格式错误：没有'='
        }

        std::string name     = Trim(trimmed.substr(0, equalPos));
        std::string valueStr = Trim(trimmed.substr(equalPos + 1));

        // **Step 6: 移除末尾的';'**
        // GLSL/HLSL常量声明必须以';'结尾
        if (!valueStr.empty() && valueStr.back() == ';')
        {
            valueStr.pop_back();
            valueStr = Trim(valueStr);
        }

        // **Step 7: 根据类型调用对应的解析函数**
        // 标量类型（Iris兼容）
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
        // 向量类型（DirectX 12扩展）
        else if (type == "vec2")
        {
            return ParseVec2(name, valueStr);
        }
        else if (type == "vec3")
        {
            return ParseVec3(name, valueStr);
        }
        else if (type == "vec4")
        {
            return ParseVec4(name, valueStr);
        }
        else if (type == "ivec2")
        {
            return ParseIntVec2(name, valueStr);
        }
        else if (type == "ivec3")
        {
            return ParseIntVec3(name, valueStr);
        }

        // 未知类型
        return false;
    }

    size_t ConstDirectiveParser::ParseLines(const std::vector<std::string>& lines)
    {
        size_t successCount = 0;

        // 遍历所有行，尝试解析每一行
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
    // 查询方法实现
    // ========================================================================

    std::optional<int> ConstDirectiveParser::GetInt(const std::string& name) const
    {
        auto it = m_constants.find(name);
        if (it == m_constants.end())
        {
            return std::nullopt; // 常量不存在
        }

        // 使用std::holds_alternative检查类型
        if (std::holds_alternative<int>(it->second))
        {
            return std::get<int>(it->second);
        }

        return std::nullopt; // 类型不匹配
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
    // 标量类型解析实现（Iris兼容）
    // ========================================================================

    bool ConstDirectiveParser::ParseInt(const std::string& name, const std::string& valueStr)
    {
        try
        {
            // std::stoi自动处理前导空白和正负号
            int value         = std::stoi(valueStr);
            m_constants[name] = value;
            return true;
        }
        catch (const std::invalid_argument&)
        {
            // 无效格式（例如："abc"）
            return false;
        }
        catch (const std::out_of_range&)
        {
            // 超出int范围
            return false;
        }
    }

    bool ConstDirectiveParser::ParseFloat(const std::string& name, const std::string& valueStr)
    {
        try
        {
            // **移除'f'或'F'后缀（HLSL风格）**
            // 例如: "0.5f" -> "0.5"
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
        // **Iris兼容逻辑**（见OptionAnnotatedSource.java第275-283行）
        // 只接受"true"和"false"字符串，不接受"1"和"0"
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

        // 不接受其他格式（与Iris一致）
        return false;
    }

    // ========================================================================
    // 向量类型解析实现（DirectX 12扩展）
    // ========================================================================

    bool ConstDirectiveParser::ParseVec2(const std::string& name, const std::string& valueStr)
    {
        // 提取vec2(x, y)中的值
        auto values = ExtractVectorValues(valueStr);
        if (values.size() != 2)
        {
            return false; // 必须恰好2个分量
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
        // **核心向量解析逻辑**
        // 格式: "vec3(0.8, 0.9, 1.0)" -> {0.8, 0.9, 1.0}
        auto values = ExtractVectorValues(valueStr);
        if (values.size() != 3)
        {
            return false; // 必须恰好3个分量
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

    // ========================================================================
    // 辅助函数实现
    // ========================================================================

    std::vector<std::string> ConstDirectiveParser::ExtractVectorValues(const std::string& valueStr)
    {
        /**
         * **向量构造函数解析逻辑**
         *
         * 输入示例: "vec3(0.8, 0.9, 1.0)"
         * 输出: ["0.8", "0.9", "1.0"]
         *
         * **解析步骤**:
         * 1. 查找'('和')' - 定位向量参数列表
         * 2. 提取括号内的内容 - "0.8, 0.9, 1.0"
         * 3. 按','分割 - ["0.8", " 0.9", " 1.0"]
         * 4. Trim每个值 - ["0.8", "0.9", "1.0"]
         */

        // **Step 1: 查找'('和')'**
        size_t openParen  = valueStr.find('(');
        size_t closeParen = valueStr.find(')');

        if (openParen == std::string::npos || closeParen == std::string::npos)
        {
            return {}; // 格式错误：没有括号
        }

        if (openParen >= closeParen)
        {
            return {}; // 格式错误：括号顺序错误
        }

        // **Step 2: 提取括号内的内容**
        std::string valuesStr = valueStr.substr(openParen + 1, closeParen - openParen - 1);

        // **Step 3 & 4: 按','分割并Trim**
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

        // 添加最后一个值（逗号之后的部分）
        std::string lastValue = valuesStr.substr(start);
        result.push_back(Trim(lastValue));

        return result;
    }

    std::string ConstDirectiveParser::Trim(const std::string& str) const
    {
        /**
         * **Trim实现 - 去除首尾空白**
         *
         * 空白字符包括: 空格, 制表符, 换行符, 回车符等
         * 使用std::isspace()判断
         */

        if (str.empty())
        {
            return str;
        }

        // **查找第一个非空白字符**
        size_t start = 0;
        while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start])))
        {
            start++;
        }

        // **查找最后一个非空白字符**
        size_t end = str.size();
        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
        {
            end--;
        }

        // **提取中间部分**
        return str.substr(start, end - start);
    }
} // namespace enigma::graphic
