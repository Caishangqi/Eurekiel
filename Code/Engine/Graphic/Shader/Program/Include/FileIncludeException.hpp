#pragma once

#include <stdexcept>
#include <string>
#include <optional>

namespace enigma::graphic
{
    /**
     * @class FileIncludeException
     * @brief Include 系统专用异常（找不到包含文件）
     *
     * **设计原则**：
     * - 继承 std::runtime_error（标准异常层次）
     * - 存储完整上下文信息（源文件、行号、目标路径）
     * - 格式化错误消息，便于调试
     *
     * **对应 Iris 源码**：
     * - Iris: FileIncludeException.java (extends NoSuchFileException)
     * - 职责：报告 #include 指令无法解析的错误
     *
     * **使用场景**：
     * - IncludeGraph 构建时找不到依赖文件
     * - IncludeProcessor 展开时路径解析失败
     * - ShaderBundleLoader file not found
     *
     * **示例**：
     * ```cpp
     * throw FileIncludeException(
     *     "/shaders/gbuffers_terrain.vsh",
     *     42,
     *     "../lib/common.hlsl",
     *     "File not found in shader bundle"
     * );
     * // 输出: Include error in '/shaders/gbuffers_terrain.vsh:42':
     * //       Cannot resolve '../lib/common.hlsl' - File not found in shader bundle
     * ```
     */
    class FileIncludeException : public std::runtime_error
    {
    public:
        /**
         * @brief 完整构造函数（带行号）
         * @param sourceFile Source file path that caused the error (virtual path)
         * @param lineNumber #include 指令所在行号（1-based）
         * @param targetPath 尝试包含的目标路径（可能是相对路径）
         * @param reason 失败原因描述
         *
         * 教学要点：
         * - 使用成员初始化列表调用基类构造函数
         * - FormatMessage() 生成格式化错误消息
         * - std::optional 表示可选的行号
         */
        FileIncludeException(
            const std::string& sourceFile,
            int                lineNumber,
            const std::string& targetPath,
            const std::string& reason
        )
            : std::runtime_error(FormatMessage(sourceFile, lineNumber, targetPath, reason))
              , m_sourceFile(sourceFile)
              , m_lineNumber(lineNumber)
              , m_targetPath(targetPath)
              , m_reason(reason)
        {
        }

        /**
         * @brief 简化构造函数（无行号）
         * @param sourceFile 出错的源文件路径
         * @param targetPath 尝试包含的目标路径
         * @param reason 失败原因描述
         *
         * 教学要点：构造函数重载，行号设为 std::nullopt
         */
        FileIncludeException(
            const std::string& sourceFile,
            const std::string& targetPath,
            const std::string& reason
        )
            : std::runtime_error(FormatMessageNoLine(sourceFile, targetPath, reason))
              , m_sourceFile(sourceFile)
              , m_lineNumber(std::nullopt)
              , m_targetPath(targetPath)
              , m_reason(reason)
        {
        }

        // ========================================================================
        // 访问器 - 获取异常上下文信息
        // ========================================================================

        /**
         * @brief 获取出错的源文件路径
         * @return Virtual path (e.g. `/shaders/gbuffers_terrain.vsh`）
         */
        const std::string& GetSourceFile() const { return m_sourceFile; }

        /**
         * @brief 获取 #include 指令所在行号
         * @return 行号（1-based），如果未指定返回 std::nullopt
         */
        std::optional<int> GetLineNumber() const { return m_lineNumber; }

        /**
         * @brief 获取尝试包含的目标路径
         * @return 相对或绝对路径（如 `../lib/common.hlsl`）
         */
        const std::string& GetTargetPath() const { return m_targetPath; }

        /**
         * @brief 获取失败原因描述
         * @return 错误原因字符串
         */
        const std::string& GetReason() const { return m_reason; }

    private:
        /**
         * @brief 格式化错误消息（带行号）
         * @return 格式化字符串
         *
         * 格式: "Include error in '<sourceFile>:<lineNumber>': Cannot resolve '<targetPath>' - <reason>"
         *
         * 教学要点: 静态辅助函数，生成清晰的错误消息
         */
        static std::string FormatMessage(
            const std::string& sourceFile,
            int                lineNumber,
            const std::string& targetPath,
            const std::string& reason
        )
        {
            return "Include error in '" + sourceFile + ":" + std::to_string(lineNumber) +
                "': Cannot resolve '" + targetPath + "' - " + reason;
        }

        /**
         * @brief 格式化错误消息（无行号）
         * @return 格式化字符串
         *
         * 格式: "Include error in '<sourceFile>': Cannot resolve '<targetPath>' - <reason>"
         */
        static std::string FormatMessageNoLine(
            const std::string& sourceFile,
            const std::string& targetPath,
            const std::string& reason
        )
        {
            return "Include error in '" + sourceFile +
                "': Cannot resolve '" + targetPath + "' - " + reason;
        }

    private:
        std::string        m_sourceFile; // 出错的源文件路径
        std::optional<int> m_lineNumber; // #include 指令行号（可选）
        std::string        m_targetPath; // 尝试包含的目标路径
        std::string        m_reason; // 失败原因

        /**
         * 教学要点总结：
         * 1. **继承标准异常**: 遵循 C++ 异常层次，兼容 std::exception
         * 2. **格式化错误消息**: 静态辅助函数生成清晰的错误信息
         * 3. **上下文信息存储**: 保存所有必要的调试信息
         * 4. **std::optional 应用**: 表示可选的行号信息
         * 5. **构造函数重载**: 支持带/不带行号两种创建方式
         */
    };
} // namespace enigma::graphic
