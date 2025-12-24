#include "Vertex_PCUTBNLayout.hpp"
#include "../VertexLayoutRegistry.hpp"

namespace enigma::graphic
{
    // ============================================================================
    // Static Element Array Definition
    // [COPIED FROM] PSOManager.cpp:210-217 (exact match)
    // ============================================================================

    const D3D12_INPUT_ELEMENT_DESC Vertex_PCUTBNLayout::s_elements[6] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    // ============================================================================
    // Constructor
    // ============================================================================

    Vertex_PCUTBNLayout::Vertex_PCUTBNLayout()
        : VertexLayout("Vertex_PCUTBN", 60)
    {
        CalculateHash(s_elements, 6);
    }

    // ============================================================================
    // VertexLayout Interface
    // ============================================================================

    const D3D12_INPUT_ELEMENT_DESC* Vertex_PCUTBNLayout::GetInputElements() const
    {
        return s_elements;
    }

    uint32_t Vertex_PCUTBNLayout::GetInputElementCount() const
    {
        return 6;
    }

    // ============================================================================
    // Static Type-Safe Access
    // ============================================================================

    const Vertex_PCUTBNLayout* Vertex_PCUTBNLayout::Get()
    {
        return static_cast<const Vertex_PCUTBNLayout*>(VertexLayoutRegistry::GetLayout("Vertex_PCUTBN"));
    }
} // namespace enigma::graphic
