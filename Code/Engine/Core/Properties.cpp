/**
 * @file Properties.cpp
 * @brief Java Properties 格式解析器实现 - 支持 C 预处理器指令
 * @date 2025-10-12
 */

#include "Engine/Core/Properties.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "EngineCommon.hpp"

namespace enigma::core
{
    //-----------------------------------------------------------------------------
    // 文件加载和解析
    //-----------------------------------------------------------------------------

    bool PropertiesFile::Load(const std::filesystem::path& filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            return false;
        }

        m_filepath = filepath;

        // 读取文件内容
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        return LoadFromString(buffer.str());
    }

    bool PropertiesFile::LoadFromString(const std::string& content)
    {
        m_originalContent = content;
        m_properties.clear();

        try
        {
            // 两阶段解析：预处理 → 键值对提取
            std::string preprocessed = Preprocess(m_originalContent);
            ParseKeyValuePairs(preprocessed);
            return true;
        }
        catch (const PropertiesException&)
        {
            return false;
        }
    }

    void PropertiesFile::Reload()
    {
        if (!m_originalContent.empty())
        {
            LoadFromString(m_originalContent);
        }
        else if (!m_filepath.empty())
        {
            Load(m_filepath);
        }
    }

    //-----------------------------------------------------------------------------
    // 预处理器宏定义
    //-----------------------------------------------------------------------------

    void PropertiesFile::Define(const std::string& macro, const std::string& value)
    {
        m_macros[macro] = value;
        Reload(); // 重新解析以应用新宏
    }

    void PropertiesFile::Undefine(const std::string& macro)
    {
        m_macros.erase(macro);
        Reload();
    }

    bool PropertiesFile::IsDefined(const std::string& macro) const
    {
        return m_macros.find(macro) != m_macros.end();
    }

    //-----------------------------------------------------------------------------
    // 键值访问
    //-----------------------------------------------------------------------------

    std::string PropertiesFile::Get(const std::string& key, const std::string& defaultValue) const
    {
        auto it = m_properties.find(key);
        return (it != m_properties.end()) ? it->second : defaultValue;
    }

    int PropertiesFile::GetInt(const std::string& key, int defaultValue) const
    {
        auto it = m_properties.find(key);
        if (it == m_properties.end())
        {
            return defaultValue;
        }

        try
        {
            return std::stoi(it->second);
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    float PropertiesFile::GetFloat(const std::string& key, float defaultValue) const
    {
        auto it = m_properties.find(key);
        if (it == m_properties.end())
        {
            return defaultValue;
        }

        try
        {
            return std::stof(it->second);
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    bool PropertiesFile::GetBool(const std::string& key, bool defaultValue) const
    {
        auto it = m_properties.find(key);
        if (it == m_properties.end())
        {
            return defaultValue;
        }

        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (value == "true" || value == "1" || value == "yes" || value == "on")
        {
            return true;
        }
        else if (value == "false" || value == "0" || value == "no" || value == "off")
        {
            return false;
        }

        return defaultValue;
    }

    std::optional<std::string> PropertiesFile::GetOptional(const std::string& key) const
    {
        auto it = m_properties.find(key);
        if (it != m_properties.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    bool PropertiesFile::Contains(const std::string& key) const
    {
        return m_properties.find(key) != m_properties.end();
    }

    //-----------------------------------------------------------------------------
    // 修改操作
    //-----------------------------------------------------------------------------

    void PropertiesFile::Set(const std::string& key, const std::string& value)
    {
        m_properties[key] = value;
    }

    void PropertiesFile::Remove(const std::string& key)
    {
        m_properties.erase(key);
    }

    void PropertiesFile::Clear()
    {
        m_properties.clear();
    }

    //-----------------------------------------------------------------------------
    // 遍历和查询
    //-----------------------------------------------------------------------------

    std::vector<std::string> PropertiesFile::GetKeys() const
    {
        std::vector<std::string> keys;
        keys.reserve(m_properties.size());
        for (const auto& [key, value] : m_properties)
        {
            keys.push_back(key);
        }
        return keys;
    }

    const std::unordered_map<std::string, std::string>& PropertiesFile::GetAll() const
    {
        return m_properties;
    }

    size_t PropertiesFile::Size() const
    {
        return m_properties.size();
    }

    bool PropertiesFile::Empty() const
    {
        return m_properties.empty();
    }

    //-----------------------------------------------------------------------------
    // 预处理器实现 - 处理 #ifdef, #if, #define 等指令
    //-----------------------------------------------------------------------------

    std::string PropertiesFile::Preprocess(const std::string& content) const
    {
        std::istringstream input(content);
        std::ostringstream output;
        std::string        line;
        size_t             lineNumber = 0;

        // 局部宏映射：合并外部宏（m_macros）和文件内 #define 指令
        std::unordered_map<std::string, std::string> localMacros = m_macros;

        // Lambda: 检查宏是否定义（局部 + 外部）
        auto IsDefinedLocal = [&localMacros](const std::string& macro) -> bool
        {
            return localMacros.find(macro) != localMacros.end();
        };

        // Lambda: 获取宏值（局部 + 外部）
        auto GetMacroValue = [&localMacros](const std::string& macro) -> std::string
        {
            auto it = localMacros.find(macro);
            return (it != localMacros.end()) ? it->second : "";
        };

        // 条件栈：每个元素表示当前 #ifdef/#if 块是否启用
        std::vector<bool> conditionStack;
        conditionStack.push_back(true); // 根级别始终启用

        while (std::getline(input, line))
        {
            ++lineNumber;
            std::string trimmedLine = Trim(line);

            // 检查是否为预处理器指令
            if (trimmedLine.empty() || trimmedLine[0] != '#')
            {
                // 普通行：只有当前所有条件都为真时才输出
                bool allConditionsTrue = true;
                for (bool cond : conditionStack)
                {
                    if (!cond)
                    {
                        allConditionsTrue = false;
                        break;
                    }
                }

                if (allConditionsTrue)
                {
                    output << line << "\n";
                }
                continue;
            }

            // 解析预处理器指令
            std::istringstream lineStream(trimmedLine);
            std::string        directive;
            lineStream >> directive;

            if (directive == "#define")
            {
                // #define MACRO VALUE
                std::string macro, value;
                lineStream >> macro;
                std::getline(lineStream, value);
                value = Trim(value);

                // 存储到局部宏映射（只有在当前条件为真时才定义）
                bool allConditionsTrue = true;
                for (bool cond : conditionStack)
                {
                    if (!cond)
                    {
                        allConditionsTrue = false;
                        break;
                    }
                }

                if (allConditionsTrue)
                {
                    localMacros[macro] = value;
                }
                continue;
            }
            else if (directive == "#ifdef")
            {
                // #ifdef MACRO
                std::string macro;
                lineStream >> macro;

                bool parentCondition = conditionStack.back();
                bool macroIsDefined  = IsDefinedLocal(macro);
                conditionStack.push_back(parentCondition && macroIsDefined);
            }
            else if (directive == "#ifndef")
            {
                // #ifndef MACRO
                std::string macro;
                lineStream >> macro;

                bool parentCondition = conditionStack.back();
                bool macroNotDefined = !IsDefinedLocal(macro);
                conditionStack.push_back(parentCondition && macroNotDefined);
            }
            else if (directive == "#if")
            {
                // #if EXPRESSION (例如: #if MC_VERSION >= 11300)
                std::string expression;
                std::getline(lineStream, expression);
                expression = Trim(expression);

                bool parentCondition = conditionStack.back();
                // 需要在表达式求值中使用 localMacros，暂时通过修改 EvaluateExpression 实现
                // 这里先使用简化实现：检查宏替换
                bool exprResult = false;

                // 检查比较运算符
                const std::vector<std::pair<std::string, std::string>> operators = {
                    {">=", ">="}, {"<=", "<="}, {"==", "=="}, {"!=", "!="}, {">", ">"}, {"<", "<"}
                };

                bool foundOperator = false;
                for (const auto& [op, opStr] : operators)
                {
                    size_t opPos = expression.find(op);
                    if (opPos != std::string::npos)
                    {
                        foundOperator     = true;
                        std::string left  = Trim(expression.substr(0, opPos));
                        std::string right = Trim(expression.substr(opPos + op.length()));

                        // 替换左侧宏（使用 localMacros）
                        if (IsDefinedLocal(left))
                        {
                            left = GetMacroValue(left);
                        }

                        // 尝试整数比较
                        try
                        {
                            int leftValue  = std::stoi(left);
                            int rightValue = std::stoi(right);

                            if (op == ">=") exprResult = (leftValue >= rightValue);
                            else if (op == "<=") exprResult = (leftValue <= rightValue);
                            else if (op == "==") exprResult = (leftValue == rightValue);
                            else if (op == "!=") exprResult = (leftValue != rightValue);
                            else if (op == ">") exprResult = (leftValue > rightValue);
                            else if (op == "<") exprResult = (leftValue < rightValue);
                        }
                        catch (...)
                        {
                            // 字符串比较
                            if (op == "==") exprResult = (left == right);
                            else if (op == "!=") exprResult = (left != right);
                        }
                        break;
                    }
                }

                // 如果没有找到运算符，检查宏存在性
                if (!foundOperator)
                {
                    exprResult = IsDefinedLocal(Trim(expression));
                }

                conditionStack.push_back(parentCondition && exprResult);
            }
            else if (directive == "#elif")
            {
                // #elif EXPRESSION
                if (conditionStack.size() <= 1)
                {
                    ERROR_AND_DIE("#elif without matching #if");
                }

                std::string expression;
                std::getline(lineStream, expression);
                expression = Trim(expression);

                // 弹出当前条件
                conditionStack.pop_back();

                bool parentCondition = conditionStack.back();
                bool exprResult      = EvaluateExpression(expression);

                // #elif 只有在之前的条件都为假且父条件为真时才可能为真
                // 这里简化实现：假设 #elif 只检查表达式本身
                conditionStack.push_back(parentCondition && exprResult);
            }
            else if (directive == "#else")
            {
                // #else
                if (conditionStack.size() <= 1)
                {
                    ERROR_AND_DIE("#else without matching #if");
                }

                // 取反当前条件
                bool currentCondition = conditionStack.back();
                conditionStack.pop_back();

                bool parentCondition = conditionStack.back();
                conditionStack.push_back(parentCondition && !currentCondition);
            }
            else if (directive == "#endif")
            {
                // #endif
                if (conditionStack.size() <= 1)
                {
                    ERROR_AND_DIE("#endif without matching #if");
                }

                conditionStack.pop_back();
            }
            else
            {
                // 未知指令或注释行（以 # 开头但不是预处理器指令）
                // 作为注释跳过
            }
        }

        // 检查是否有未闭合的条件块
        if (conditionStack.size() > 1)
        {
            ERROR_AND_DIE("Unclosed #if/#ifdef block");
        }

        return output.str();
    }

    //-----------------------------------------------------------------------------
    // 键值对解析
    //-----------------------------------------------------------------------------

    void PropertiesFile::ParseKeyValuePairs(const std::string& content)
    {
        std::istringstream input(content);
        std::string        line;
        std::string        continuedLine;
        size_t             lineNumber = 0;

        while (std::getline(input, line))
        {
            ++lineNumber;

            // 处理行续接（反斜杠结尾）
            while (!line.empty() && line.back() == '\\')
            {
                line.pop_back(); // 移除反斜杠
                continuedLine += line;

                if (!std::getline(input, line))
                {
                    break;
                }
                ++lineNumber;
            }

            if (!continuedLine.empty())
            {
                continuedLine += line;
                ParseLine(continuedLine, lineNumber);
                continuedLine.clear();
            }
            else
            {
                ParseLine(line, lineNumber);
            }
        }
    }

    void PropertiesFile::ParseLine(const std::string& line, size_t lineNumber)
    {
        std::string trimmedLine = Trim(line);
        UNUSED(lineNumber)
        // 跳过空行和注释
        if (trimmedLine.empty() || trimmedLine[0] == '#' || trimmedLine[0] == '!')
        {
            return;
        }

        // 查找分隔符 '=' 或 ':'
        size_t separatorPos = trimmedLine.find_first_of("=:");
        if (separatorPos == std::string::npos)
        {
            // 没有分隔符，跳过该行
            return;
        }

        // 提取 key 和 value
        std::string key   = Trim(trimmedLine.substr(0, separatorPos));
        std::string value = Trim(trimmedLine.substr(separatorPos + 1));

        // Unicode 转义处理
        value = UnescapeUnicode(value);

        m_properties[key] = value;
    }

    //-----------------------------------------------------------------------------
    // 表达式求值 - 支持比较运算符
    //-----------------------------------------------------------------------------

    bool PropertiesFile::EvaluateExpression(const std::string& expression) const
    {
        std::string expr = Trim(expression);

        // 检查比较运算符
        const std::vector<std::pair<std::string, std::string>> operators = {
            {">=", ">="},
            {"<=", "<="},
            {"==", "=="},
            {"!=", "!="},
            {">", ">"},
            {"<", "<"}
        };

        for (const auto& [op, opStr] : operators)
        {
            size_t opPos = expr.find(op);
            if (opPos != std::string::npos)
            {
                std::string left  = Trim(expr.substr(0, opPos));
                std::string right = Trim(expr.substr(opPos + op.length()));

                // 替换左侧宏
                if (IsDefined(left))
                {
                    left = m_macros.at(left);
                }

                // 尝试转换为整数比较
                try
                {
                    int leftValue  = std::stoi(left);
                    int rightValue = std::stoi(right);

                    if (op == ">=") return leftValue >= rightValue;
                    if (op == "<=") return leftValue <= rightValue;
                    if (op == "==") return leftValue == rightValue;
                    if (op == "!=") return leftValue != rightValue;
                    if (op == ">") return leftValue > rightValue;
                    if (op == "<") return leftValue < rightValue;
                }
                catch (...)
                {
                    // 字符串比较
                    if (op == "==") return left == right;
                    if (op == "!=") return left != right;
                    // 其他运算符不支持字符串比较
                    return false;
                }
            }
        }

        // 简单宏存在性检查
        return IsDefined(expr);
    }

    //-----------------------------------------------------------------------------
    // 工具函数
    //-----------------------------------------------------------------------------

    std::string PropertiesFile::Trim(const std::string& str)
    {
        if (str.empty())
        {
            return str;
        }

        size_t start = 0;
        size_t end   = str.length();

        while (start < end && std::isspace(static_cast<unsigned char>(str[start])))
        {
            ++start;
        }

        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
        {
            --end;
        }

        return str.substr(start, end - start);
    }

    std::string PropertiesFile::UnescapeUnicode(const std::string& str)
    {
        std::string result;
        result.reserve(str.length());

        for (size_t i = 0; i < str.length(); ++i)
        {
            if (str[i] == '\\' && i + 5 < str.length() && str[i + 1] == 'u')
            {
                // 解析 \uXXXX
                std::string hexCode = str.substr(i + 2, 4);
                try
                {
                    int codePoint = std::stoi(hexCode, nullptr, 16);
                    // 简化处理：假设所有 Unicode 字符都在 ASCII 扩展范围内
                    result += static_cast<char>(codePoint);
                    i += 5; // 跳过 \uXXXX
                }
                catch (...)
                {
                    // 解析失败，保留原字符
                    result += str[i];
                }
            }
            else if (str[i] == '\\' && i + 1 < str.length())
            {
                // 其他转义序列
                char nextChar = str[i + 1];
                switch (nextChar)
                {
                case 'n': result += '\n';
                    ++i;
                    break;
                case 't': result += '\t';
                    ++i;
                    break;
                case 'r': result += '\r';
                    ++i;
                    break;
                case '\\': result += '\\';
                    ++i;
                    break;
                default: result += str[i];
                    break;
                }
            }
            else
            {
                result += str[i];
            }
        }

        return result;
    }
} // namespace enigma::core
