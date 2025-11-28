/**
 * @file ShaderFallbackGenerator.cpp
 * @brief Shader Fallback 生成器实现 - 资源系统集成版
 * @date 2025-10-04
 */

#include "ShaderFallbackGenerator.hpp"
#include "ProgramId.hpp" // 使用 GetProgramFallback() 函数

namespace enigma::graphic
{
    // ========================================================================
    // 公共方法实现
    // ========================================================================

    std::optional<ShaderFallbackGenerator::ShaderResourceRef> ShaderFallbackGenerator::GetFallbackShader(
        ProgramId id) const
    {
        if (!HasFallback(id))
        {
            return std::nullopt;
        }

        auto fallbackId = GetDirectFallback(id);
        if (!fallbackId)
        {
            return std::nullopt;
        }

        ProgramId currentId = *fallbackId;
        while (true)
        {
            if (currentId == ProgramId::Basic || currentId == ProgramId::Textured)
            {
                ShaderResourceRef shaderRef;
                std::string       baseName =
                    (currentId == ProgramId::Basic) ? "gbuffers_basic" : "gbuffers_textured";

                // 教学要点: 资源系统集成
                // 1. 使用 ResourceLocation 而非文件路径
                // 2. 格式: "engine:shaders/core/gbuffers_basic.vs" (无 .hlsl 扩展名)
                // 3. 符合 Minecraft NeoForge 约定
                using enigma::resource::ResourceLocation;

                shaderRef.vertexShader = ResourceLocation("engine", "shaders/core/" + baseName + ".vs");
                shaderRef.pixelShader  = ResourceLocation("engine", "shaders/core/" + baseName + ".ps");

                return shaderRef;
            }

            auto nextFallback = GetDirectFallback(currentId);
            if (!nextFallback)
            {
                return std::nullopt;
            }
            currentId = *nextFallback;
        }
    }

    bool ShaderFallbackGenerator::HasFallback(ProgramId id) const
    {
        if (id == ProgramId::Shadow || id == ProgramId::ShadowSolid || id == ProgramId::ShadowCutout)
        {
            return false;
        }

        if (id == ProgramId::Final)
        {
            return false;
        }

        if (id == ProgramId::Basic)
        {
            return false;
        }

        return true;
    }

    std::vector<ProgramId> ShaderFallbackGenerator::GetFallbackChain(
        ProgramId id) const
    {
        std::vector<ProgramId> chain;

        ProgramId currentId = id;
        while (true)
        {
            auto fallbackId = GetDirectFallback(currentId);
            if (!fallbackId)
            {
                break;
            }

            chain.push_back(*fallbackId);
            currentId = *fallbackId;

            if (chain.size() > 10)
            {
                break;
            }
        }

        return chain;
    }

    std::string ShaderFallbackGenerator::GetFallbackDescription(ProgramId id) const
    {
        auto fallbackId = GetDirectFallback(id);
        if (!fallbackId)
        {
            return "No fallback (Fallback = null)";
        }

        ProgramId finalFallback = *fallbackId;
        while (true)
        {
            auto nextFallback = GetDirectFallback(finalFallback);
            if (!nextFallback ||
                finalFallback == ProgramId::Basic ||
                finalFallback == ProgramId::Textured)
            {
                break;
            }
            finalFallback = *nextFallback;
        }

        if (finalFallback == ProgramId::Basic)
        {
            return "gbuffers_basic - Pure color rendering (no texture)";
        }
        else if (finalFallback == ProgramId::Textured)
        {
            return "gbuffers_textured - Textured rendering (with sampling)";
        }

        return "Unknown fallback";
    }

    // ========================================================================
    // 私有方法实现 - Fallback 链定义
    // ========================================================================

    std::optional<ProgramId> ShaderFallbackGenerator::GetDirectFallback(ProgramId id) const
    {
        // 教学要点: DRY 原则
        // 不重复实现 Fallback 逻辑，直接调用 ProgramId.hpp 中的函数
        return GetProgramFallback(id);
    }
} // namespace enigma::graphic
