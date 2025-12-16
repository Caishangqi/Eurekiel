/**
 * @file ShaderPackOptions.hpp
 * @brief Shader Pack 宏选项系统 - 对齐 Iris ShaderPackOptions.java
 * @date 2025-10-13
 *
 * 核心职责:
 * 1. 解析 shaderpack.properties 文件中的宏选项定义
 * 2. 管理 Boolean 和 String 两类选项
 * 3. 提供选项查询和修改接口
 * 4. 生成着色器预处理宏定义
 *
 * 教学要点:
 * - Iris 选项系统架构: OptionSet + BooleanOption + StringOption
 * - 宏选项格式: option.<name> = <value> [<allowed1> <allowed2> ...]
 * - 选项注释: option.<name>.comment = <description>
 * - 宏定义生成: Boolean → #define FLAG, String → #define KEY VALUE
 *
 * 参考 Iris 源码:
 * - ShaderPackOptions.java: F:\p4\Personal\SD\Thesis\ReferenceProject\Iris-multiloader-new\common\src\main\java\net\irisshaders\iris\shaderpack\option\ShaderPackOptions.java
 * - BooleanOption.java: 布尔选项定义
 * - StringOption.java: 字符串选项定义 (多选一)
 *
 * shaderpack.properties 典型内容示例:
 * ```properties
 * # Boolean 选项 (true/false)
 * option.SHADOW_ENABLED = true
 * option.SHADOW_ENABLED.comment = Enable shadow rendering
 *
 * # String 选项 (多选一，带允许值列表)
 * option.SHADOW_QUALITY = HIGH [LOW MEDIUM HIGH ULTRA]
 * option.SHADOW_QUALITY.comment = Shadow map resolution quality
 *
 * # 更多选项示例
 * option.BLOOM_ENABLED = false
 * option.CLOUD_TYPE = VANILLA [VANILLA CUSTOM OFF]
 * ```
 *
 * 使用示例:
 * ```cpp
 * // 1. 创建并解析选项
 * ShaderPackOptions options;
 * options.Parse(shaderPackRoot);
 *
 * // 2. 查询选项
 * if (options.HasOption("SHADOW_ENABLED")) {
 *     auto option = options.GetOption("SHADOW_ENABLED");
 *     // option->currentValue == "true"
 * }
 *
 * // 3. 修改选项 (运行时)
 * options.SetOptionValue("SHADOW_QUALITY", "ULTRA");
 *
 * // 4. 生成宏定义 (用于 DXC 编译器)
 * auto macros = options.GetMacroDefinitions();
 * // macros["SHADOW_ENABLED"] = ""        (定义为空，#define SHADOW_ENABLED)
 * // macros["SHADOW_QUALITY"] = "ULTRA"   (#define SHADOW_QUALITY ULTRA)
 * ```
 */

#pragma once

#include "Engine/Core/Properties.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <filesystem>
#include <sstream>
#include <algorithm>

namespace enigma::graphic
{
    // ========================================================================
    // 选项类型枚举
    // ========================================================================

    /**
     * @enum OptionType
     * @brief 宏选项类型
     *
     * 对齐 Iris 选项系统:
     * - BOOLEAN: 布尔选项 (true/false)，控制功能开关
     * - STRING: 字符串选项 (多选一)，控制多级设置
     */
    enum class OptionType
    {
        BOOLEAN, ///< 布尔选项 (true/false)
        STRING ///< 字符串选项 (多选一，例如: LOW/MEDIUM/HIGH)
    };

    // ========================================================================
    // 选项值结构体
    // ========================================================================

    /**
     * @struct OptionValue
     * @brief 单个宏选项的完整描述
     *
     * 数据成员:
     * - name: 选项名称 (例如: "SHADOW_ENABLED", "SHADOW_QUALITY")
     * - type: 选项类型 (BOOLEAN 或 STRING)
     * - currentValue: 当前值 (运行时可修改)
     * - defaultValue: 默认值 (从 shaderpack.properties 读取)
     * - allowedValues: 允许的值列表 (仅 STRING 选项)
     * - comment: 选项描述 (用于 UI 显示)
     *
     * 教学要点:
     * - Boolean 选项: allowedValues 固定为 ["true", "false"]
     * - String 选项: allowedValues 从 [...] 括号内解析
     * - 注释分离存储，支持多语言 UI
     */
    struct OptionValue
    {
        std::string              name; ///< 选项名称 (例如: "SHADOW_ENABLED")
        OptionType               type; ///< 选项类型 (BOOLEAN/STRING)
        std::string              currentValue; ///< 当前值 (可修改)
        std::string              defaultValue; ///< 默认值 (不可变)
        std::vector<std::string> allowedValues; ///< 允许的值列表 (仅 STRING 选项)
        std::string              comment; ///< 选项描述 (UI 显示)

        /**
         * @brief 默认构造函数
         */
        OptionValue() : type(OptionType::BOOLEAN)
        {
        }
    };

    // ========================================================================
    // ShaderPackOptions 类定义
    // ========================================================================

    /**
     * @class ShaderPackOptions
     * @brief Shader Pack 宏选项管理器 - 对齐 Iris 架构
     *
     * 核心功能:
     * 1. **解析 shaderpack.properties**: 支持 Boolean/String 选项
     * 2. **选项查询**: 按名称查询选项值和元数据
     * 3. **选项修改**: 运行时修改选项值 (支持热重载)
     * 4. **宏定义生成**: 转换为 DXC 编译器宏参数
     *
     * 与 Iris 对齐:
     * - 对应 Iris ShaderPackOptions.java
     * - 支持 OptionSet (选项集合) 概念
     * - 支持 BooleanOption 和 StringOption
     *
     * 文件格式:
     * ```properties
     * # 定义选项及默认值
     * option.SHADOW_ENABLED = true
     * option.SHADOW_QUALITY = HIGH [LOW MEDIUM HIGH ULTRA]
     *
     * # 定义选项注释 (可选)
     * option.SHADOW_ENABLED.comment = Enable shadow rendering
     * option.SHADOW_QUALITY.comment = Shadow map resolution quality
     * ```
     *
     * 教学要点:
     * - **组合模式**: 使用 PropertiesFile 基类加载文件
     * - **宏定义转换**: Boolean → #define FLAG, String → #define KEY VALUE
     * - **验证机制**: 检查值是否在 allowedValues 中
     * - **静默失败**: 对齐 Iris 宽松策略，解析失败不中断初始化
     */
    class ShaderPackOptions
    {
    public:
        // ====================================================================
        // 构造和销毁
        // ====================================================================

        /**
         * @brief 默认构造函数
         */
        ShaderPackOptions();

        /**
         * @brief 析构函数
         */
        ~ShaderPackOptions() = default;

        // 禁用拷贝
        ShaderPackOptions(const ShaderPackOptions&)            = delete;
        ShaderPackOptions& operator=(const ShaderPackOptions&) = delete;

        // 支持移动
        ShaderPackOptions(ShaderPackOptions&&) noexcept            = default;
        ShaderPackOptions& operator=(ShaderPackOptions&&) noexcept = default;

        // ====================================================================
        // 加载和解析
        // ====================================================================

        /**
         * @brief 从 Shader Pack 根目录加载 shaderpack.properties
         * @param rootPath Shader Pack 根目录路径
         * @return true 成功, false 失败
         *
         * 流程:
         * 1. 构造路径: rootPath / "shaderpack.properties"
         * 2. 检查文件存在性 (不存在不算错误，对齐 Iris 宽松策略)
         * 3. 调用 PropertiesFile::Load() 加载文件
         * 4. 遍历所有键值对，调用 ParseDirective() 解析
         *
         * 注意:
         * - shaderpack.properties 是可选文件
         * - 解析失败不中断初始化 (对齐 Iris 行为)
         */
        bool Parse(const std::filesystem::path& rootPath);

        /**
         * @brief 从字符串加载选项定义 (用于单元测试)
         * @param content 文件内容
         * @return true 成功, false 失败
         */
        bool ParseFromString(const std::string& content);

        // ====================================================================
        // 选项查询
        // ====================================================================

        /**
         * @brief 检查选项是否存在
         * @param name 选项名称 (例如: "SHADOW_ENABLED")
         * @return true 存在, false 不存在
         */
        bool HasOption(const std::string& name) const;

        /**
         * @brief 获取选项值结构体
         * @param name 选项名称
         * @return 选项值 (如果不存在返回 nullopt)
         */
        std::optional<OptionValue> GetOption(const std::string& name) const;

        /**
         * @brief 获取所有选项
         * @return 选项映射 (name -> OptionValue)
         */
        const std::unordered_map<std::string, OptionValue>& GetAllOptions() const;

        /**
         * @brief 获取选项数量
         * @return 选项总数
         */
        size_t GetOptionCount() const;

        // ====================================================================
        // 选项修改 (运行时)
        // ====================================================================

        /**
         * @brief 设置选项值 (支持运行时修改)
         * @param name 选项名称
         * @param value 新值
         * @return true 成功, false 失败 (选项不存在或值不合法)
         *
         * 验证规则:
         * - 选项必须存在
         * - 值必须在 allowedValues 列表中
         */
        bool SetOptionValue(const std::string& name, const std::string& value);

        /**
         * @brief 重置选项为默认值
         * @param name 选项名称
         * @return true 成功, false 失败 (选项不存在)
         */
        bool ResetOptionToDefault(const std::string& name);

        /**
         * @brief 重置所有选项为默认值
         */
        void ResetAllToDefaults();

        // ====================================================================
        // 宏定义生成 (用于着色器预处理)
        // ====================================================================

        /**
         * @brief 生成宏定义映射 (用于 DXC 编译器)
         * @return 宏定义映射 (name -> value)
         *
         * 转换规则:
         * - Boolean 选项:
         *   - true → 定义为空 (#define SHADOW_ENABLED)
         *   - false → 不添加到映射 (不定义)
         * - String 选项:
         *   - 定义为选项值 (#define SHADOW_QUALITY HIGH)
         *
         * 示例输出:
         * ```cpp
         * {
         *     "SHADOW_ENABLED": "",      // #define SHADOW_ENABLED
         *     "SHADOW_QUALITY": "HIGH"   // #define SHADOW_QUALITY HIGH
         * }
         * ```
         */
        std::unordered_map<std::string, std::string> GetMacroDefinitions() const;

        // ====================================================================
        // 验证和状态
        // ====================================================================

        /**
         * @brief 检查选项系统是否有效
         * @return true 有效, false 无效
         */
        bool IsValid() const;

        /**
         * @brief 清空所有选项 (用于重新加载)
         */
        void Clear();

    private:
        // ====================================================================
        // 内部解析方法
        // ====================================================================

        /**
         * @brief 解析单个指令行 (分发器)
         * @param key 键名 (例如: "option.SHADOW_ENABLED")
         * @param value 值 (例如: "true")
         *
         * 识别指令类型:
         * - option.<name>: 选项定义 → 调用 ParseOptionDefinition()
         * - option.<name>.comment: 注释定义 → 更新已存在选项的 comment
         * - 其他: 静默忽略 (对齐 Iris 行为)
         */
        void ParseDirective(const std::string& key, const std::string& value);

        /**
         * @brief 解析选项定义 (核心逻辑)
         * @param name 选项名称 (已去除 "option." 前缀)
         * @param value 选项值 (可能包含 [...] 允许值列表)
         *
         * 解析规则:
         * 1. 检查是否有 [...] 括号:
         *    - 有 → String 选项，提取默认值和允许值列表
         *    - 无 → Boolean 选项，值直接作为默认值
         * 2. 创建 OptionValue 结构体并存储
         *
         * 示例:
         * - "true" → Boolean 选项，默认值 "true"
         * - "HIGH [LOW MEDIUM HIGH ULTRA]" → String 选项，默认值 "HIGH"，允许 4 个值
         */
        void ParseOptionDefinition(const std::string& name, const std::string& value);

        /**
         * @brief 分割字符串 (按空格)
         * @param value 输入字符串
         * @return 分割后的字符串列表
         *
         * 用途:
         * - 解析允许值列表: "LOW MEDIUM HIGH ULTRA" → ["LOW", "MEDIUM", "HIGH", "ULTRA"]
         */
        static std::vector<std::string> SplitWhitespace(const std::string& value);

        /**
         * @brief 去除字符串首尾空白
         * @param str 输入字符串
         * @return 去除空白后的字符串
         */
        static std::string Trim(const std::string& str);

    private:
        // ====================================================================
        // 内部数据成员
        // ====================================================================

        /// PropertiesFile 基类实例 (组合模式)
        core::PropertiesFile m_propertiesFile;

        /// 选项映射 (name -> OptionValue)
        std::unordered_map<std::string, OptionValue> m_options;

        /// 有效性标志
        bool m_isValid;
    };
} // namespace enigma::graphic
