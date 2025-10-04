/**
 * @file ShaderProgramBuilder.hpp
 * @brief 着色器程序构建器 - 对应 Iris ShaderCreator
 * @date 2025-10-03
 *
 * 职责:
 * 1. 从 ShaderSource 编译生成 CompiledShader (顶点 + 像素)
 * 2. 调用 DXCCompiler 进行 HLSL → DXIL 编译
 * 3. 处理 ShaderDirectives 配置
 * 4. 支持 Geometry/Compute 等可选着色器阶段
 *
 * 对应 Iris:
 * - ShaderCreator.java (静态工厂方法)
 * - 输入: ProgramSource
 * - 输出: Program (已编译的 OpenGL 程序)
 */

#pragma once

#include "ShaderSource.hpp"
#include "Engine/Graphic/Resource/CompiledShader.hpp"
#include "Engine/Graphic/Shader/Compiler/DXCCompiler.hpp"
#include <memory>
#include <optional>

namespace enigma::graphic
{
    /**
     * @class ShaderProgramBuilder
     * @brief 着色器程序构建器 - 静态工厂类
     *
     * 教学要点:
     * - 对应 Iris 的 ShaderCreator.java
     * - 负责编译单个 Program (VS + PS 一对)
     * - 使用静态方法,无需实例化
     * - 采用 RAII 原则,自动管理编译资源
     *
     * 设计决策:
     * - 静态类 - 所有方法都是静态的,符合工厂模式
     * - 单一职责 - 只负责编译,不管理已编译的程序
     * - 依赖 DXCCompiler - 通过组合而非继承
     */
    class ShaderProgramBuilder
    {
    public:
        // 禁止实例化 (纯静态工厂类)
        ShaderProgramBuilder()  = delete;
        ~ShaderProgramBuilder() = delete;

        /**
         * @brief 编译结果结构体
         *
         * 教学要点:
         * - 包含所有编译后的着色器阶段
         * - 失败时返回错误信息
         * - 使用 std::optional 表示可选阶段
         */
        struct BuildResult
        {
            bool        success = false; // 是否成功
            std::string errorMessage; // 错误信息

            std::unique_ptr<CompiledShader> vertexShader; // 顶点着色器 (必需)
            std::unique_ptr<CompiledShader> pixelShader; // 像素着色器 (必需)
            std::unique_ptr<CompiledShader> geometryShader; // 几何着色器 (可选)
            std::unique_ptr<CompiledShader> computeShader; // 计算着色器 (可选)

            ShaderDirectives directives; // 从源码解析的指令
        };

        /**
         * @brief 从 ShaderSource 构建着色器程序
         * @param source 着色器源码容器
         * @param type 着色器类型 (对应 Iris ProgramId)
         * @return BuildResult 包含编译后的着色器
         *
         * 教学要点:
         * - 对应 Iris ShaderCreator.create()
         * - 自动处理所有编译步骤
         * - 顶点和像素着色器是必需的
         * - 几何和计算着色器是可选的
         *
         * 编译流程:
         * 1. 验证 ShaderSource 是否有效 (vertex + pixel 必须存在)
         * 2. 调用 DXCCompiler 编译顶点着色器
         * 3. 调用 DXCCompiler 编译像素着色器
         * 4. 如果存在,编译几何/计算着色器
         * 5. 提取并合并 ShaderDirectives
         * 6. 返回 BuildResult
         */
        static BuildResult BuildProgram(
            const ShaderSource& source,
            ShaderType          type
        );

        /**
         * @brief 编译单个着色器阶段
         * @param source HLSL 源码字符串
         * @param stage 着色器阶段 (Vertex, Pixel, Geometry, Compute)
         * @param name 着色器名称 (如 "gbuffers_terrain")
         * @param directives 着色器指令 (用于配置编译选项)
         * @return CompiledShader 编译后的着色器
         *
         * 教学要点:
         * - 内部辅助方法,封装 DXCCompiler 调用
         * - 根据 ShaderStage 自动选择 Profile (vs_6_6, ps_6_6 等)
         * - 处理编译错误和警告
         */
        static std::unique_ptr<CompiledShader> CompileShaderStage(
            const std::string&      source,
            ShaderStage             stage,
            const std::string&      name,
            const ShaderDirectives& directives
        );

    private:
        /**
         * @brief 根据 ShaderStage 获取 HLSL Profile
         * @param stage 着色器阶段
         * @return Profile 字符串 (如 "vs_6_6", "ps_6_6")
         *
         * 教学要点:
         * - Shader Model 6.6 对应 DXC
         * - 支持 Vertex, Pixel, Geometry, Compute
         * - 未来可扩展 Hull/Domain 阶段
         */
        static std::string GetShaderProfile(ShaderStage stage);

        /**
         * @brief 根据 ShaderStage 获取入口点名称
         * @param stage 着色器阶段
         * @return 入口函数名 (如 "VSMain", "PSMain")
         *
         * 教学要点:
         * - 标准化入口点命名
         * - 所有着色器遵循统一约定
         */
        static std::string GetEntryPoint(ShaderStage stage);

        /**
         * @brief 根据 ShaderDirectives 配置 DXCCompiler 选项
         * @param directives 着色器指令
         * @param stage 着色器阶段
         * @return CompileOptions DXC 编译选项
         *
         * 教学要点:
         * - 根据 Iris 注释配置编译选项
         * - 例如: const int shadowMapResolution 生成 -D SHADOW_MAP_RES=4096
         * - blend 模式影响 PSO 配置,不影响编译
         */
        static DXCCompiler::CompileOptions ConfigureCompileOptions(
            const ShaderDirectives& directives,
            ShaderStage             stage
        );

        /**
         * @brief 合并多个着色器阶段的 ShaderDirectives
         * @param vertexDirectives 顶点着色器指令
         * @param pixelDirectives 像素着色器指令
         * @return 合并后的 ShaderDirectives
         *
         * 教学要点:
         * - Iris 的注释可能在 VS 或 PS 中
         * - 像素着色器的指令优先级更高
         * - 处理冲突时保留 PS 的配置
         */
        static ShaderDirectives MergeDirectives(
            const ShaderDirectives& vertexDirectives,
            const ShaderDirectives& pixelDirectives
        );
    };
} // namespace enigma::graphic
