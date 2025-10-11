/**
 * @file ProgramId.hpp
 * @brief Iris兼容的单一程序ID枚举 - 对应 ProgramId.java
 * @date 2025-10-04
 *
 * 对应 Iris 源码:
 * F:\...\Iris\shaderpack\loading\ProgramId.java
 *
 * 设计要点:
 * - ProgramId: 单一程序 (每个枚举值对应一个着色器程序)
 * - ProgramArrayId: 程序数组 (每个枚举值对应100个槽位的着色器数组)
 *
 * 文件命名规则:
 * - ProgramId: gbuffers_terrain.vsh, gbuffers_entities.fsh (无数字后缀)
 * - ProgramArrayId: composite.vsh, composite1.vsh, composite2.vsh...composite99.vsh
 *
 * Fallback Chain 示例:
 * - TerrainCutout → Terrain → TexturedLit → Textured → Basic
 */

#pragma once

#include <string>
#include <optional>

namespace enigma::graphic
{
    /**
     * @enum ProgramId
     * @brief 单一着色器程序类型枚举
     *
     * Iris 设计原则:
     * - 每个枚举值对应一个唯一的着色器程序
     * - 支持 Fallback Chain (回退链)
     * - 文件名无数字后缀 (例: gbuffers_terrain.vsh)
     *
     * 对应 Iris ProgramId.java 的完整47个程序类型
     */
    enum class ProgramId
    {
        // ========================================================================
        // Shadow 阴影程序组
        // ========================================================================
        Shadow, // shadow.vsh/fsh (基础阴影)
        ShadowSolid, // shadow_solid.vsh/fsh (回退: Shadow)
        ShadowCutout, // shadow_cutout.vsh/fsh (回退: Shadow)
        ShadowWater, // shadow_water.vsh/fsh (回退: Shadow)
        ShadowEntities, // shadow_entities.vsh/fsh (回退: Shadow)
        ShadowLightning, // shadow_lightning.vsh/fsh (回退: ShadowEntities)
        ShadowBlock, // shadow_block.vsh/fsh (回退: Shadow)

        // ========================================================================
        // Gbuffers 基础程序组
        // ========================================================================
        Basic, // gbuffers_basic.vsh/fsh (最基础)
        Line, // gbuffers_line.vsh/fsh (回退: Basic)
        Textured, // gbuffers_textured.vsh/fsh (回退: Basic)
        TexturedLit, // gbuffers_textured_lit.vsh/fsh (回退: Textured)

        // ========================================================================
        // Gbuffers 天空程序组
        // ========================================================================
        SkyBasic, // gbuffers_skybasic.vsh/fsh (回退: Basic)
        SkyTextured, // gbuffers_skytextured.vsh/fsh (回退: Textured)
        Clouds, // gbuffers_clouds.vsh/fsh (回退: Textured)

        // ========================================================================
        // Gbuffers 地形程序组
        // ========================================================================
        Terrain, // gbuffers_terrain.vsh/fsh (回退: TexturedLit)
        TerrainSolid, // gbuffers_terrain_solid.vsh/fsh (回退: Terrain)
        TerrainCutout, // gbuffers_terrain_cutout.vsh/fsh (回退: Terrain)
        DamagedBlock, // gbuffers_damagedblock.vsh/fsh (回退: Terrain)

        // ========================================================================
        // Gbuffers 方块程序组
        // ========================================================================
        Block, // gbuffers_block.vsh/fsh (回退: Terrain)
        BlockTrans, // gbuffers_block_translucent.vsh/fsh (回退: Block)
        BeaconBeam, // gbuffers_beaconbeam.vsh/fsh (回退: Textured)
        Item, // gbuffers_item.vsh/fsh (回退: TexturedLit)

        // ========================================================================
        // Gbuffers 实体程序组
        // ========================================================================
        Entities, // gbuffers_entities.vsh/fsh (回退: TexturedLit)
        EntitiesTrans, // gbuffers_entities_translucent.vsh/fsh (回退: Entities)
        Lightning, // gbuffers_lightning.vsh/fsh (回退: Entities)
        Particles, // gbuffers_particles.vsh/fsh (回退: TexturedLit)
        ParticlesTrans, // gbuffers_particles_translucent.vsh/fsh (回退: Particles)
        EntitiesGlowing, // gbuffers_entities_glowing.vsh/fsh (回退: Entities)
        ArmorGlint, // gbuffers_armor_glint.vsh/fsh (回退: Textured)
        SpiderEyes, // gbuffers_spidereyes.vsh/fsh (回退: Textured, 特殊混合模式)

        // ========================================================================
        // Gbuffers 手部和天气
        // ========================================================================
        Hand, // gbuffers_hand.vsh/fsh (回退: TexturedLit)
        Weather, // gbuffers_weather.vsh/fsh (回退: TexturedLit)
        Water, // gbuffers_water.vsh/fsh (回退: Terrain)
        HandWater, // gbuffers_hand_water.vsh/fsh (回退: Hand)

        // ========================================================================
        // Distant Horizons (DH) 程序组
        // ========================================================================
        DhTerrain, // dh_terrain.vsh/fsh (Distant Horizons 地形)
        DhWater, // dh_water.vsh/fsh (回退: DhTerrain)
        DhGeneric, // dh_generic.vsh/fsh (回退: DhTerrain)
        DhShadow, // dh_shadow.vsh/fsh (Distant Horizons 阴影)

        // ========================================================================
        // Final 最终合成
        // ========================================================================
        Final, // final.vsh/fsh (最终输出)

        COUNT // 总数 = 47
    };

    /**
     * @brief 将 ProgramId 转换为源文件名 (不含扩展名)
     *
     * Iris 规则: group.getBaseName() + "_" + name
     * 例如: Terrain → "gbuffers_terrain"
     *       Shadow → "shadow"
     *       Final → "final"
     *
     * @param id 程序ID
     * @return 源文件名前缀 (例: "gbuffers_terrain", "shadow_solid")
     */
    inline std::string ProgramIdToSourceName(ProgramId id)
    {
        switch (id)
        {
        // Shadow 组
        case ProgramId::Shadow: return "shadow";
        case ProgramId::ShadowSolid: return "shadow_solid";
        case ProgramId::ShadowCutout: return "shadow_cutout";
        case ProgramId::ShadowWater: return "shadow_water";
        case ProgramId::ShadowEntities: return "shadow_entities";
        case ProgramId::ShadowLightning: return "shadow_lightning";
        case ProgramId::ShadowBlock: return "shadow_block";

        // Gbuffers 基础组
        case ProgramId::Basic: return "gbuffers_basic";
        case ProgramId::Line: return "gbuffers_line";
        case ProgramId::Textured: return "gbuffers_textured";
        case ProgramId::TexturedLit: return "gbuffers_textured_lit";

        // Gbuffers 天空组
        case ProgramId::SkyBasic: return "gbuffers_skybasic";
        case ProgramId::SkyTextured: return "gbuffers_skytextured";
        case ProgramId::Clouds: return "gbuffers_clouds";

        // Gbuffers 地形组
        case ProgramId::Terrain: return "gbuffers_terrain";
        case ProgramId::TerrainSolid: return "gbuffers_terrain_solid";
        case ProgramId::TerrainCutout: return "gbuffers_terrain_cutout";
        case ProgramId::DamagedBlock: return "gbuffers_damagedblock";

        // Gbuffers 方块组
        case ProgramId::Block: return "gbuffers_block";
        case ProgramId::BlockTrans: return "gbuffers_block_translucent";
        case ProgramId::BeaconBeam: return "gbuffers_beaconbeam";
        case ProgramId::Item: return "gbuffers_item";

        // Gbuffers 实体组
        case ProgramId::Entities: return "gbuffers_entities";
        case ProgramId::EntitiesTrans: return "gbuffers_entities_translucent";
        case ProgramId::Lightning: return "gbuffers_lightning";
        case ProgramId::Particles: return "gbuffers_particles";
        case ProgramId::ParticlesTrans: return "gbuffers_particles_translucent";
        case ProgramId::EntitiesGlowing: return "gbuffers_entities_glowing";
        case ProgramId::ArmorGlint: return "gbuffers_armor_glint";
        case ProgramId::SpiderEyes: return "gbuffers_spidereyes";

        // Gbuffers 手部和天气
        case ProgramId::Hand: return "gbuffers_hand";
        case ProgramId::Weather: return "gbuffers_weather";
        case ProgramId::Water: return "gbuffers_water";
        case ProgramId::HandWater: return "gbuffers_hand_water";

        // Distant Horizons
        case ProgramId::DhTerrain: return "dh_terrain";
        case ProgramId::DhWater: return "dh_water";
        case ProgramId::DhGeneric: return "dh_generic";
        case ProgramId::DhShadow: return "dh_shadow";

        // Final
        case ProgramId::Final: return "final";

        default: return "unknown";
        }
    }

    /**
     * @brief 获取 ProgramId 的 Fallback 回退程序
     *
     * Iris Fallback Chain 示例:
     * - TerrainCutout → Terrain → TexturedLit → Textured → Basic → nullptr
     * - ShadowLightning → ShadowEntities → Shadow → nullptr
     *
     * 使用方式:
     * ```cpp
     * ProgramId current = ProgramId::TerrainCutout;
     * while (current != ProgramId::COUNT) {
     *     auto fallback = GetProgramFallback(current);
     *     if (!fallback.has_value()) break;
     *     current = fallback.value();
     * }
     * ```
     *
     * @param id 当前程序ID
     * @return Fallback程序ID (如果没有回退则返回nullopt)
     */
    inline std::optional<ProgramId> GetProgramFallback(ProgramId id)
    {
        switch (id)
        {
        // Shadow 组回退链
        case ProgramId::ShadowSolid: return ProgramId::Shadow;
        case ProgramId::ShadowCutout: return ProgramId::Shadow;
        case ProgramId::ShadowWater: return ProgramId::Shadow;
        case ProgramId::ShadowEntities: return ProgramId::Shadow;
        case ProgramId::ShadowLightning: return ProgramId::ShadowEntities;
        case ProgramId::ShadowBlock: return ProgramId::Shadow;

        // Gbuffers 回退链
        case ProgramId::Line: return ProgramId::Basic;
        case ProgramId::Textured: return ProgramId::Basic;
        case ProgramId::TexturedLit: return ProgramId::Textured;
        case ProgramId::SkyBasic: return ProgramId::Basic;
        case ProgramId::SkyTextured: return ProgramId::Textured;
        case ProgramId::Clouds: return ProgramId::Textured;

        // 地形回退链
        case ProgramId::Terrain: return ProgramId::TexturedLit;
        case ProgramId::TerrainSolid: return ProgramId::Terrain;
        case ProgramId::TerrainCutout: return ProgramId::Terrain;
        case ProgramId::DamagedBlock: return ProgramId::Terrain;

        // 方块回退链
        case ProgramId::Block: return ProgramId::Terrain;
        case ProgramId::BlockTrans: return ProgramId::Block;
        case ProgramId::BeaconBeam: return ProgramId::Textured;
        case ProgramId::Item: return ProgramId::TexturedLit;

        // 实体回退链
        case ProgramId::Entities: return ProgramId::TexturedLit;
        case ProgramId::EntitiesTrans: return ProgramId::Entities;
        case ProgramId::Lightning: return ProgramId::Entities;
        case ProgramId::Particles: return ProgramId::TexturedLit;
        case ProgramId::ParticlesTrans: return ProgramId::Particles;
        case ProgramId::EntitiesGlowing: return ProgramId::Entities;
        case ProgramId::ArmorGlint: return ProgramId::Textured;
        case ProgramId::SpiderEyes: return ProgramId::Textured;

        // 手部和天气回退链
        case ProgramId::Hand: return ProgramId::TexturedLit;
        case ProgramId::Weather: return ProgramId::TexturedLit;
        case ProgramId::Water: return ProgramId::Terrain;
        case ProgramId::HandWater: return ProgramId::Hand;

        // Distant Horizons 回退链
        case ProgramId::DhWater: return ProgramId::DhTerrain;
        case ProgramId::DhGeneric: return ProgramId::DhTerrain;

        // 基础程序和最终程序无回退
        case ProgramId::Shadow:
        case ProgramId::Basic:
        case ProgramId::DhTerrain:
        case ProgramId::DhShadow:
        case ProgramId::Final:
        default: return std::nullopt;
        }
    }
} // namespace enigma::graphic
