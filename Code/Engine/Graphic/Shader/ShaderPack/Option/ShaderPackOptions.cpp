/**
 * @file ShaderPackOptions.cpp
 * @brief Shader Pack 宏选项系统实现 - 对齐 Iris ShaderPackOptions.java
 * @date 2025-10-13
 */

#include "ShaderPackOptions.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <cctype>

using namespace enigma::graphic;
using namespace enigma::core;

// ============================================================================
// 构造函数
// ============================================================================

ShaderPackOptions::ShaderPackOptions()
    : m_isValid(false)
{
}

// ============================================================================
// 加载和解析
// ============================================================================

bool ShaderPackOptions::Parse(const std::filesystem::path& rootPath)
{
    // ========================================================================
    // Step 1: 构造 shaders.properties 路径
    // ========================================================================
    auto propertiesPath = rootPath / "shaders" / "shaders.properties";

    // ========================================================================
    // Step 2: 检查文件存在性
    // ========================================================================
    // 注意: shaders.properties 是可选文件，不存在不算错误 (对齐 Iris 宽松策略)
    if (!std::filesystem::exists(propertiesPath))
    {
        DebuggerPrintf("[ShaderPackOptions] shaders.properties not found at '%s' (optional file)\n",
                       propertiesPath.string().c_str());

        // 标记为有效，但不包含任何选项
        m_isValid = true;
        return true;
    }

    // ========================================================================
    // Step 3: 使用 PropertiesFile 基类加载文件
    // ========================================================================
    if (!m_propertiesFile.Load(propertiesPath))
    {
        DebuggerPrintf("[ShaderPackOptions] Failed to load shaders.properties at '%s'\n",
                       propertiesPath.string().c_str());

        // 对齐 Iris 宽松策略: 加载失败不中断初始化
        m_isValid = true;
        return false;
    }

    // ========================================================================
    // Step 4: 遍历所有键值对，调用 ParseDirective() 解析
    // ========================================================================
    const auto& allProperties = m_propertiesFile.GetAll();
    for (const auto& [key, value] : allProperties)
    {
        ParseDirective(key, value);
    }

    // ========================================================================
    // Step 5: 标记为有效
    // ========================================================================
    m_isValid = true;

    DebuggerPrintf("[ShaderPackOptions] Loaded %zu options from '%s'\n",
                   m_options.size(),
                   propertiesPath.string().c_str());

    return true;
}

bool ShaderPackOptions::ParseFromString(const std::string& content)
{
    // ========================================================================
    // Step 1: 使用 PropertiesFile 基类加载字符串
    // ========================================================================
    if (!m_propertiesFile.LoadFromString(content))
    {
        DebuggerPrintf("[ShaderPackOptions] Failed to parse string content\n");
        return false;
    }

    // ========================================================================
    // Step 2: 遍历所有键值对，调用 ParseDirective() 解析
    // ========================================================================
    const auto& allProperties = m_propertiesFile.GetAll();
    for (const auto& [key, value] : allProperties)
    {
        ParseDirective(key, value);
    }

    // ========================================================================
    // Step 3: 标记为有效
    // ========================================================================
    m_isValid = true;

    DebuggerPrintf("[ShaderPackOptions] Loaded %zu options from string\n", m_options.size());

    return true;
}

// ============================================================================
// 选项查询
// ============================================================================

bool ShaderPackOptions::HasOption(const std::string& name) const
{
    return m_options.count(name) > 0;
}

std::optional<OptionValue> ShaderPackOptions::GetOption(const std::string& name) const
{
    // 查找选项
    auto it = m_options.find(name);
    if (it == m_options.end())
    {
        return std::nullopt;
    }

    return it->second;
}

const std::unordered_map<std::string, OptionValue>& ShaderPackOptions::GetAllOptions() const
{
    return m_options;
}

size_t ShaderPackOptions::GetOptionCount() const
{
    return m_options.size();
}

// ============================================================================
// 选项修改 (运行时)
// ============================================================================

bool ShaderPackOptions::SetOptionValue(const std::string& name, const std::string& value)
{
    // ========================================================================
    // Step 1: 检查选项是否存在
    // ========================================================================
    if (!m_options.count(name))
    {
        DebuggerPrintf("[ShaderPackOptions] SetOptionValue: Option '%s' does not exist\n", name.c_str());
        return false;
    }

    OptionValue& option = m_options[name];

    // ========================================================================
    // Step 2: 验证值是否在允许列表中
    // ========================================================================
    // 注意: allowedValues 为空表示无限制 (不太可能出现)
    if (!option.allowedValues.empty())
    {
        auto it = std::find(option.allowedValues.begin(), option.allowedValues.end(), value);
        if (it == option.allowedValues.end())
        {
            DebuggerPrintf("[ShaderPackOptions] SetOptionValue: Invalid value '%s' for option '%s'\n",
                           value.c_str(),
                           name.c_str());
            return false;
        }
    }

    // ========================================================================
    // Step 3: 更新值
    // ========================================================================
    option.currentValue = value;

    DebuggerPrintf("[ShaderPackOptions] SetOptionValue: '%s' = '%s'\n", name.c_str(), value.c_str());

    return true;
}

bool ShaderPackOptions::ResetOptionToDefault(const std::string& name)
{
    // 检查选项是否存在
    if (!m_options.count(name))
    {
        return false;
    }

    OptionValue& option = m_options[name];
    option.currentValue = option.defaultValue;

    return true;
}

void ShaderPackOptions::ResetAllToDefaults()
{
    for (auto& [name, option] : m_options)
    {
        option.currentValue = option.defaultValue;
    }
}

// ============================================================================
// 宏定义生成 (用于着色器预处理)
// ============================================================================

std::unordered_map<std::string, std::string> ShaderPackOptions::GetMacroDefinitions() const
{
    std::unordered_map<std::string, std::string> macros;

    // ========================================================================
    // 遍历所有选项，生成宏定义
    // ========================================================================
    for (const auto& [name, option] : m_options)
    {
        // ====================================================================
        // Boolean 选项: true 时定义宏，false 时不定义
        // ====================================================================
        if (option.type == OptionType::BOOLEAN)
        {
            // 检查值是否为 true (支持 "true", "1", "on", "yes")
            std::string lowerValue = option.currentValue;
            std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), [](unsigned char c) -> char
            {
                return static_cast<char>(std::tolower(c));
            });

            if (lowerValue == "true" || lowerValue == "1" || lowerValue == "on" || lowerValue == "yes")
            {
                // 定义为空 (#define SHADOW_ENABLED)
                macros[name] = "";
            }
            // false 时不添加到 macros (相当于不定义)
        }
        // ====================================================================
        // String 选项: 定义为选项值
        // ====================================================================
        else if (option.type == OptionType::STRING)
        {
            // 定义为选项值 (#define SHADOW_QUALITY HIGH)
            macros[name] = option.currentValue;
        }
    }

    return macros;
}

// ============================================================================
// 验证和状态
// ============================================================================

bool ShaderPackOptions::IsValid() const
{
    return m_isValid;
}

void ShaderPackOptions::Clear()
{
    m_options.clear();
    m_isValid = false;
}

// ============================================================================
// 内部解析方法
// ============================================================================

void ShaderPackOptions::ParseDirective(const std::string& key, const std::string& value)
{
    // ========================================================================
    // 检查是否以 "option." 开头
    // ========================================================================
    if (key.find("option.") != 0)
    {
        // 不是选项定义，静默忽略 (对齐 Iris 行为)
        return;
    }

    // ========================================================================
    // 去掉 "option." 前缀，获取剩余部分
    // ========================================================================
    std::string remainder = key.substr(7); // 去掉 "option."

    // ========================================================================
    // 检查是否是注释 (option.<name>.comment)
    // ========================================================================
    if (remainder.find(".comment") != std::string::npos)
    {
        // 提取选项名称 (去掉 ".comment" 后缀)
        std::string optionName = remainder.substr(0, remainder.find(".comment"));

        // 更新已存在选项的注释
        if (m_options.count(optionName))
        {
            m_options[optionName].comment = value;
            DebuggerPrintf("[ShaderPackOptions] Updated comment for '%s': '%s'\n", optionName.c_str(), value.c_str());
        }
        else
        {
            // 选项不存在，记录警告 (注释在选项定义之前)
            DebuggerPrintf("[ShaderPackOptions] Warning: Comment for non-existent option '%s'\n", optionName.c_str());
        }

        return;
    }

    // ========================================================================
    // 否则是选项定义 (option.<name>)
    // ========================================================================
    ParseOptionDefinition(remainder, value);
}

void ShaderPackOptions::ParseOptionDefinition(const std::string& name, const std::string& value)
{
    OptionValue option;
    option.name = name;

    // ========================================================================
    // 检查是否有允许值列表: "HIGH [LOW MEDIUM HIGH ULTRA]"
    // ========================================================================
    size_t bracketPos = value.find('[');
    if (bracketPos != std::string::npos)
    {
        // ====================================================================
        // String 选项 (多选一)
        // ====================================================================
        option.type = OptionType::STRING;

        // ----------------------------------------------------------------
        // 提取默认值 (括号前的部分)
        // ----------------------------------------------------------------
        option.defaultValue = value.substr(0, bracketPos);
        option.defaultValue = Trim(option.defaultValue);

        // ----------------------------------------------------------------
        // 提取允许值列表 (括号内的部分)
        // ----------------------------------------------------------------
        size_t endBracket = value.find(']', bracketPos);
        if (endBracket != std::string::npos)
        {
            std::string allowedStr = value.substr(bracketPos + 1, endBracket - bracketPos - 1);
            option.allowedValues   = SplitWhitespace(allowedStr);
        }
        else
        {
            // 括号未闭合，记录警告
            DebuggerPrintf("[ShaderPackOptions] Warning: Unclosed bracket in option '%s'\n", name.c_str());
        }

        // ----------------------------------------------------------------
        // 设置当前值为默认值
        // ----------------------------------------------------------------
        option.currentValue = option.defaultValue;
    }
    else
    {
        // ====================================================================
        // Boolean 选项
        // ====================================================================
        option.type         = OptionType::BOOLEAN;
        option.defaultValue = Trim(value);
        option.currentValue = option.defaultValue;

        // ----------------------------------------------------------------
        // Boolean 选项的允许值固定为 ["true", "false"]
        // ----------------------------------------------------------------
        option.allowedValues = {"true", "false"};
    }

    // ========================================================================
    // 存储选项
    // ========================================================================
    m_options[name] = option;

    DebuggerPrintf("[ShaderPackOptions] Loaded option '%s' (type=%s, default='%s')\n",
                   name.c_str(),
                   (option.type == OptionType::BOOLEAN ? "BOOLEAN" : "STRING"),
                   option.defaultValue.c_str());
}

// ============================================================================
// 辅助工具方法
// ============================================================================

std::vector<std::string> ShaderPackOptions::SplitWhitespace(const std::string& value)
{
    std::vector<std::string> result;
    std::istringstream       stream(value);
    std::string              token;

    // 按空格分割
    while (stream >> token)
    {
        result.push_back(token);
    }

    return result;
}

std::string ShaderPackOptions::Trim(const std::string& str)
{
    // 查找第一个非空白字符
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
    {
        // 全是空白字符
        return "";
    }

    // 查找最后一个非空白字符
    size_t end = str.find_last_not_of(" \t\n\r");

    return str.substr(start, end - start + 1);
}
