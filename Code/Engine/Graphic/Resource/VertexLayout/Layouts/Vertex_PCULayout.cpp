#include "Vertex_PCULayout.hpp"
#include "../VertexLayoutRegistry.hpp"

namespace enigma::graphic
{
    // ============================================================================
    // Static Element Array Definition
    // ============================================================================

    const D3D12_INPUT_ELEMENT_DESC Vertex_PCULayout::s_elements[3] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    // ============================================================================
    // Constructor
    // ============================================================================

    Vertex_PCULayout::Vertex_PCULayout()
        : VertexLayout("Vertex_PCU", 24)
    {
        CalculateHash(s_elements, 3);
    }

    // ============================================================================
    // VertexLayout Interface
    // ============================================================================

    const D3D12_INPUT_ELEMENT_DESC* Vertex_PCULayout::GetInputElements() const
    {
        return s_elements;
    }

    uint32_t Vertex_PCULayout::GetInputElementCount() const
    {
        return 3;
    }

    // ============================================================================
    // Static Type-Safe Access
    // ============================================================================

    const Vertex_PCULayout* Vertex_PCULayout::Get()
    {
        return static_cast<const Vertex_PCULayout*>(VertexLayoutRegistry::GetLayout("Vertex_PCU"));
    }
} // namespace enigma::graphic
