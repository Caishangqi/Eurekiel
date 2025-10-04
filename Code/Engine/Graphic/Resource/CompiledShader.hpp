/**
 * @file CompiledShader.hpp
 * @brief 编译后的着色器程序 - 简化版 (移除反射)
 * @date 2025-10-03
 *
 * 设计变更:
 * ❌ 移除 ID3D12ShaderReflection - 使用固定 Input Layout
 * ❌ 移除 ID3DBlob - 使用 std::vector<uint8_t> 字节码
 * ✅ 保留 IrisAnnotations - 仍需解析注释配置
 * ✅ 保留 ShaderType/ShaderStage 枚举
 * ✅ 新增 sourceCode 字段 - 支持热重载
 */

#pragma once

#include "ShaderDirectives.hpp"
#include <memory>
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl/client.h>

namespace enigma::graphic
{
    /**
     * @brief 着色器程序类型枚举
     *
     * 教学要点: 对应 Iris 规范的各种着色器程序类型
     */
    enum class ShaderType
    {
        // Setup 阶段 - Compute Shader only
        Setup, // setup1.csh - setup99.csh

        // Begin 阶段 - Composite-style
        Begin, // begin1.vsh/.fsh - begin99.vsh/.fsh

        // Shadow 阶段 - Gbuffers-style
        Shadow, // shadow.vsh/.fsh

        // ShadowComp 阶段 - Composite-style
        ShadowComp, // shadowcomp1.vsh/.fsh - shadowcomp99.vsh/.fsh

        // Prepare 阶段 - Composite-style
        Prepare, // prepare1.vsh/.fsh - prepare99.vsh/.fsh

        // GBuffers 阶段 - Gbuffers-style (18 种不同程序)
        GBuffers_Terrain,
        GBuffers_Entities,
        GBuffers_EntitiesTranslucent,
        GBuffers_Hand,
        GBuffers_Weather,
        GBuffers_Block,
        GBuffers_BeaconBeam,
        GBuffers_Item,
        GBuffers_Entities_glowing,
        GBuffers_Glint,
        GBuffers_Eyes,
        GBuffers_Armor_glint,
        GBuffers_SpiderEyes,
        GBuffers_Hand_water,
        GBuffers_Textured,
        GBuffers_Textured_lit,
        GBuffers_Skybasic,
        GBuffers_Skytextured,
        GBuffers_Clouds,
        GBuffers_Water,

        // Deferred 阶段 - Composite-style
        Deferred, // deferred1.vsh/.fsh - deferred99.vsh/.fsh

        // Composite 阶段 - Composite-style
        Composite, // composite1.vsh/.fsh - composite99.vsh/.fsh

        // Final 阶段 - Composite-style
        Final // final.vsh/.fsh
    };

    /**
     * @brief 着色器阶段枚举 (GPU 流水线阶段)
     */
    enum class ShaderStage
    {
        Vertex, // 顶点着色器 (.vsh -> .hlsl)
        Pixel, // 像素着色器 (.fsh -> .hlsl)
        Compute, // 计算着色器 (.csh -> .hlsl)
        Geometry, // 几何着色器 (.gsh -> .hlsl) - 可选
        Hull, // 外壳着色器 (.tcs -> .hlsl) - 可选
        Domain // 域着色器 (.tes -> .hlsl) - 可选
    };

    /**
     * @brief 编译后的着色器程序 - 简化版
     *
     * 设计变更:
     * - ❌ 移除 ID3DBlob* bytecode
     * - ✅ 使用 std::vector<uint8_t> bytecode (与 DXCCompiler 一致)
     * - ❌ 移除 ID3D12PipelineState* (PSO 由外部管理)
     * - ✅ 保留 IrisAnnotations (仍需解析)
     * - ✅ 保留 sourceCode (支持热重载)
     */
    struct CompiledShader
    {
        // 着色器元数据
        ShaderType  type; // 着色器类型
        ShaderStage stage; // GPU 流水线阶段
        std::string name; // 着色器名称 (如 "gbuffers_terrain")
        std::string entryPoint; // 入口函数名 (如 "VSMain", "PSMain")
        std::string profile; // HLSL 编译配置 (如 "vs_6_6", "ps_6_6")

        // 编译结果
        std::vector<uint8_t> bytecode; // DXIL 字节码
        bool                 success; // 编译是否成功
        std::string          errorMessage; // 编译错误信息 (如果失败)
        std::string          warningMessage; // 编译警告信息

        // Iris 配置
        ShaderDirectives directives; // 解析的注释指令

        // 热重载支持
        std::string sourceCode; // 原始 HLSL 代码 (用于热重载)

        // 构造和析构
        CompiledShader()
            : type(ShaderType::GBuffers_Terrain)
              , stage(ShaderStage::Vertex)
              , success(false)
        {
        }

        ~CompiledShader() = default;

        // 禁用拷贝
        CompiledShader(const CompiledShader&)            = delete;
        CompiledShader& operator=(const CompiledShader&) = delete;

        // 支持移动
        CompiledShader(CompiledShader&& other) noexcept
            : type(other.type)
              , stage(other.stage)
              , name(std::move(other.name))
              , entryPoint(std::move(other.entryPoint))
              , profile(std::move(other.profile))
              , bytecode(std::move(other.bytecode))
              , success(other.success)
              , errorMessage(std::move(other.errorMessage))
              , warningMessage(std::move(other.warningMessage))
              , directives(std::move(other.directives))
              , sourceCode(std::move(other.sourceCode))
        {
        }

        CompiledShader& operator=(CompiledShader&& other) noexcept
        {
            if (this != &other)
            {
                type           = other.type;
                stage          = other.stage;
                name           = std::move(other.name);
                entryPoint     = std::move(other.entryPoint);
                profile        = std::move(other.profile);
                bytecode       = std::move(other.bytecode);
                success        = other.success;
                errorMessage   = std::move(other.errorMessage);
                warningMessage = std::move(other.warningMessage);
                directives     = std::move(other.directives);
                sourceCode     = std::move(other.sourceCode);
            }
            return *this;
        }

        /**
         * @brief 获取字节码指针
         * @return 字节码数据指针
         *
         * 教学要点: 用于创建 PSO
         */
        const void* GetBytecodePtr() const
        {
            return bytecode.data();
        }

        /**
         * @brief 获取字节码大小
         * @return 字节数
         */
        size_t GetBytecodeSize() const
        {
            return bytecode.size();
        }

        /**
         * @brief 检查是否有警告
         * @return 是否有警告
         */
        bool HasWarnings() const
        {
            return !warningMessage.empty();
        }

        /**
         * @brief 检查是否编译成功且有有效字节码
         * @return 是否可用
         */
        bool IsValid() const
        {
            return success && !bytecode.empty();
        }
    };
} // namespace enigma::graphic
