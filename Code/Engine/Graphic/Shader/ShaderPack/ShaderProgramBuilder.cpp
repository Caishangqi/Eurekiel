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
        // ✅ 修复：使用 "main" 作为默认入口点（Iris兼容）
        // 注意：此函数现在主要用于向后兼容
        // 推荐使用配置系统指定入口点
        switch (stage)
        {
        case ShaderStage::Vertex: return "main"; // ✅ Iris兼容
        case ShaderStage::Pixel: return "main"; // ✅ Iris兼容
        case ShaderStage::Geometry: return "main"; // ✅ Iris兼容
        case ShaderStage::Compute: return "main"; // ✅ Iris兼容
        case ShaderStage::Hull: return "main"; // ✅ Iris兼容
        case ShaderStage::Domain: return "main"; // ✅ Iris兼容
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

    // ========================================================================
    // Shrimp Task 3: 编译选项合并
    // ========================================================================

    DXCCompiler::CompileOptions ShaderProgramBuilder::MergeCompileOptions(
        const DXCCompiler::CompileOptions& defaultOpts,
        const ShaderCompileOptions&        customOpts
    )
    {
        DXCCompiler::CompileOptions merged = defaultOpts;

        // 1. 合并 Include 路径（用户路径优先，追加到前面）
        // 策略：用户路径 + 默认路径
        std::vector<std::wstring> userIncludePaths = ShaderCompilationHelper::BuildIncludePaths(customOpts.includePaths);

        // 将用户路径插入到默认路径之前
        merged.includePaths.insert(
            merged.includePaths.begin(),
            userIncludePaths.begin(),
            userIncludePaths.end()
        );

        // 2. 合并宏定义（用户宏优先，追加到前面）
        // 策略：用户宏 + 默认宏
        merged.defines.insert(
            merged.defines.begin(),
            customOpts.defines.begin(),
            customOpts.defines.end()
        );

        // 3. 覆盖调试选项（用户选项优先）
        // 如果用户启用了调试信息，则覆盖默认设置
        if (customOpts.enableDebugInfo)
        {
            merged.enableDebugInfo = true;
        }

        // 如果用户禁用了优化，则覆盖默认设置
        if (!customOpts.enableOptimization)
        {
            merged.enableOptimization = false;
        }

        // 4. 覆盖其他选项（用户选项优先）
        // 注意：这些选项通常保持默认值，但如果用户明确设置，则覆盖
        merged.enable16BitTypes = customOpts.enable16BitTypes;
        merged.enableBindless   = customOpts.enableBindless;

        // 5. 合并入口点（用户入口点优先）
        // 教学要点:
        // - 配置优先级: 用户配置 > 默认配置
        // - 空值检查: 只有非空时才覆盖
        // - Iris兼容: 支持用户配置 "main" 作为入口点
        if (!customOpts.entryPoint.empty())
        {
            merged.entryPoint = customOpts.entryPoint;
        }

        return merged;
    }

    // ========================================================================
    // Shrimp Task 3: 支持自定义编译选项的重载方法
    // ========================================================================

    ShaderProgramBuilder::BuildResult ShaderProgramBuilder::BuildProgram(
        const ShaderSource&         source,
        ShaderType                  type,
        const ShaderCompileOptions& customOptions
    )
    {
        UNUSED(type)
        BuildResult result;

        // 调试日志：验证入口点配置传递
        std::cout << "[ShaderProgramBuilder] Building shader program: " << source.GetName() << std::endl;
        std::cout << "[ShaderProgramBuilder] Custom entry point: "
            << (customOptions.entryPoint.empty() ? "<empty>" : customOptions.entryPoint) << std::endl;

        // 1. 验证 ShaderSource
        if (!source.IsValid())
        {
            result.success      = false;
            result.errorMessage = "ShaderSource is invalid: missing vertex or pixel shader source";
            return result;
        }

        // 2. 获取默认编译选项（从 ProgramDirectives 生成）
        DXCCompiler::CompileOptions defaultOpts = ConfigureCompileOptions(
            source.GetDirectives(),
            ShaderStage::Vertex // 使用 Vertex 作为默认阶段
        );

        // 3. 合并用户自定义选项
        DXCCompiler::CompileOptions mergedOpts = MergeCompileOptions(defaultOpts, customOptions);

        // 调试日志：验证合并后的入口点
        std::cout << "[ShaderProgramBuilder] Merged entry point: " << mergedOpts.entryPoint << std::endl;

        // 4. 编译顶点着色器（使用合并后的选项）
        auto compiledVS   = std::make_unique<CompiledShader>();
        compiledVS->stage = ShaderStage::Vertex;
        compiledVS->name  = source.GetName();
        // ✅ 修复：优先使用配置的入口点，如果为空则使用默认值 "main"（Iris兼容）
        compiledVS->entryPoint = !mergedOpts.entryPoint.empty()
                                     ? mergedOpts.entryPoint
                                     : "main";
        compiledVS->profile    = GetShaderProfile(ShaderStage::Vertex);
        compiledVS->sourceCode = source.GetVertexSource();

        // 日志记录：验证入口点配置
        std::cout << "[ShaderProgramBuilder] VS entry point: " << compiledVS->entryPoint << std::endl;

        mergedOpts.entryPoint = compiledVS->entryPoint;
        mergedOpts.target     = compiledVS->profile;

        DXCCompiler compiler;
        if (!compiler.Initialize())
        {
            result.success      = false;
            result.errorMessage = "Failed to initialize DXC compiler";
            return result;
        }

        DXCCompiler::CompileResult vsResult = compiler.CompileShader(source.GetVertexSource(), mergedOpts);
        compiledVS->success                 = vsResult.success;
        compiledVS->errorMessage            = vsResult.errorMessage;
        compiledVS->warningMessage          = vsResult.warningMessage;
        compiledVS->bytecode                = std::move(vsResult.bytecode);

        if (!compiledVS->IsValid())
        {
            result.success      = false;
            result.errorMessage = "Failed to compile vertex shader: " + compiledVS->errorMessage;
            return result;
        }

        result.vertexShader = std::move(compiledVS);

        // 5. 编译像素着色器（使用合并后的选项）
        auto compiledPS   = std::make_unique<CompiledShader>();
        compiledPS->stage = ShaderStage::Pixel;
        compiledPS->name  = source.GetName();
        // ✅ 修复：优先使用配置的入口点，如果为空则使用默认值 "main"（Iris兼容）
        compiledPS->entryPoint = !mergedOpts.entryPoint.empty()
                                     ? mergedOpts.entryPoint
                                     : "main";
        compiledPS->profile    = GetShaderProfile(ShaderStage::Pixel);
        compiledPS->sourceCode = source.GetPixelSource();

        // 日志记录：验证入口点配置
        std::cout << "[ShaderProgramBuilder] PS entry point: " << compiledPS->entryPoint << std::endl;

        mergedOpts.entryPoint = compiledPS->entryPoint;
        mergedOpts.target     = compiledPS->profile;

        DXCCompiler::CompileResult psResult = compiler.CompileShader(source.GetPixelSource(), mergedOpts);
        compiledPS->success                 = psResult.success;
        compiledPS->errorMessage            = psResult.errorMessage;
        compiledPS->warningMessage          = psResult.warningMessage;
        compiledPS->bytecode                = std::move(psResult.bytecode);

        if (!compiledPS->IsValid())
        {
            result.success      = false;
            result.errorMessage = "Failed to compile pixel shader: " + compiledPS->errorMessage;
            return result;
        }

        result.pixelShader = std::move(compiledPS);

        // 6. 编译几何着色器（可选，使用合并后的选项）
        if (source.HasGeometryShader())
        {
            auto compiledGS        = std::make_unique<CompiledShader>();
            compiledGS->stage      = ShaderStage::Geometry;
            compiledGS->name       = source.GetName();
            compiledGS->entryPoint = GetEntryPoint(ShaderStage::Geometry);
            compiledGS->profile    = GetShaderProfile(ShaderStage::Geometry);
            compiledGS->sourceCode = source.GetGeometrySource().value();

            mergedOpts.entryPoint = compiledGS->entryPoint;
            mergedOpts.target     = compiledGS->profile;

            DXCCompiler::CompileResult gsResult = compiler.CompileShader(source.GetGeometrySource().value(), mergedOpts);
            compiledGS->success                 = gsResult.success;
            compiledGS->errorMessage            = gsResult.errorMessage;
            compiledGS->warningMessage          = gsResult.warningMessage;
            compiledGS->bytecode                = std::move(gsResult.bytecode);

            if (compiledGS->IsValid())
            {
                result.geometryShader = std::move(compiledGS);
            }
            else
            {
                std::cerr << "[ShaderProgramBuilder] Warning: Geometry shader compilation failed for "
                    << source.GetName() << std::endl;
            }
        }

        // 7. 编译计算着色器（可选，使用合并后的选项）
        if (source.HasComputeShader())
        {
            auto compiledCS        = std::make_unique<CompiledShader>();
            compiledCS->stage      = ShaderStage::Compute;
            compiledCS->name       = source.GetName();
            compiledCS->entryPoint = GetEntryPoint(ShaderStage::Compute);
            compiledCS->profile    = GetShaderProfile(ShaderStage::Compute);
            compiledCS->sourceCode = source.GetComputeSource().value();

            mergedOpts.entryPoint = compiledCS->entryPoint;
            mergedOpts.target     = compiledCS->profile;

            DXCCompiler::CompileResult csResult = compiler.CompileShader(source.GetComputeSource().value(), mergedOpts);
            compiledCS->success                 = csResult.success;
            compiledCS->errorMessage            = csResult.errorMessage;
            compiledCS->warningMessage          = csResult.warningMessage;
            compiledCS->bytecode                = std::move(csResult.bytecode);

            if (compiledCS->IsValid())
            {
                result.computeShader = std::move(compiledCS);
            }
            else
            {
                std::cerr << "[ShaderProgramBuilder] Warning: Compute shader compilation failed for "
                    << source.GetName() << std::endl;
            }
        }

        // 8. 存储 ProgramDirectives（从 ShaderSource 获取）
        result.directives = source.GetDirectives();

        // 9. 成功
        result.success = true;
        return result;
    }
} // namespace enigma::graphic
