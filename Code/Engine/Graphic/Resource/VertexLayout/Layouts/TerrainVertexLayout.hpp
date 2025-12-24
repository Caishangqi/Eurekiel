#pragma once

// ============================================================================
// TerrainVertexLayout.hpp - Vertex layout for terrain rendering
// 
// [PLACEHOLDER] Currently uses same format as Vertex_PCUTBN (60 bytes)
// Can be customized based on actual terrain rendering needs.
// 
// Layout: 60 bytes total (same as Vertex_PCUTBN)
// - POSITION (R32G32B32_FLOAT, offset 0, 12 bytes)
// - COLOR (R8G8B8A8_UNORM, offset 12, 4 bytes)
// - TEXCOORD (R32G32_FLOAT, offset 16, 8 bytes)
// - TANGENT (R32G32B32_FLOAT, offset 24, 12 bytes)
// - BITANGENT (R32G32B32_FLOAT, offset 36, 12 bytes)
// - NORMAL (R32G32B32_FLOAT, offset 48, 12 bytes)
// ============================================================================

#include "../VertexLayout.hpp"

namespace enigma::graphic
{
    /**
     * @brief Vertex layout for terrain rendering
     * 
     * [PLACEHOLDER] Currently identical to Vertex_PCUTBNLayout.
     * Can be customized for terrain-specific needs such as:
     * - Blend weights for texture splatting
     * - Additional UV coordinates for detail textures
     * - Terrain-specific attributes
     * 
     * Total: 60 bytes (currently)
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
        static const D3D12_INPUT_ELEMENT_DESC s_elements[6];
    };
} // namespace enigma::graphic
