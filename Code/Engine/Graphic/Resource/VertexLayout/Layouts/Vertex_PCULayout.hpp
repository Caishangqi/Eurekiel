#pragma once

// ============================================================================
// Vertex_PCULayout.hpp - Vertex layout for Position + Color + UV
// 
// Layout: 24 bytes total
// - POSITION (R32G32B32_FLOAT, offset 0, 12 bytes)
// - COLOR (R8G8B8A8_UNORM, offset 12, 4 bytes)
// - TEXCOORD (R32G32_FLOAT, offset 16, 8 bytes)
// ============================================================================

#include "../VertexLayout.hpp"

namespace enigma::graphic
{
    /**
     * @brief Vertex layout for Position + Color + UV
     * 
     * Matches Vertex_PCU struct in Core/Vertex_PCU.hpp:
     * - Vec3 m_position (12 bytes)
     * - Rgba8 m_color (4 bytes)
     * - Vec2 m_uvTextCoords (8 bytes)
     * 
     * Total: 24 bytes
     */
    class Vertex_PCULayout : public VertexLayout
    {
    public:
        Vertex_PCULayout();
        ~Vertex_PCULayout() override = default;

        // ========================================================================
        // VertexLayout Interface
        // ========================================================================

        [[nodiscard]] const D3D12_INPUT_ELEMENT_DESC* GetInputElements() const override;
        [[nodiscard]] uint32_t                        GetInputElementCount() const override;

        // ========================================================================
        // Static Type-Safe Access
        // ========================================================================

        /**
         * @brief Get Vertex_PCULayout from registry (type-safe)
         * @return Typed pointer to this layout, or nullptr if not registered
         */
        [[nodiscard]] static const Vertex_PCULayout* Get();

    private:
        static const D3D12_INPUT_ELEMENT_DESC s_elements[3];
    };
} // namespace enigma::graphic
