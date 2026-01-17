#pragma once

// ============================================================================
// TerrainVertexLayout.hpp - Vertex layout for terrain rendering
// 
// Layout: 56 bytes total (Phase 1: Iris-compatible extension)
// - POSITION (R32G32B32_FLOAT, offset 0, 12 bytes)
// - COLOR (R8G8B8A8_UNORM, offset 12, 4 bytes)
// - TEXCOORD0 (R32G32_FLOAT, offset 16, 8 bytes) - UV coordinates
// - NORMAL (R32G32B32_FLOAT, offset 24, 12 bytes)
// - LIGHTMAP (R32G32_FLOAT, offset 36, 8 bytes) - Lightmap coordinates
// - ENTITY_ID (R16_UINT, offset 44, 2 bytes) - Block ID (mc_Entity in Iris)
// - PADDING (2 bytes, offset 46) - Alignment padding for MID_TEXCOORD
// - MID_TEXCOORD (R32G32_FLOAT, offset 48, 8 bytes) - Texture center (mc_midTexCoord in Iris)
//
// [IMPORTANT] Lightmap data convention:
// - m_lightmapCoord.x = blocklight (0.0 - 1.0)
// - m_lightmapCoord.y = skylight (0.0 - 1.0)
//
// [IMPORTANT] Phase 1 excludes m_tangent (TBN matrix):
// - Normals already transformed to world space in vertex shader
// - TBN matrix only needed for normal mapping (Phase 2)
// ============================================================================

#include "../../Graphic/Resource/VertexLayout/VertexLayout.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/Rgba8.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // TerrainVertex - Vertex data structure for terrain rendering
    // 
    // Total: 56 bytes (Phase 1: Iris-compatible extension)
    // Used by ChunkMesh and World for terrain rendering data.
    //
    // [IMPORTANT] Lightmap data convention:
    // - m_lightmapCoord.x = blocklight (0.0 - 1.0, converted from 0-15)
    // - m_lightmapCoord.y = skylight (0.0 - 1.0, converted from 0-15)
    //
    // [IMPORTANT] Iris-compatible attributes (Phase 1):
    // - m_entityId: Block ID from BlockRegistry (mc_Entity in Iris)
    // - m_midTexCoord: Texture center for animation (mc_midTexCoord in Iris)
    // - Phase 1 excludes m_tangent (TBN matrix not needed)
    // ========================================================================
    struct TerrainVertex
    {
        Vec3  m_position; // 12 bytes, offset 0
        Rgba8 m_color; // 4 bytes, offset 12
        Vec2  m_uvTexCoords; // 8 bytes, offset 16
        Vec3  m_normal; // 12 bytes, offset 24
        Vec2  m_lightmapCoord; // 8 bytes, offset 36

        // [NEW] Phase 1: Iris-compatible extension (12 bytes with padding)
        uint16_t m_entityId; // 2 bytes, offset 44 - Block ID (mc_Entity)
        uint16_t m_padding; // 2 bytes, offset 46 - Alignment padding
        Vec2     m_midTexCoord; // 8 bytes, offset 48 - Texture center (mc_midTexCoord)

        // Total: 56 bytes
    };

    /**
     * @brief Vertex layout for terrain rendering
     * 
     * Matches TerrainVertex struct layout (56 bytes, Phase 1):
     * - POSITION: float3 (12 bytes)
     * - COLOR: rgba8 (4 bytes)
     * - TEXCOORD0: float2 (8 bytes) - UV coordinates
     * - NORMAL: float3 (12 bytes)
     * - LIGHTMAP: float2 (8 bytes) - Lightmap coordinates
     * - ENTITY_ID: uint16 (2 bytes) - Block ID (mc_Entity in Iris)
     * - PADDING: uint16 (2 bytes) - Alignment padding
     * - MID_TEXCOORD: float2 (8 bytes) - Texture center (mc_midTexCoord in Iris)
     * 
     * Registered by Game RenderPass (TerrainRenderPass::Initialize()).
     */
    class TerrainVertexLayout : public VertexLayout
    {
    public:
        TerrainVertexLayout();
        ~TerrainVertexLayout() override = default;

        // ========================================================================
        // VertexLayout Interface
        // ========================================================================

        [[nodiscard]] const D3D12_INPUT_ELEMENT_DESC* GetInputElements() const override;
        [[nodiscard]] uint32_t                        GetInputElementCount() const override;

        // ========================================================================
        // Static Type-Safe Access
        // ========================================================================

        /**
         * @brief Get TerrainVertexLayout from registry (type-safe)
         * @return Typed pointer to this layout, or nullptr if not registered
         */
        [[nodiscard]] static const TerrainVertexLayout* Get();

    private:
        static const D3D12_INPUT_ELEMENT_DESC s_elements[7]; // [UPDATED] 5 -> 7 elements
    };
} // namespace enigma::graphic
