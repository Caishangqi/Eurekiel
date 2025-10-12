#pragma once

#include <string>
#include <optional>
#include <filesystem>

namespace enigma::graphic
{
    /**
     * @class AbsolutePackPath
     * @brief Shader Pack 内部绝对路径表示（Unix 风格）
     *
     * **设计原则**：
     * - 所有路径以 `/` 开头（Unix 风格绝对路径）
     * - 使用 `/` 作为路径分隔符（避免 Windows/Linux 差异）
     * - 不可变对象（线程安全）
     * - 自动标准化路径（处理 `.` 和 `..`）
     *
     * **对应 Iris 源码**：
     * - Iris: AbsolutePackPath.java
     * - 职责：统一 Shader Pack 内部路径表示
     *
     * **使用场景**：
     * - Include 依赖图管理（IncludeGraph）
     * - 着色器文件查找（ShaderPackLoader）
     * - 相对路径解析（#include "../common.hlsl"）
     *
     * **示例**：
     * ```cpp
     * auto path1 = AbsolutePackPath::FromAbsolutePath("/shaders/gbuffers_terrain.hlsl");
     * auto path2 = path1.Resolve("../lib/common.hlsl");
     * // 结果: /shaders/lib/common.hlsl
     * ```
     */
    class AbsolutePackPath
    {
    public:
        /**
         * @brief 从绝对路径字符串创建 AbsolutePackPath
         * @param absolutePath 必须以 `/` 开头的绝对路径
         * @return 标准化后的 AbsolutePackPath
         * @throws std::invalid_argument 如果路径不是绝对路径
         *
         * 教学要点：
         * - 工厂方法模式（静态工厂）
         * - 验证输入（必须以 `/` 开头）
         * - 自动标准化（处理 `.` 和 `..`）
         */
        static AbsolutePackPath FromAbsolutePath(const std::string& absolutePath);

        /**
         * @brief 获取父路径
         * @return 如果有父路径返回 Optional，根路径 `/` 返回空
         *
         * 示例：
         * - `/shaders/lib/common.hlsl` → `/shaders/lib`
         * - `/shaders` → `/`
         * - `/` → std::nullopt
         */
        std::optional<AbsolutePackPath> Parent() const;

        /**
         * @brief 解析相对路径
         * @param relativePath 相对路径（如 `../common.hlsl`）
         * @return 解析后的绝对路径
         *
         * 业务逻辑：
         * - 如果 relativePath 以 `/` 开头，直接返回新的绝对路径
         * - 否则，基于当前路径解析相对路径
         * - 自动处理 `.` 和 `..`
         *
         * 示例：
         * - 当前路径: `/shaders/gbuffers_terrain.hlsl`
         * - 解析 `../lib/common.hlsl`
         * - 结果: `/shaders/lib/common.hlsl`
         */
        AbsolutePackPath Resolve(const std::string& relativePath) const;

        /**
         * @brief 转换为文件系统路径
         * @param root Shader Pack 根目录（文件系统路径）
         * @return 完整的文件系统路径
         *
         * 示例：
         * - Shader Pack 路径: `/shaders/gbuffers_terrain.hlsl`
         * - 根目录: `F:/shaderpacks/ComplementaryReimagined/`
         * - 结果: `F:/shaderpacks/ComplementaryReimagined/shaders/gbuffers_terrain.hlsl`
         */
        std::filesystem::path Resolved(const std::filesystem::path& root) const;

        /**
         * @brief 获取路径字符串（Unix 风格）
         * @return 路径字符串（始终以 `/` 开头）
         */
        std::string GetPathString() const { return m_path; }

        // ========================================================================
        // 标准比较运算符
        // ========================================================================
        bool operator==(const AbsolutePackPath& other) const { return m_path == other.m_path; }
        bool operator!=(const AbsolutePackPath& other) const { return m_path != other.m_path; }

        // ========================================================================
        // 调试支持
        // ========================================================================
        std::string ToString() const { return "AbsolutePackPath {" + m_path + "}"; }

    private:
        /**
         * @brief 私有构造函数（使用工厂方法）
         * @param normalizedPath 已标准化的路径
         */
        explicit AbsolutePackPath(const std::string& normalizedPath);

        /**
         * @brief 标准化绝对路径
         * @param path 原始路径（必须以 `/` 开头）
         * @return 标准化后的路径
         *
         * 标准化规则：
         * 1. 移除空段和 `.` 段
         * 2. 处理 `..` 段（回到上级目录）
         * 3. 移除多余的 `/` 分隔符
         *
         * 示例：
         * - `/shaders/./lib/../common.hlsl` → `/shaders/common.hlsl`
         * - `/shaders//lib/common.hlsl` → `/shaders/lib/common.hlsl`
         * - `/` → `/`
         */
        static std::string NormalizeAbsolutePath(const std::string& path);

    private:
        std::string m_path; // 标准化后的路径（不可变）

        /**
         * 教学要点总结：
         * 1. **不可变对象设计**：m_path 为 private + 无 setter
         * 2. **工厂方法模式**：私有构造函数 + 公有静态工厂
         * 3. **Unix 风格路径**：统一使用 `/`，避免平台差异
         * 4. **路径标准化**：自动处理 `.` 和 `..`，简化使用
         * 5. **std::optional 应用**：Parent() 可能无父路径
         */
    };
} // namespace enigma::graphic

// ========================================================================
// AbsolutePackPath 哈希支持（用于 unordered_map/set）
// ========================================================================

namespace std
{
    /**
     * @brief std::hash 特化，使 AbsolutePackPath 可以用于 unordered_map/set
     *
     * 教学要点:
     * - std::hash 特化应该与类定义在同一个头文件中
     * - 使用路径字符串的哈希值作为 AbsolutePackPath 的哈希值
     * - 这样 unordered_map<AbsolutePackPath, T> 才能正常工作
     */
    template <>
    struct hash<enigma::graphic::AbsolutePackPath>
    {
        std::size_t operator()(const enigma::graphic::AbsolutePackPath& path) const noexcept
        {
            return std::hash<std::string>()(path.GetPathString());
        }
    };
} // namespace std
