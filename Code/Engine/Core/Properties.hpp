/**
 * @file Properties.hpp
 * @brief Java Properties 格式解析器 - 支持 C 预处理器指令
 * @date 2025-10-12
 *
 * 核心特性:
 * 1. 标准 Java Properties 格式 (key=value)
 * 2. C 预处理器支持 (#ifdef, #if, #define, #elif, #else, #endif)
 * 3. 多行值支持 (\\ 行连续符)
 * 4. Unicode 转义 (\\uXXXX)
 * 5. 类型安全访问 (GetInt, GetFloat, GetBool)
 *
 * 教学要点:
 * - 参考 Iris shaders.properties 格式
 * - ComplementaryReimagined 预处理器指令示例
 * - 通用库设计 (不依赖 Shader Pack)
 *
 * 使用示例:
 * ```cpp
 * PropertiesFile props;
 * props.Load("shaders/shaders.properties");
 *
 * // 定义宏后重新解析 (#ifdef 条件编译)
 * props.Define("MC_VERSION", "11300");
 * props.Reload(); // 重新应用预处理器
 *
 * std::string name = props.Get("shaderpack.name", "Unknown");
 * int quality = props.GetInt("SHADOW_QUALITY", 2);
 * ```
 */

#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace enigma::core
{
    /**
     * @class PropertiesException
     * @brief Properties 解析异常
     */
    class PropertiesException : public std::runtime_error
    {
    public:
        explicit PropertiesException(const std::string& message)
            : std::runtime_error(message)
        {
        }
    };

    /**
     * @class PropertiesFile
     * @brief Java Properties 文件解析器 - 支持 C 预处理器
     *
     * 功能特性:
     * 1. **基础格式**:
     *    - key=value 键值对
     *    - # 注释（单行）
     *    - \\ 行连续符（跨行值）
     *    - \\uXXXX Unicode 转义
     *
     * 2. **预处理器** (Iris 扩展):
     *    - #ifdef MACRO
     *    - #if EXPRESSION (支持 ==, !=, >, <, >=, <=)
     *    - #elif EXPRESSION
     *    - #else
     *    - #endif
     *    - #define MACRO VALUE
     *
     * 3. **特殊语法**:
     *    - 空格分隔的多值 (sliders=SLIDER1 SLIDER2 ...)
     *    - <empty> 占位符
     *    - [CATEGORY] 方括号标记（视觉分隔）
     *
     * 教学要点:
     * - 两阶段解析: 预处理 → 键值对提取
     * - 宏定义栈管理 (#ifdef 嵌套)
     * - 表达式求值 (#if SHADOW_QUALITY > 0)
     */
    class PropertiesFile
    {
    public:
        PropertiesFile()  = default;
        ~PropertiesFile() = default;

        // 禁用拷贝
        PropertiesFile(const PropertiesFile&)            = delete;
        PropertiesFile& operator=(const PropertiesFile&) = delete;

        // 支持移动
        PropertiesFile(PropertiesFile&&) noexcept            = default;
        PropertiesFile& operator=(PropertiesFile&&) noexcept = default;

        // ========================================================================
        // 加载和解析
        // ========================================================================

        /**
         * @brief 从文件加载 properties
         * @param filepath 文件路径
         * @return true 成功, false 失败
         */
        bool Load(const std::filesystem::path& filepath);

        /**
         * @brief 从字符串加载 properties
         * @param content 文件内容
         * @return true 成功, false 失败
         */
        bool LoadFromString(const std::string& content);

        /**
         * @brief 重新应用预处理器 (宏定义改变后)
         * @details 保留原始文本,重新执行预处理和解析
         */
        void Reload();

        // ========================================================================
        // 预处理器宏定义
        // ========================================================================

        /**
         * @brief 定义预处理器宏
         * @param macro 宏名称 (例如: "MC_VERSION")
         * @param value 宏值 (例如: "11300")
         * @details 需要调用 Reload() 重新应用预处理器
         */
        void Define(const std::string& macro, const std::string& value);

        /**
         * @brief 取消定义预处理器宏
         * @param macro 宏名称
         */
        void Undefine(const std::string& macro);

        /**
         * @brief 检查宏是否已定义
         * @param macro 宏名称
         * @return true 已定义, false 未定义
         */
        bool IsDefined(const std::string& macro) const;

        // ========================================================================
        // 键值访问
        // ========================================================================

        /**
         * @brief 获取字符串值
         * @param key 键名
         * @param defaultValue 默认值
         * @return 值（如果不存在返回默认值）
         */
        std::string Get(const std::string& key, const std::string& defaultValue = "") const;

        /**
         * @brief 获取整数值
         * @param key 键名
         * @param defaultValue 默认值
         * @return 值（如果不存在或解析失败返回默认值）
         */
        int GetInt(const std::string& key, int defaultValue = 0) const;

        /**
         * @brief 获取浮点数值
         * @param key 键名
         * @param defaultValue 默认值
         * @return 值（如果不存在或解析失败返回默认值）
         */
        float GetFloat(const std::string& key, float defaultValue = 0.0f) const;

        /**
         * @brief 获取布尔值 (true/false, on/off, yes/no, 1/0)
         * @param key 键名
         * @param defaultValue 默认值
         * @return 值（如果不存在或解析失败返回默认值）
         */
        bool GetBool(const std::string& key, bool defaultValue = false) const;

        /**
         * @brief 获取可选值
         * @param key 键名
         * @return 值（如果不存在返回 nullopt）
         */
        std::optional<std::string> GetOptional(const std::string& key) const;

        /**
         * @brief 检查键是否存在
         * @param key 键名
         * @return true 存在, false 不存在
         */
        bool Contains(const std::string& key) const;

        // ========================================================================
        // 修改操作
        // ========================================================================

        /**
         * @brief 设置键值对
         * @param key 键名
         * @param value 值
         */
        void Set(const std::string& key, const std::string& value);

        /**
         * @brief 移除键值对
         * @param key 键名
         */
        void Remove(const std::string& key);

        /**
         * @brief 清空所有键值对（保留宏定义）
         */
        void Clear();

        // ========================================================================
        // 遍历和查询
        // ========================================================================

        /**
         * @brief 获取所有键
         * @return 键列表
         */
        std::vector<std::string> GetKeys() const;

        /**
         * @brief 获取所有键值对
         * @return 键值对映射
         */
        const std::unordered_map<std::string, std::string>& GetAll() const;

        /**
         * @brief 获取键值对数量
         * @return 数量
         */
        size_t Size() const;

        /**
         * @brief 检查是否为空
         * @return true 空, false 非空
         */
        bool Empty() const;

    private:
        // ========================================================================
        // 内部实现
        // ========================================================================

        /**
         * @brief 预处理文本 (处理 #ifdef, #if, #define 等)
         * @param content 原始内容
         * @return 预处理后的内容
         */
        std::string Preprocess(const std::string& content) const;

        /**
         * @brief 解析预处理后的文本为键值对
         * @param content 预处理后的内容
         */
        void ParseKeyValuePairs(const std::string& content);

        /**
         * @brief 解析单行 (处理行连续符 \\)
         * @param line 单行文本
         * @param lineNumber 行号 (用于错误报告)
         */
        void ParseLine(const std::string& line, size_t lineNumber);

        /**
         * @brief 求值预处理器表达式
         * @param expression 表达式 (例如: "SHADOW_QUALITY > 0")
         * @return true 真, false 假
         */
        bool EvaluateExpression(const std::string& expression) const;

        /**
         * @brief 去除字符串首尾空白
         * @param str 字符串
         * @return 去除空白后的字符串
         */
        static std::string Trim(const std::string& str);

        /**
         * @brief 处理 Unicode 转义 (\uXXXX)
         * @param str 包含转义的字符串
         * @return 解码后的字符串
         */
        static std::string UnescapeUnicode(const std::string& str);

    private:
        /// 键值对存储
        std::unordered_map<std::string, std::string> m_properties;

        /// 预处理器宏定义 (例如: "MC_VERSION" -> "11300")
        std::unordered_map<std::string, std::string> m_macros;

        /// 原始文本 (用于 Reload)
        std::string m_originalContent;

        /// 文件路径 (用于错误报告)
        std::filesystem::path m_filepath;
    };
} // namespace enigma::core
