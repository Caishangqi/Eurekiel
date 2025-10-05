/**
 * @file ShaderFallbackGenerator.cpp
 * @brief Shader Fallback 生成器实现 - 资源系统集成版
 * @date 2025-10-04
 */

#include "ShaderFallbackGenerator.hpp"

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
            if (currentId == ProgramId::GBUFFERS_BASIC || currentId == ProgramId::GBUFFERS_TEXTURED)
            {
                ShaderResourceRef shaderRef;
                std::string       baseName =
                    (currentId == ProgramId::GBUFFERS_BASIC) ? "gbuffers_basic" : "gbuffers_textured";

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
        if (id == ProgramId::SHADOW || id == ProgramId::SHADOW_SOLID || id == ProgramId::SHADOW_CUTOUT)
        {
            return false;
        }

        if (id == ProgramId::FINAL)
        {
            return false;
        }

        if (id == ProgramId::GBUFFERS_BASIC)
        {
            return false;
        }

        return true;
    }

    std::vector<ShaderFallbackGenerator::ProgramId> ShaderFallbackGenerator::GetFallbackChain(
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
                finalFallback == ProgramId::GBUFFERS_BASIC ||
                finalFallback == ProgramId::GBUFFERS_TEXTURED)
            {
                break;
            }
            finalFallback = *nextFallback;
        }

        if (finalFallback == ProgramId::GBUFFERS_BASIC)
        {
            return "gbuffers_basic - Pure color rendering (no texture)";
        }
        else if (finalFallback == ProgramId::GBUFFERS_TEXTURED)
        {
            return "gbuffers_textured - Textured rendering (with sampling)";
        }

        return "Unknown fallback";
    }

    // ========================================================================
    // 私有方法实现 - Fallback 链定义
    // ========================================================================

    std::optional<ShaderFallbackGenerator::ProgramId> ShaderFallbackGenerator::GetDirectFallback(
        ProgramId id) const
    {
        switch (id)
        {
        case ProgramId::SHADOW:
        case ProgramId::SHADOW_SOLID:
        case ProgramId::SHADOW_CUTOUT:
        case ProgramId::FINAL:
        case ProgramId::GBUFFERS_BASIC:
            return std::nullopt;

        case ProgramId::GBUFFERS_TEXTURED:
            return ProgramId::GBUFFERS_BASIC;

        case ProgramId::GBUFFERS_TEXTURED_LIT:
            return ProgramId::GBUFFERS_TEXTURED;

        case ProgramId::GBUFFERS_SKYBASIC:
            return ProgramId::GBUFFERS_BASIC;

        case ProgramId::GBUFFERS_SKYTEXTURED:
            return ProgramId::GBUFFERS_TEXTURED;

        case ProgramId::GBUFFERS_CLOUDS:
            return ProgramId::GBUFFERS_TEXTURED;

        case ProgramId::GBUFFERS_TERRAIN:
            return ProgramId::GBUFFERS_TEXTURED_LIT;

        case ProgramId::GBUFFERS_TERRAIN_SOLID:
        case ProgramId::GBUFFERS_TERRAIN_CUTOUT:
        case ProgramId::GBUFFERS_DAMAGEDBLOCK:
        case ProgramId::GBUFFERS_BLOCK:
            return ProgramId::GBUFFERS_TERRAIN;

        case ProgramId::GBUFFERS_BEACONBEAM:
            return ProgramId::GBUFFERS_TEXTURED;

        case ProgramId::GBUFFERS_ITEM:
        case ProgramId::GBUFFERS_ENTITIES:
        case ProgramId::GBUFFERS_HAND:
        case ProgramId::GBUFFERS_WEATHER:
            return ProgramId::GBUFFERS_TEXTURED_LIT;

        case ProgramId::GBUFFERS_ARMOR_GLINT:
        case ProgramId::GBUFFERS_SPIDEREYES:
            return ProgramId::GBUFFERS_TEXTURED;

        case ProgramId::GBUFFERS_WATER:
            return ProgramId::GBUFFERS_TERRAIN;

        default:
            return ProgramId::GBUFFERS_TEXTURED;
        }
    }
} // namespace enigma::graphic
