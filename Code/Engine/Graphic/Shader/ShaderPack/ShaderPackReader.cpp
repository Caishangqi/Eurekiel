/**
 * @file ShaderPackReader.cpp
 * @brief ShaderPack文件读取器实现
 * @date 2025-11-04
 * @author Caizii
 */

#include "ShaderPackReader.hpp"
#include <fstream>
#include <sstream>

namespace enigma::graphic
{
    // ========================================================================
    // 构造函数
    // ========================================================================

    ShaderPackReader::ShaderPackReader(const std::filesystem::path& packRoot)
        : m_packRoot(packRoot)
    {
        /**
         * 教学要点:
         * - 使用初始化列表直接初始化成员变量
         * - 不验证根路径有效性（延迟到首次使用）
         * - 依赖注入模式：根路径通过构造函数注入
         */
    }

    // ========================================================================
    // IFileReader接口实现
    // ========================================================================

    std::optional<std::string> ShaderPackReader::ReadFile(const ShaderPath& path)
    {
        /**
         * 实现流程:
         * 1. 将ShaderPath虚拟路径转换为文件系统路径
         * 2. 检查文件是否存在
         * 3. 打开文件并读取内容
         * 4. 返回文件内容或nullopt
         *
         * 教学要点:
         * - 使用ShaderPath::Resolved()进行路径转换
         * - 使用std::filesystem检查文件存在性
         * - 使用std::ifstream读取文件内容
         * - 使用std::optional表示可能的失败
         */

        // Step 1: 转换为文件系统路径
        auto fullPath = path.Resolved(m_packRoot);

        // Step 2: 检查文件是否存在
        if (!std::filesystem::exists(fullPath))
        {
            // 文件不存在，返回nullopt
            return std::nullopt;
        }

        // Step 3: 打开并读取文件
        std::ifstream file(fullPath);
        if (!file.is_open())
        {
            // 文件无法打开（权限问题等），返回nullopt
            return std::nullopt;
        }

        // 读取文件内容到stringstream
        std::stringstream buffer;
        buffer << file.rdbuf();

        // Step 4: 返回文件内容
        return buffer.str();
    }

    bool ShaderPackReader::FileExists(const ShaderPath& path) const
    {
        /**
         * 实现流程:
         * 1. 将ShaderPath虚拟路径转换为文件系统路径
         * 2. 使用std::filesystem::exists()检查文件存在性
         *
         * 教学要点:
         * - 常量成员函数：不修改对象状态
         * - 快速检查：避免实际读取文件内容
         * - 路径转换：ShaderPath→文件系统路径
         */

        // 转换为文件系统路径
        auto fullPath = path.Resolved(m_packRoot);

        // 检查文件是否存在
        return std::filesystem::exists(fullPath);
    }

    std::filesystem::path ShaderPackReader::GetRootPath() const
    {
        /**
         * 实现说明:
         * - 直接返回存储的根路径
         * - 用于调试和路径转换
         *
         * 教学要点:
         * - 常量成员函数：不修改对象状态
         * - 封装原则：通过getter访问私有成员
         * - 简单直接的实现
         */

        return m_packRoot;
    }
} // namespace enigma::graphic
