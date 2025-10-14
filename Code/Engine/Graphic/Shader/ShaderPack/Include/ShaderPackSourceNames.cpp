#include "ShaderPackSourceNames.hpp"
#include <algorithm>

namespace enigma::graphic
{
    // ========================================================================
    // 文件名生成实现
    // ========================================================================

    std::vector<std::string> ShaderPackSourceNames::GenerateAllPossibleNames(
        const std::string& baseName,
        bool               includeComputeVariants
    )
    {
        /**
         * 教学要点: 动态生成 HLSL 文件名列表
         *
         * 业务逻辑:
         * 1. 添加 5 个标准扩展名 (.vs.hlsl, .ps.hlsl, .gs.hlsl, .hs.hlsl, .ds.hlsl)
         * 2. 添加基础 Compute 扩展名 (.cs.hlsl)
         * 3. 可选：添加 26 个 Compute 变体 (_a.cs.hlsl, _b.cs.hlsl, ..., _z.cs.hlsl)
         */

        std::vector<std::string> result;
        result.reserve(includeComputeVariants ? 32 : 6); // 预分配空间

        // Step 1: 添加标准 HLSL 扩展名
        for (const auto& ext : STANDARD_EXTENSIONS)
        {
            result.push_back(baseName + std::string(ext));
        }

        // Step 2: 添加基础 Compute 扩展名
        result.push_back(baseName + std::string(COMPUTE_EXTENSION));

        // Step 3: 可选 - 添加 Compute 变体
        if (includeComputeVariants)
        {
            for (int i = 1; i < COMPUTE_VARIANT_COUNT; ++i)
            {
                char letter = static_cast<char>('a' + i - 1); // a-z
                result.push_back(baseName + "_" + letter + std::string(COMPUTE_EXTENSION));
            }
        }

        return result;
    }

    std::vector<std::string> ShaderPackSourceNames::GetAllShaderExtensions()
    {
        /**
         * 教学要点: 一次性生成所有可能的 HLSL 扩展名
         *
         * 业务逻辑:
         * - 5 个标准扩展名 (.vs.hlsl, .ps.hlsl, .gs.hlsl, .hs.hlsl, .ds.hlsl)
         * - 1 个 Compute 扩展名 (.cs.hlsl) - 变体共用相同扩展名
         * - 1 个库文件扩展名 (.hlsl)
         * - 总计 7 个扩展名
         */

        std::vector<std::string> result;
        result.reserve(7);

        // 标准扩展名
        for (const auto& ext : STANDARD_EXTENSIONS)
        {
            result.push_back(std::string(ext));
        }

        // Compute 扩展名（基础，变体共用相同扩展名）
        result.push_back(std::string(COMPUTE_EXTENSION)); // .cs.hlsl

        // 库文件扩展名
        result.push_back(std::string(LIBRARY_EXTENSION)); // .hlsl

        return result;
    }

    // ========================================================================
    // 文件类型检查实现
    // ========================================================================

    bool ShaderPackSourceNames::IsShaderSourceFile(const std::string& fileName)
    {
        /**
         * 教学要点: 快速检查文件是否是着色器源文件
         *
         * 业务逻辑:
         * 1. 提取文件扩展名
         * 2. 检查是否在标准扩展名列表中
         * 3. 检查是否是 Compute 扩展名
         * 4. 排除库文件 (.hlsl)
         */

        std::string ext = GetFileExtension(fileName);
        if (ext.empty())
        {
            return false; // 没有扩展名
        }

        // 排除库文件
        if (ext == LIBRARY_EXTENSION)
        {
            return false;
        }

        // 检查标准扩展名
        for (const auto& standardExt : STANDARD_EXTENSIONS)
        {
            if (ext == standardExt)
            {
                return true;
            }
        }

        // 检查 Compute 扩展名
        return IsComputeShaderExtension(ext);
    }

    bool ShaderPackSourceNames::IsLibraryFile(const std::string& fileName)
    {
        /**
         * 教学要点: 检查是否是库文件（.hlsl 纯头文件）
         *
         * 业务逻辑:
         * - "Common.hlsl" → true
         * - "Lighting.hlsl" → true
         * - "gbuffers_terrain.vs.hlsl" → false（着色器文件）
         * - "final.cs.hlsl" → false（着色器文件）
         */

        std::string ext = GetFileExtension(fileName);
        return ext == LIBRARY_EXTENSION;
    }

    bool ShaderPackSourceNames::IsComputeShaderExtension(const std::string& extension)
    {
        /**
         * 教学要点: 检查是否是 Compute 着色器扩展名
         *
         * 业务逻辑:
         * - 扩展名: ".cs.hlsl"
         * - 变体文件（final_a.cs.hlsl）也使用相同扩展名
         *
         * 实现策略:
         * - 只需检查是否等于 ".cs.hlsl"
         */

        return extension == COMPUTE_EXTENSION;
    }

    // ========================================================================
    // Compute 着色器变体生成
    // ========================================================================

    std::vector<std::string> ShaderPackSourceNames::GenerateComputeVariantNames(
        const std::string& baseName
    )
    {
        /**
         * 教学要点: 生成 Compute 着色器的所有变体文件名
         *
         * 业务逻辑:
         * - final.cs.hlsl（基础）
         * - final_a.cs.hlsl, final_b.cs.hlsl, ..., final_z.cs.hlsl（26个变体）
         * - 总计 27 个文件名
         *
         * 对应 Iris 的 addComputeStarts() 方法
         */

        std::vector<std::string> result;
        result.reserve(COMPUTE_VARIANT_COUNT);

        // 基础变体
        result.push_back(baseName + std::string(COMPUTE_EXTENSION));

        // 字母变体
        for (int i = 1; i < COMPUTE_VARIANT_COUNT; ++i)
        {
            char letter = static_cast<char>('a' + i - 1);
            result.push_back(baseName + "_" + letter + std::string(COMPUTE_EXTENSION));
        }

        return result;
    }

    // ========================================================================
    // 辅助函数实现
    // ========================================================================

    std::string ShaderPackSourceNames::GetFileExtension(const std::string& fileName)
    {
        /**
         * 教学要点: HLSL 文件扩展名提取（支持双段扩展名）
         *
         * 业务逻辑:
         * - "gbuffers_terrain.vs.hlsl" → ".vs.hlsl"（双段）
         * - "final_a.cs.hlsl" → ".cs.hlsl"（双段）
         * - "Common.hlsl" → ".hlsl"（单段）
         * - "file" → ""
         *
         * 实现细节:
         * - 优先检查是否以已知的双段扩展名结尾
         * - 如果不是，返回最后一个 '.' 之后的部分
         */

        // Step 1: 检查标准双段扩展名
        for (const auto& ext : STANDARD_EXTENSIONS)
        {
            if (fileName.size() >= ext.size() &&
                fileName.compare(fileName.size() - ext.size(), ext.size(), ext) == 0)
            {
                return std::string(ext);
            }
        }

        // Step 2: 检查 Compute 双段扩展名
        if (fileName.size() >= COMPUTE_EXTENSION.size() &&
            fileName.compare(fileName.size() - COMPUTE_EXTENSION.size(),
                             COMPUTE_EXTENSION.size(),
                             COMPUTE_EXTENSION) == 0)
        {
            return std::string(COMPUTE_EXTENSION);
        }

        // Step 3: 回退到单段扩展名
        size_t dotPos = fileName.find_last_of('.');
        if (dotPos == std::string::npos)
        {
            return ""; // 没有扩展名
        }

        return fileName.substr(dotPos); // 包含 '.'
    }
} // namespace enigma::graphic
