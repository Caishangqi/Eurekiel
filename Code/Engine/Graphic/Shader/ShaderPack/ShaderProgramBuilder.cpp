/**
 * @file ShaderProgramBuilder.cpp
 * @brief 着色器程序构建器实现
 * @date 2025-10-03
 */

#include "ShaderProgramBuilder.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include <iostream>

#include "Engine/Core/EngineCommon.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // 主要构建方法
    // ========================================================================

    ShaderProgramBuilder::BuildResult ShaderProgramBuilder::BuildProgram(
        const ShaderSource& source,
        ShaderType          type
    )
    {
        UNUSED(type)
        BuildResult result;

        // 1. 验证 ShaderSource
        if (!source.IsValid())
        {
            result.success      = false;
            result.errorMessage = "ShaderSource is invalid: missing vertex or pixel shader source";
            return result;
        }

        // 2. 编译顶点着色器 (必需)
        result.vertexShader = CompileShaderStage(
            source.GetVertexSource(),
            ShaderStage::Vertex,
            source.GetName(),
            source.GetDirectives()
        );

        if (!result.vertexShader || !result.vertexShader->IsValid())
        {
            result.success      = false;
            result.errorMessage = "Failed to compile vertex shader: " +
                (result.vertexShader ? result.vertexShader->errorMessage : "nullptr");
            return result;
        }

        // 3. 编译像素着色器 (必需)
        result.pixelShader = CompileShaderStage(
            source.GetPixelSource(),
            ShaderStage::Pixel,
            source.GetName(),
            source.GetDirectives()
        );

        if (!result.pixelShader || !result.pixelShader->IsValid())
        {
            result.success      = false;
            result.errorMessage = "Failed to compile pixel shader: " +
                (result.pixelShader ? result.pixelShader->errorMessage : "nullptr");
            return result;
        }

        // 4. 编译几何着色器 (可选)
        if (source.HasGeometryShader())
        {
            result.geometryShader = CompileShaderStage(
                source.GetGeometrySource().value(),
                ShaderStage::Geometry,
                source.GetName(),
                source.GetDirectives()
            );

            // 几何着色器编译失败是可以容忍的 (警告而非错误)
            if (!result.geometryShader || !result.geometryShader->IsValid())
            {
                std::cerr << "[ShaderProgramBuilder] Warning: Geometry shader compilation failed for "
                    << source.GetName() << std::endl;
                result.geometryShader.reset(); // 清空失败的着色器
            }
        }

        // 5. 编译计算着色器 (可选)
        if (source.HasComputeShader())
        {
            result.computeShader = CompileShaderStage(
                source.GetComputeSource().value(),
                ShaderStage::Compute,
                source.GetName(),
                source.GetDirectives()
            );

            // 计算着色器编译失败是可以容忍的
            if (!result.computeShader || !result.computeShader->IsValid())
            {
                std::cerr << "[ShaderProgramBuilder] Warning: Compute shader compilation failed for "
                    << source.GetName() << std::endl;
                result.computeShader.reset();
            }
        }

        // 6. 存储 ProgramDirectives (从 ShaderSource 获取)
        result.directives = source.GetDirectives();

        // 7. 成功
        result.success = true;
        return result;
    }

    // ========================================================================
    // 编译单个着色器阶段
    // ========================================================================

    std::unique_ptr<CompiledShader> ShaderProgramBuilder::CompileShaderStage(
        const std::string&               source,
        ShaderStage                      stage,
        const std::string&               name,
        const shader::ProgramDirectives& directives
    )
    {
        auto compiledShader = std::make_unique<CompiledShader>();

        // 1. 设置着色器元数据
        compiledShader->stage      = stage;
        compiledShader->name       = name;
        compiledShader->entryPoint = GetEntryPoint(stage);
        compiledShader->profile    = GetShaderProfile(stage);
        compiledShader->sourceCode = source;

        // 2. 配置 DXC 编译选项
        DXCCompiler::CompileOptions options = ConfigureCompileOptions(directives, stage);
        options.entryPoint                  = compiledShader->entryPoint;
        options.target                      = compiledShader->profile;

        // 3. 调用 DXC 编译
        DXCCompiler compiler;
        if (!compiler.Initialize())
        {
            compiledShader->success      = false;
            compiledShader->errorMessage = "Failed to initialize DXC compiler";
            return compiledShader;
        }

        DXCCompiler::CompileResult result = compiler.CompileShader(source, options);

        // 4. 填充编译结果
        compiledShader->success        = result.success;
        compiledShader->errorMessage   = result.errorMessage;
        compiledShader->warningMessage = result.warningMessage;
        compiledShader->bytecode       = std::move(result.bytecode);

        // 5. 打印警告 (如果有)
        if (compiledShader->HasWarnings())
        {
            std::cerr << "[ShaderProgramBuilder] Warning in " << name << " ("
                << compiledShader->entryPoint << "):\n"
                << compiledShader->warningMessage << std::endl;
        }

        return compiledShader;
    }

    // ========================================================================
    // 辅助方法
    // ========================================================================

    std::string ShaderProgramBuilder::GetShaderProfile(ShaderStage stage)
    {
        // 使用 Shader Model 6.6 (支持 Bindless)
        switch (stage)
        {
        case ShaderStage::Vertex: return "vs_6_6";
        case ShaderStage::Pixel: return "ps_6_6";
        case ShaderStage::Geometry: return "gs_6_6";
        case ShaderStage::Compute: return "cs_6_6";
        case ShaderStage::Hull: return "hs_6_6";
        case ShaderStage::Domain: return "ds_6_6";
        default:
            ERROR_AND_DIE("Unknown ShaderStage")
        }
    }

    std::string ShaderProgramBuilder::GetEntryPoint(ShaderStage stage)
    {
        // 标准化入口点命名
        switch (stage)
        {
        case ShaderStage::Vertex: return "VSMain";
        case ShaderStage::Pixel: return "PSMain";
        case ShaderStage::Geometry: return "GSMain";
        case ShaderStage::Compute: return "CSMain";
        case ShaderStage::Hull: return "HSMain";
        case ShaderStage::Domain: return "DSMain";
        default:
            ERROR_AND_DIE("Unknown ShaderStage")
        }
    }

    DXCCompiler::CompileOptions ShaderProgramBuilder::ConfigureCompileOptions(
        const shader::ProgramDirectives& directives,
        ShaderStage                      stage
    )
    {
        UNUSED(stage)
        DXCCompiler::CompileOptions options;

        // 1. 基础配置
        options.enableOptimization = true; // 默认启用优化
        options.enableDebugInfo    = false; // Release 模式不生成调试信息
        options.enable16BitTypes   = true; // 支持 16-bit 类型 (性能优化)

        // 2. 宏定义 (从 ProgramDirectives 转换)
        // 例如: const int shadowMapResolution = 4096; → -D SHADOW_MAP_RES=4096
        // (注意: ProgramDirectives 当前没有存储 const 定义,此处预留扩展)

        // 3. Include 路径
        // 默认包含 ShaderPack 目录
        options.includePaths.push_back(L"F:/p4/Personal/SD/Engine/Code/Engine/Graphic/Shader/ShaderPack/");
        options.includePaths.push_back(L"F:/p4/Personal/SD/Engine/Code/Engine/Graphic/Shader/Common/");

        // 4. 根据 ProgramDirectives 添加宏定义
        auto blendModeOpt = directives.GetBlendMode();
        if (blendModeOpt.has_value())
        {
            options.defines.push_back("BLEND_MODE=" + blendModeOpt.value());
        }

        auto depthTestOpt = directives.GetDepthTest();
        if (depthTestOpt.has_value())
        {
            options.defines.push_back("DEPTH_TEST=" + depthTestOpt.value());
        }

        auto cullFaceOpt = directives.GetCullFace();
        if (cullFaceOpt.has_value())
        {
            options.defines.push_back("CULL_FACE=" + cullFaceOpt.value());
        }

        auto depthWriteOpt = directives.GetDepthWrite();
        if (depthWriteOpt.has_value())
        {
            if (depthWriteOpt.value())
            {
                options.defines.push_back("DEPTH_WRITE=1");
            }
            else
            {
                options.defines.push_back("DEPTH_WRITE=0");
            }
        }

        return options;
    }
} // namespace enigma::graphic
