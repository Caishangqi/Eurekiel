/**
 * @file FileSystemReader.hpp
 * @brief 文件系统读取器 - IFileReader接口的本地文件系统实现
 * @date 2025-11-04
 * @author Caizii
 *
 * 架构说明:
 * FileSystemReader 是 IFileReader 接口的本地文件系统实现，提供安全的文件读取功能。
 * 这是Shader Include系统的核心文件访问组件，使用当前工作目录作为根目录和安全检查功能。
 *
 * 设计原则:
 * - 安全第一: 所有文件访问都限制在根目录内
 * - 工作目录根: 使用当前工作目录作为根目录
 * - 异常安全: 使用RAII和现代C++最佳实践
 * - 扩展性: 支持手动指定根目录的构造函数重载
 *
 * 职责边界:
 * - + 本地文件读取 (ReadFile)
 * - + 文件存在性检查 (FileExists)
 * - + 根路径管理 (GetRootPath)
 * - + 路径安全验证 (IsPathWithinRoot)
 * - ❌ 不包含缓存逻辑 (可由装饰器类添加)
 * - ❌ 不包含网络文件访问 (可由其他实现类提供)
 *
 * 安全特性:
 * - 路径遍历攻击防护
 * - 工作目录边界检查
 * - 符号链接安全处理
 * - 权限验证
 *
 * 使用示例:
 * @code
 * // 使用当前工作目录作为根目录
 * auto reader1 = std::make_unique<FileSystemReader>();
 * 
 * // 手动指定根目录
 * auto reader2 = std::make_unique<FileSystemReader>(explicitRootPath);
 * 
 * // 读取文件
 * auto shaderPath = ShaderPath::FromAbsolutePath("/shaders/common.hlsl");
 * auto content = reader2->ReadFile(shaderPath);
 * if (content) {
 *     std::cout << "文件内容: " << *content << std::endl;
 * }
 * 
 * // 检查文件存在性
 * if (reader2->FileExists(shaderPath)) {
 *     std::cout << "文件存在" << std::endl;
 * }
 * @endcode
 *
 * 教学要点:
 * - RAII资源管理
 * - 路径安全编程
 * - 现代C++文件系统操作
 * - 异常安全设计
 * - 策略模式实现
 */

#pragma once

#include "IFileReader.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

// 前向声明避免循环依赖
namespace enigma::graphic
{
    class ShaderPath;
}

namespace enigma::graphic
{
    /**
     * @class FileSystemReader
     * @brief 本地文件系统读取器实现
     *
     * 核心特性:
     * - 使用当前工作目录作为根目录
     * - 严格的路径安全检查
     * - 支持手动指定根目录
     * - 高效的文件IO操作
     *
     * 安全机制:
     * - 所有文件访问限制在根目录内
     * - 防护路径遍历攻击 (../etc/passwd)
     * - 符号链接安全验证
     * - 详细的错误处理和日志
     *
     * 性能优化:
     * - 使用std::filesystem进行高效路径操作
     * - 文件流缓冲优化
     * - 最小化系统调用
     * - 智能错误处理
     */
    class FileSystemReader : public IFileReader
    {
    public:
        /**
         * @brief 默认构造函数 - 使用当前工作目录作为根目录
         *
         * 工作流程:
         * 1. 获取当前工作目录 std::filesystem::current_path()
         * 2. 将当前目录设置为根目录
         * 3. 验证根目录的有效性
         *
         * 示例:
         * @code
         * // 使用当前工作目录作为根目录
         * auto reader = std::make_unique<FileSystemReader>();
         * @endcode
         */
        FileSystemReader();

        /**
         * @brief 构造函数重载 - 手动指定根目录
         * @param explicitRoot 明确指定的根目录路径
         *
         * 使用场景:
         * - 已知明确的项目根目录
         * - 测试环境中指定特殊根目录
         * - 需要精确控制根目录位置
         *
         * 示例:
         * @code
         * FileSystemReader reader("F:/project/");
         * @endcode
         *
         * @throws std::runtime_error 如果根目录无效
         */
        explicit FileSystemReader(const std::filesystem::path& explicitRoot);

        /**
         * @brief 析构函数
         *
         * 设计说明:
         * - 使用默认析构函数
         * - RAII自动资源管理
         * - 现代C++推荐做法
         */
        ~FileSystemReader() override = default;

        /**
         * @brief 读取文件内容
         * @param path Shader Pack 路径 (Unix风格绝对路径)
         * @return std::optional<std::string> 成功返回文件内容，失败返回nullopt
         *
         * 安全检查:
         * - 验证路径是否在根目录内
         * - 检查文件是否存在且可读
         * - 防止路径遍历攻击
         *
         * 读取策略:
         * - 使用std::ifstream进行文本读取
         * - 自动处理换行符转换
         * - 优化缓冲区大小
         * - 详细错误日志
         *
         * @throws std::invalid_argument 如果路径在根目录外
         */
        std::optional<std::string> ReadFile(const ShaderPath& path) override;

        /**
         * @brief 检查文件是否存在
         * @param path Shader Pack 路径 (Unix风格绝对路径)
         * @return bool 文件存在返回true，否则返回false
         *
         * 安全特性:
         * - 路径安全验证
         * - 符号链接安全处理
         * - 权限检查
         *
         * 性能优化:
         * - 使用std::filesystem::exists快速检查
         * - 避免不必要的文件打开操作
         * - 缓存友好设计
         *
         * @throws std::invalid_argument 如果路径在根目录外
         */
        bool FileExists(const ShaderPath& path) const override;

        /**
         * @brief 获取根路径
         * @return std::filesystem::path 根路径
         *
         * 返回值说明:
         * - 返回标准化的绝对路径
         * - 路径以分隔符结尾 (便于路径拼接)
         * - 平台相关路径格式
         */
        std::filesystem::path GetRootPath() const override;

    private:
        /**
         * @brief 根目录路径
         * 
         * 不变性:
         * - 构造后不再修改
         * - 始终为绝对路径
         * - 默认为当前工作目录
         */
        std::filesystem::path m_rootPath;


        /**
         * @brief 检查路径是否在根目录内
         * @param path 要检查的路径
         * @param root 根目录路径
         * @return bool 在根目录内返回true，否则返回false
         *
         * 安全检查:
         * - 规范化路径 (使用weakly_canonical处理..和.)
         * - 检查路径前缀
         * - 符号链接安全验证
         * - 防止路径遍历攻击
         *
         * 检查策略:
         * 1. 将两个路径都转换为绝对和规范形式 (使用weakly_canonical)
         * 2. 检查path是否以root开头
         * 3. 确保不越过root边界
         * 4. 处理符号链接的安全风险
         *
         * @throws std::invalid_argument 如果路径不在根目录内
         */
        static bool IsPathWithinRoot(const std::filesystem::path& path,
                                     const std::filesystem::path& root);

        /**
         * @brief 安全读取文件内容
         * @param filePath 要读取的文件路径 (已验证在根目录内)
         * @return std::optional<std::string> 文件内容或nullopt
         *
         * 读取细节:
         * - 以文本模式打开文件
         * - 自动处理换行符转换
         * - 使用stringstream高效读取
         * - 处理大文件的内存优化
         *
         * 错误处理:
         * - 文件不存在 → nullopt
         * - 权限不足 → nullopt  
         * - 编码错误 → nullopt
         * - IO错误 → nullopt
         *
         * 性能优化:
         * - 预分配字符串缓冲区
         * - 使用移动语义减少拷贝
         * - 异常处理最小化开销
         */
        static std::optional<std::string> ReadFileContent(const std::filesystem::path& filePath);

        /**
         * @brief 规范化路径并处理符号链接
         * @param path 原始路径
         * @return 规范化后的绝对路径
         *
         * 处理步骤:
         * 1. 转换为绝对路径
         * 2. 解析所有符号链接
         * 3. 规范化路径分隔符
         * 4. 移除冗余的.和..
         *
         * 安全考虑:
         * - 防止符号链接攻击
         * - 处理循环链接
         * - 权限验证
         */
        static std::filesystem::path CanonicalizePath(const std::filesystem::path& path);


        /**
         * 教学要点总结：
         * 1. **RAII资源管理**：构造函数获取资源，析构函数自动释放
         * 2. **异常安全设计**：使用RAII和智能指针，确保异常安全
         * 3. **路径安全编程**：防止路径遍历攻击，严格边界检查
         * 4. **现代C++实践**：使用std::filesystem，范围for循环，auto关键字
         * 5. **SOLID原则**：单一职责，开闭原则，依赖倒置
         * 6. **防御性编程**：输入验证，边界检查，详细错误处理
         */
    };
} // namespace enigma::graphic
