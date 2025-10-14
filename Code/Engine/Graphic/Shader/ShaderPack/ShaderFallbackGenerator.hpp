/**
 * @file ShaderFallbackGenerator.hpp
 * @brief Shader Fallback 生成器 - Iris 兼容 Fallback 链管理 + 资源系统集成
 * @date 2025-10-04
 *
 * 设计决策:
 * + 使用预编译的 HLSL 模板 (gbuffers_basic, gbuffers_textured)
 * + 支持 Fallback = null (Shadow passes 跳过渲染)
 * + 完整实现 Iris Fallback 链 (Water → Terrain → TexturedLit → Textured → Basic)
 * + 资源系统集成 - 引擎核心着色器通过资源系统加载 (engine:shaders/core/*)
 * ❌ 不动态生成着色器 (简化设计, 利用固定 Input Layout)
 *
 * 教学要点:
 * 1. Fallback 机制 - 当 Shader Pack 未提供某个 Program 时自动回退
 * 2. Fallback = null 处理 - Shadow passes 可以没有 Fallback (跳过渲染)
 * 3. 静态 Fallback 模板 - 只需 2 个预编译 HLSL 文件
 * 4. 资源系统集成 - 使用 ResourceLocation 引用引擎核心着色器
 *
 * Iris 参考:
 * - ProgramFallbackResolver.java - Fallback 链递归解析
 * - ProgramId.java - Fallback 链定义
 */

#pragma once

#include "ShaderPack.hpp"
#include "Engine/Resource/ResourceCommon.hpp" // 资源系统集成
#include <optional>
#include <string>

namespace enigma::graphic
{
    /**
     * @class ShaderFallbackGenerator
     * @brief Shader Fallback 生成器 - 资源系统集成版
     *
     * 职责:
     * - 提供 Fallback 着色器资源标识符 (ResourceLocation)
     * - 判断某个 Program 是否支持 Fallback
     * - 返回 Fallback 链 (用于调试和日志)
     *
     * 不负责:
     * - ❌ 动态生成 HLSL 代码 (使用预编译模板)
     * - ❌ 编译着色器 (由 DXCCompiler 负责)
     * - ❌ 加载 Shader Pack (由 ShaderPackLoader 负责)
     *
     * 资源系统集成:
     * - 引擎核心着色器存储在 .enigma/assets/engine/shaders/core/
     * - 使用 ResourceLocation("engine", "shaders/core/gbuffers_basic.vs") 访问
     * - 通过 g_theResource->GetResource() 加载二进制数据
     * - 传递给 DXCCompiler::CompileFromMemory() 编译
     */
    class ShaderFallbackGenerator
    {
    public:
        /**
         * @struct ShaderResourceRef
         * @brief 着色器资源引用 (ResourceLocation 版本)
         *
         * 教学要点:
         * 1. 不再使用文件系统路径 (std::filesystem::path)
         * 2. 使用资源标识符 (ResourceLocation)
         * 3. 符合 Minecraft NeoForge 资源系统约定
         */
        struct ShaderResourceRef
        {
            enigma::resource::ResourceLocation vertexShader; ///< 顶点着色器资源标识符
            enigma::resource::ResourceLocation pixelShader; ///< 像素着色器资源标识符
        };

        ShaderFallbackGenerator()  = default;
        ~ShaderFallbackGenerator() = default;

        /**
         * @brief 获取 Fallback 着色器资源引用
         * @param id Program ID
         * @return Fallback 着色器资源标识符 (ResourceLocation)
         *
         * 教学要点:
         * 1. 返回资源标识符而非文件路径
         * 2. 格式: "engine:shaders/core/gbuffers_basic.vs" (无 .hlsl 扩展名)
         * 3. 可直接传递给 g_theResource->GetResource()
         */
        std::optional<ShaderResourceRef> GetFallbackShader(ProgramId id) const;

        bool                   HasFallback(ProgramId id) const;
        std::vector<ProgramId> GetFallbackChain(ProgramId id) const;
        std::string            GetFallbackDescription(ProgramId id) const;

    private:
        std::optional<ProgramId> GetDirectFallback(ProgramId id) const;
    };
} // namespace enigma::graphic
