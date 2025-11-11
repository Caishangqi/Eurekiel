/**
 * @file ShaderCompilationHelper.cpp
 * @brief Shader编译辅助工具类实现
 * @date 2025-11-04
 * @author Caizii
 */

#include "ShaderCompilationHelper.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include <fstream>
#include <sstream>

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    // ========== 文件IO操作 ==========

    std::optional<std::string> ShaderCompilationHelper::ReadShaderSourceFromFile(
        const std::filesystem::path& filePath
    )
    {
        // 检查文件是否存在
        if (!std::filesystem::exists(filePath))
        {
            LogError(core::LogRenderer, "Failed to read shader file (file not found): %s", filePath.string().c_str());
            ERROR_AND_DIE(Stringf("Failed to read shader file (file not found): %s",
                filePath.string().c_str()))
        }

        // 打开文件
        std::ifstream file(filePath, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            LogError(core::LogRenderer, "Failed to open shader file: %s", filePath.string().c_str());
            ERROR_AND_DIE(Stringf("Failed to open shader file: %s",
                filePath.string().c_str()))
        }

        // 读取文件内容
        std::stringstream buffer;
        buffer << file.rdbuf();

        // 检查读取是否成功
        if (file.fail() && !file.eof())
        {
            LogError(core::LogRenderer, "Failed to read shader file content: %s", filePath.string().c_str());
            ERROR_AND_DIE(Stringf("Failed to read shader file content: %s",
                filePath.string().c_str()))
        }

        return buffer.str();
    }

    // ========== 路径处理 ==========

    std::string ShaderCompilationHelper::ExtractProgramNameFromPath(
        const std::filesystem::path& filePath
    )
    {
        // 提取文件名 (去除目录路径)
        std::string filename = filePath.filename().string();

        // 查找第一个 '.' 的位置
        size_t dotPos = filename.find('.');

        // 如果找到 '.'，返回之前的部分；否则返回完整文件名
        if (dotPos != std::string::npos)
        {
            return filename.substr(0, dotPos);
        }

        return filename;
    }

    std::filesystem::path ShaderCompilationHelper::GetEngineShaderCorePath()
    {
        // 返回引擎着色器核心目录路径
        // 注意：这是相对于项目根目录的路径
        return std::filesystem::path("Run") / ".enigma" / "assets" / "engine" / "shaders" / "core";
    }

    std::vector<std::wstring> ShaderCompilationHelper::BuildIncludePaths(
        const std::vector<std::filesystem::path>& userIncludePaths
    )
    {
        std::vector<std::wstring> result;
        result.reserve(userIncludePaths.size());

        for (const auto& path : userIncludePaths)
        {
            // 转换为绝对路径
            std::filesystem::path absolutePath = std::filesystem::absolute(path);

            // 转换为宽字符串 (DXC要求)
            result.push_back(absolutePath.wstring());
        }

        return result;
    }

    // ========== 配置生成 ==========

    shader::ProgramDirectives ShaderCompilationHelper::CreateDefaultDirectives()
    {
        // 使用默认构造函数创建ProgramDirectives
        // 默认配置:
        // - drawBuffers: {0} (只输出到 RT0)
        // - 其他指令: std::nullopt (未指定)
        return shader::ProgramDirectives();
    }

    DXCCompiler::CompileOptions ShaderCompilationHelper::ConvertToCompilerOptions(
        const ShaderCompileOptions& opts,
        ShaderStage                 stage,
        const std::string&          configuredEntryPoint
    )
    {
        DXCCompiler::CompileOptions dxcOpts;

        // 使用配置的入口点
        dxcOpts.entryPoint = GetEntryPoint(stage, configuredEntryPoint);
        dxcOpts.target     = GetShaderProfile(stage);

        // 复制编译选项
        dxcOpts.enableDebugInfo    = opts.enableDebugInfo;
        dxcOpts.enableOptimization = opts.enableOptimization;
        dxcOpts.enable16BitTypes   = opts.enable16BitTypes;
        dxcOpts.enableBindless     = opts.enableBindless;

        // 复制宏定义
        dxcOpts.defines = opts.defines;

        // 转换Include路径为宽字符串
        dxcOpts.includePaths = BuildIncludePaths(opts.includePaths);

        return dxcOpts;
    }

    // ========== ShaderCompileOptions 静态工厂方法 ==========

    ShaderCompileOptions ShaderCompileOptions::Default()
    {
        ShaderCompileOptions opts;
        // 使用默认值（结构体初始化器已设置）
        return opts;
    }

    ShaderCompileOptions ShaderCompileOptions::Debug()
    {
        ShaderCompileOptions opts;
        opts.enableDebugInfo    = true;
        opts.enableOptimization = false;
        return opts;
    }

    ShaderCompileOptions ShaderCompileOptions::WithCommonInclude()
    {
        ShaderCompileOptions opts;
        opts.includePaths.push_back(ShaderCompilationHelper::GetEngineShaderCorePath());
        opts.entryPoint = "main"; //  Iris兼容：使用 "main" 作为入口点
        return opts;
    }

    // ========== 私有辅助方法 ==========

    std::string ShaderCompilationHelper::GetShaderProfile(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:
            return "vs_6_6";
        case ShaderStage::Pixel:
            return "ps_6_6";
        case ShaderStage::Compute:
            return "cs_6_6";
        case ShaderStage::Geometry:
            return "gs_6_6";
        case ShaderStage::Hull:
            return "hs_6_6";
        case ShaderStage::Domain:
            return "ds_6_6";
        default:
            return "vs_6_6"; // 默认返回顶点着色器
        }
    }

    std::string ShaderCompilationHelper::GetEntryPoint(
        ShaderStage        stage,
        const std::string& configuredEntryPoint
    )
    {
        // 如果配置了入口点，直接使用（所有阶段统一）
        if (!configuredEntryPoint.empty())
        {
            return configuredEntryPoint;
        }

        // Fallback: 使用默认入口点（向后兼容）
        switch (stage)
        {
        case ShaderStage::Vertex:
            return "VSMain";
        case ShaderStage::Pixel:
            return "PSMain";
        case ShaderStage::Compute:
            return "CSMain";
        case ShaderStage::Geometry:
            return "GSMain";
        case ShaderStage::Hull:
            return "HSMain";
        case ShaderStage::Domain:
            return "DSMain";
        default:
            return "VSMain"; // 默认返回顶点着色器入口点
        }
    }
} // namespace enigma::graphic
