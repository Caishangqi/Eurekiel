#pragma once

// ============================================================================
// TerrainVertexLayout.hpp - Vertex layout for terrain rendering
// 
// Layout: 44 bytes total
// - POSITION (R32G32B32_FLOAT, offset 0, 12 bytes)
// - COLOR (R8G8B8A8_UNORM, offset 12, 4 bytes)
// - TEXCOORD0 (R32G32_FLOAT, offset 16, 8 bytes) - UV coordinates
// - NORMAL (R32G32B32_FLOAT, offset 24, 12 bytes)
// - LIGHTMAP (R32G32_FLOAT, offset 36, 8 bytes) - Lightmap coordinates
//
// [IMPORTANT] Lightmap data convention:
// - m_lightmapCoord.x = blocklight (0.0 - 1.0)
// - m_lightmapCoord.y = skylight (0.0 - 1.0)
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
    // Total: 44 bytes
    // Used by ChunkMesh and World for terrain rendering data.
    //
    // [IMPORTANT] Lightmap data convention:
    // - m_lightmapCoord.x = blocklight (0.0 - 1.0, converted from 0-15)
    // - m_lightmapCoord.y = skylight (0.0 - 1.0, converted from 0-15)
    // ========================================================================
    struct TerrainVertex
    {
        Vec3  m_position; // 12 bytes, offset 0
        Rgba8 m_color; // 4 bytes, offset 12
        Vec2  m_uvTexCoords; // 8 bytes, offset 16
        Vec3  m_normal; // 12 bytes, offset 24
        Vec2  m_lightmapCoord; // 8 bytes, offset 36 [NEW]
        // Total: 44 bytes
    };

    /**
     * @brief Vertex layout for terrain rendering
     * 
     * Matches TerrainVertex struct layout (44 bytes):
     * - POSITION: float3 (12 bytes)
     * - COLOR: rgba8 (4 bytes)
     * - TEXCOORD0: float2 (8 bytes) - UV coordinates
     * - NORMAL: float3 (12 bytes)
     * - LIGHTMAP: float2 (8 bytes) - Lightmap coordinates
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
        static const D3D12_INPUT_ELEMENT_DESC s_elements[5];
    };
} // namespace enigma::graphic
