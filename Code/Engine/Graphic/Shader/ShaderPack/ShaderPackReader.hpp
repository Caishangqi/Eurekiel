/**
 * @file ShaderPackReader.hpp
 * @brief ShaderPack文件读取器 - 从ShaderPack读取着色器文件
 * @date 2025-11-04
 * @author Caizii
 *
 * 架构说明:
 * ShaderPackReader 实现IFileReader接口，为Shader Include系统提供从ShaderPack读取文件的能力。
 * 这是Include系统重构的核心组件之一，负责将ShaderPack虚拟路径转换为文件系统路径并读取文件。
 *
 * 设计原则:
 * - 单一职责: 专注于ShaderPack文件读取
 * - 依赖倒置: 实现IFileReader抽象接口
 * - 路径转换: 使用ShaderPath::Resolved()进行路径转换
 * - 异常安全: 使用std::optional处理失败情况
 *
 * 职责边界:
 * - + ShaderPack文件读取 (ReadFile)
 * - + 文件存在性检查 (FileExists)
 * - + 根路径管理 (GetRootPath)
 * - + 虚拟路径到文件系统路径转换
 * - ❌ 不包含文件缓存逻辑 (可由上层添加)
 * - ❌ 不包含Include依赖解析 (由IncludeGraph负责)
 *
 * 使用示例:
 * @code
 * // 创建ShaderPackReader
 * auto packRoot = std::filesystem::path("F:/shaderpacks/MyPack");
 * auto reader = std::make_unique<ShaderPackReader>(packRoot);
 *
 * // 读取文件
 * auto path = ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.hlsl");
 * auto content = reader->ReadFile(path);
 * if (content) {
 *     // 处理文件内容
 *     ProcessShaderSource(*content);
 * } else {
 *     // 文件读取失败
 *     LogError("Failed to read shader file");
 * }
 *
 * // 检查文件是否存在
 * if (reader->FileExists(path)) {
 *     // 文件存在，可以安全读取
 * }
 * @endcode
 *
 * 教学要点:
 * - 接口实现模式
 * - 路径转换策略
 * - RAII资源管理
 * - std::optional语义
 * - 依赖注入原则
 */

#pragma once

#include "Engine/Graphic/Shader/Common/IFileReader.hpp"
#include "Engine/Graphic/Shader/ShaderPack/Include/ShaderPath.hpp"
#include <filesystem>
#include <string>
#include <optional>

namespace enigma::graphic
{
    /**
     * @class ShaderPackReader
     * @brief ShaderPack文件读取器实现
     *
     * 设计理念:
     * - 具体实现: 实现IFileReader接口的具体文件读取逻辑
     * - 路径转换: 将ShaderPath虚拟路径转换为文件系统路径
     * - 简单直接: 直接读取文件系统中的文件
     * - 兼容性: 与现有ShaderPackLoader完全兼容
     *
     * 实现细节:
     * - 存储ShaderPack根路径
     * - 使用ShaderPath::Resolved()进行路径转换
     * - 使用std::filesystem检查文件存在性
     * - 使用std::ifstream读取文件内容
     *
     * 线程安全性:
     * - 读取操作线程安全（只读根路径）
     * - 可以安全地并发读取不同文件
     * - 建议每个线程创建独立的ShaderPackReader实例
     */
    class ShaderPackReader : public IFileReader
    {
    public:
        /**
         * @brief 构造ShaderPackReader
         * @param packRoot ShaderPack根目录（文件系统路径）
         *
         * 设计说明:
         * - 存储ShaderPack的根目录
         * - 根路径用于将虚拟路径转换为文件系统路径
         * - 不验证根路径的有效性（延迟到首次使用）
         *
         * 使用示例:
         * @code
         * auto packRoot = std::filesystem::path("F:/shaderpacks/MyPack");
         * auto reader = std::make_unique<ShaderPackReader>(packRoot);
         * @endcode
         *
         * 教学要点:
         * - explicit构造函数：防止隐式转换
         * - 成员初始化：使用初始化列表
         * - 依赖注入：通过构造函数注入根路径
         */
        explicit ShaderPackReader(const std::filesystem::path& packRoot);

        /**
         * @brief 虚析构函数
         *
         * 设计说明:
         * - 使用默认实现
         * - 确保通过基类指针正确销毁
         * - 遵循C++最佳实践
         */
        ~ShaderPackReader() override = default;

        // ========================================================================
        // IFileReader接口实现
        // ========================================================================

        /**
         * @brief 读取ShaderPack中的文件
         * @param path Shader Pack 路径（Unix风格绝对路径）
         * @return std::optional<std::string> 成功返回文件内容，失败返回nullopt
         *
         * 实现流程:
         * 1. 使用ShaderPath::Resolved()将虚拟路径转换为文件系统路径
         * 2. 检查文件是否存在
         * 3. 打开并读取文件内容
         * 4. 返回文件内容或nullopt（失败时）
         *
         * 失败情况:
         * - 文件不存在
         * - 文件无法打开（权限问题等）
         * - 读取错误
         *
         * 示例:
         * @code
         * auto path = ShaderPath::FromAbsolutePath("/shaders/common.hlsl");
         * auto content = reader->ReadFile(path);
         * if (content) {
         *     std::cout << "File content: " << *content << std::endl;
         * }
         * @endcode
         *
         * 教学要点:
         * - 虚函数重写：override关键字
         * - 路径转换：虚拟路径→文件系统路径
         * - 错误处理：使用optional表示可能的失败
         * - 文件IO：标准库文件操作
         */
        std::optional<std::string> ReadFile(const ShaderPath& path) override;

        /**
         * @brief 检查ShaderPack中的文件是否存在
         * @param path Shader Pack 路径（Unix风格绝对路径）
         * @return bool 文件存在返回true，否则返回false
         *
         * 实现流程:
         * 1. 使用ShaderPath::Resolved()将虚拟路径转换为文件系统路径
         * 2. 使用std::filesystem::exists()检查文件存在性
         *
         * 使用场景:
         * - Include依赖图构建
         * - 文件预检查
         * - 错误诊断
         *
         * 示例:
         * @code
         * auto path = ShaderPath::FromAbsolutePath("/shaders/common.hlsl");
         * if (reader->FileExists(path)) {
         *     // 文件存在，可以读取
         * }
         * @endcode
         *
         * 教学要点:
         * - 虚函数重写：override关键字
         * - 常量成员函数：const修饰符
         * - 快速检查：避免实际读取文件
         */
        bool FileExists(const ShaderPath& path) const override;

        /**
         * @brief 获取ShaderPack根目录
         * @return std::filesystem::path ShaderPack根路径
         *
         * 实现说明:
         * - 直接返回存储的根路径
         * - 用于路径转换和调试
         *
         * 示例:
         * @code
         * auto rootPath = reader->GetRootPath();
         * std::cout << "ShaderPack root: " << rootPath << std::endl;
         * @endcode
         *
         * 教学要点:
         * - 虚函数重写：override关键字
         * - 常量成员函数：const修饰符
         * - 封装原则：通过getter访问私有成员
         */
        std::filesystem::path GetRootPath() const override;

    private:
        /**
         * @brief ShaderPack根目录（文件系统路径）
         *
         * 设计说明:
         * - 存储ShaderPack的文件系统根路径
         * - 用于将虚拟路径转换为实际文件路径
         * - 不可变对象（构造后不修改）
         *
         * 路径示例:
         * - "F:/shaderpacks/ComplementaryReimagined/"
         * - "C:/Users/Admin/shaderpacks/BSL/"
         *
         * 教学要点:
         * - 私有成员变量
         * - 不可变对象模式
         * - 数据封装
         */
        std::filesystem::path m_packRoot;

        /**
         * 教学要点总结:
         * 1. **接口实现模式**: 实现IFileReader抽象接口
         * 2. **路径转换策略**: ShaderPath虚拟路径→文件系统路径
         * 3. **依赖倒置原则**: 依赖抽象而非具体实现
         * 4. **单一职责原则**: 专注于ShaderPack文件读取
         * 5. **异常安全**: 使用optional而非异常处理失败
         */
    };
} // namespace enigma::graphic
