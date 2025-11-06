/**
 * @file IFileReader.hpp
 * @brief 文件读取抽象接口 - 为Shader Include系统提供文件读取抽象层
 * @date 2025-11-04
 * @author Caizii
 *
 * 架构说明:
 * IFileReader 提供文件读取的抽象接口，支持不同的文件读取实现。
 * 这是Include系统重构的核心抽象层，为解耦文件访问逻辑奠定基础。
 *
 * 设计原则:
 * - 纯抽象接口: 仅定义契约，不包含具体实现
 * - 前向声明: 避免与ShaderPath类的循环依赖
 * - 异常安全: 使用std::optional表示可能失败的操作
 * - 扩展性: 支持未来多种文件读取实现（本地文件、内存文件、网络文件等）
 *
 * 职责边界:
 * - + 文件读取抽象 (ReadFile)
 * - + 文件存在性检查 (FileExists)
 * - + 根路径获取 (GetRootPath)
 * - ❌ 不包含具体文件IO实现 (由子类实现)
 * - ❌ 不包含缓存逻辑 (可由具体实现添加)
 *
 * 使用示例:
 * @code
 * // 具体实现示例 (未来)
 * class LocalFileReader : public IFileReader {
 * public:
 *     std::optional<std::string> ReadFile(const ShaderPath& path) override {
 *         auto fsPath = path.Resolved(GetRootPath());
 *         return ShaderCompilationHelper::ReadShaderSourceFromFile(fsPath);
 *     }
 *     
 *     bool FileExists(const ShaderPath& path) const override {
 *         auto fsPath = path.Resolved(GetRootPath());
 *         return std::filesystem::exists(fsPath);
 *     }
 *     
 *     std::filesystem::path GetRootPath() const override {
 *         return m_rootPath;
 *     }
 * };
 *
 * // 使用示例
 * auto reader = std::make_unique<LocalFileReader>(shaderPackRoot);
 * auto content = reader->ReadFile(shaderPath);
 * if (content) {
 *     // 处理文件内容
 * }
 * @endcode
 *
 * 教学要点:
 * - 接口隔离原则 (Interface Segregation Principle)
 * - 依赖倒置原则 (Dependency Inversion Principle)
 * - 策略模式 (Strategy Pattern)
 * - RAII资源管理
 * - std::optional语义
 */

#pragma once

#include <optional>
#include <string>
#include <filesystem>

namespace enigma::graphic
{
    // 前向声明避免循环依赖
    class ShaderPath;

    /**
     * @interface IFileReader
     * @brief 文件读取抽象接口
     *
     * 设计理念:
     * - 纯虚接口: 定义文件读取的通用契约
     * - 统一接口: 为Include系统提供一致的文件访问方式
     * - 扩展友好: 支持多种文件读取策略
     *
     * 接口设计原则:
     * - 最小化接口: 只包含必要的方法
     * - 语义清晰: 每个方法的职责明确
     * - 异常安全: 使用optional而非异常处理失败情况
     *
     * 未来扩展方向:
     * - LocalFileReader: 本地文件系统读取
     * - MemoryFileReader: 内存缓存读取
     * - NetworkFileReader: 网络资源读取
     * - CompressedFileReader: 压缩文件读取
     */
    class IFileReader
    {
    public:
        /**
         * @brief 虚析构函数
         *
         * 教学要点:
         * - 虚析构函数确保通过基类指针删除派生类对象时正确调用析构函数
         * - 默认实现使用 = default，现代C++推荐做法
         * - RAII原则: 资源获取即初始化
         */
        virtual ~IFileReader() = default;

        /**
         * @brief 读取文件内容
         * @param path Shader Pack 路径 (Unix风格绝对路径)
         * @return std::optional<std::string> 成功返回文件内容，失败返回nullopt
         *
         * 语义说明:
         * - 读取指定路径的完整文件内容
         * - 以文本形式返回文件内容
         * - 如果文件不存在或读取失败，返回nullopt
         *
         * 错误处理策略:
         * - 文件不存在 → 返回 nullopt
         * - 权限不足 → 返回 nullopt
         * - 读取错误 → 返回 nullopt
         * - 编码问题 → 返回 nullopt
         *
         * 教学要点:
         * - std::optional表示可能失败的操作
         * - 避免使用异常处理预期的错误情况
         * - 明确的错误处理语义
         *
         * @code
         * auto reader = CreateFileReader();
         * auto content = reader.ReadFile(ShaderPath::FromAbsolutePath("/shaders/common.hlsl"));
         * if (content) {
         *     std::cout << "文件内容: " << *content << std::endl;
         * } else {
         *     std::cout << "文件读取失败" << std::endl;
         * }
         * @endcode
         */
        virtual std::optional<std::string> ReadFile(const ShaderPath& path) = 0;

        /**
         * @brief 检查文件是否存在
         * @param path Shader Pack 路径 (Unix风格绝对路径)
         * @return bool 文件存在返回true，否则返回false
         *
         * 语义说明:
         * - 检查指定路径的文件是否存在
         * - 不读取文件内容，仅检查存在性
         * - 用于Include系统的预处理和依赖检查
         *
         * 使用场景:
         * - Include依赖图构建前的存在性验证
         * - 循环依赖检测
         * - 增量编译的文件变更检测
         *
         * 教学要点:
         * - 常量成员函数: 不修改对象状态
         * - 快速检查: 避免不必要的文件IO
         * - 布尔返回值: 明确的语义
         *
         * @code
         * auto reader = CreateFileReader();
         * auto path = ShaderPath::FromAbsolutePath("/shaders/common.hlsl");
         * if (reader.FileExists(path)) {
         *     // 文件存在，可以安全读取
         *     auto content = reader.ReadFile(path);
         * } else {
         *     // 文件不存在，处理错误情况
         *     HandleMissingFile(path);
         * }
         * @endcode
         */
        virtual bool FileExists(const ShaderPath& path) const = 0;

        /**
         * @brief 获取文件读取器的根路径
         * @return std::filesystem::path 根路径 (文件系统路径)
         *
         * 语义说明:
         * - 返回文件读取器的基础路径
         * - 用于将Shader Pack路径转换为文件系统路径
         * - 所有相对路径都基于此根路径解析
         *
         * 使用场景:
         * - Shader Pack根目录定位
         * - 路径转换和解析
         * - 资源管理和定位
         *
         * 路径类型说明:
         * - 返回std::filesystem::path (平台相关路径)
         * - 与ShaderPath::Resolved()方法配合使用
         * - ShaderPath (Unix风格) + 根路径 → 完整文件系统路径
         *
         * 教学要点:
         * - 常量成员函数: 不修改对象状态
         * - 路径抽象: 隐藏平台差异
         * - 资源定位: 提供统一的路径基础
         *
         * @code
         * auto reader = CreateFileReader();
         * auto rootPath = reader.GetRootPath(); // 例如: "F:/shaderpacks/MyPack/"
         * auto shaderPath = ShaderPath::FromAbsolutePath("/shaders/main.hlsl");
         * auto fullPath = shaderPath.Resolved(rootPath); // "F:/shaderpacks/MyPack/shaders/main.hlsl"
         * @endcode
         */
        virtual std::filesystem::path GetRootPath() const = 0;

    protected:
        /**
         * @brief 受保护的默认构造函数
         *
         * 设计理念:
         * - 接口类通常不应被直接实例化
         * - 受保护构造函数确保只能通过派生类创建对象
         * - 遵循C++最佳实践
         *
         * 教学要点:
         * - 构造函数保护: 控制对象创建方式
         * - 接口设计: 明确使用模式
         * - 继承层次: 清晰的类关系
         */
        IFileReader() = default;

        /**
         * @brief 受保护的拷贝构造函数
         * @param other 要拷贝的对象
         *
         * 设计说明:
         * - 支持多态语义的正确拷贝
         * - 派生类可以决定是否支持拷贝
         * - 遵循C++ Rule of Three/Five
         */
        IFileReader(const IFileReader& other) = default;

        /**
         * @brief 受保护的赋值操作符
         * @param other 要赋值的对象
         * @return *this
         *
         * 设计说明:
         * - 支持多态语义的正确赋值
         * - 派生类可以决定是否支持赋值
         * - 遵循C++ Rule of Three/Five
         */
        IFileReader& operator=(const IFileReader& other) = default;
    };
} // namespace enigma::graphic
