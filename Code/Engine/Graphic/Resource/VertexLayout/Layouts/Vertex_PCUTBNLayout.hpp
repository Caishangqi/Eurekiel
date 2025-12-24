#pragma once

// ============================================================================
// Vertex_PCUTBNLayout.hpp - Vertex layout for Position + Color + UV + TBN
// 
// Layout: 60 bytes total
// - POSITION (R32G32B32_FLOAT, offset 0, 12 bytes)
// - COLOR (R8G8B8A8_UNORM, offset 12, 4 bytes)
// - TEXCOORD (R32G32_FLOAT, offset 16, 8 bytes)
// - TANGENT (R32G32B32_FLOAT, offset 24, 12 bytes)
// - BITANGENT (R32G32B32_FLOAT, offset 36, 12 bytes)
// - NORMAL (R32G32B32_FLOAT, offset 48, 12 bytes)
// 
// [IMPORTANT] This layout replaces the hardcoded InputLayout in PSOManager.cpp
// ============================================================================

#include "../VertexLayout.hpp"

namespace enigma::graphic
{
    /**
     * @brief Vertex layout for Position + Color + UV + Tangent + Bitangent + Normal
     * 
     * Matches Vertex_PCUTBN struct in Core/Vertex_PCU.hpp:
     * - Vec3 m_position (12 bytes)
     * - Rgba8 m_color (4 bytes)
     * - Vec2 m_uvTexCoords (8 bytes)
     * - Vec3 m_tangent (12 bytes)
     * - Vec3 m_bitangent (12 bytes)
     * - Vec3 m_normal (12 bytes)
     * 
     * Total: 60 bytes
     * 
     * This is the default layout set by VertexLayoutRegistry::Initialize().
     */
    class Vertex_PCUTBNLayout : public VertexLayout
    {
    public:
        Vertex_PCUTBNLayout();
        ~Vertex_PCUTBNLayout() override = default;

        // ========================================================================
        // VertexLayout Interface
        // ========================================================================

        [[nodiscard]] const D3D12_INPUT_ELEMENT_DESC* GetInputElements() const override;
        [[nodiscard]] uint32_t                        GetInputElementCount() const override;

        // ========================================================================
        // Static Type-Safe Access
        // ========================================================================

        /**
         * @brief Get Vertex_PCUTBNLayout from registry (type-safe)
         * @return Typed pointer to this layout, or nullptr if not registered
         */
        [[nodiscard]] static const Vertex_PCUTBNLayout* Get();

    private:
        static const D3D12_INPUT_ELEMENT_DESC s_elements[6];
    };
} // namespace enigma::graphic
