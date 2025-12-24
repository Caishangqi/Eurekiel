#include "TerrainVertexLayout.hpp"
#include "../VertexLayoutRegistry.hpp"

namespace enigma::graphic
{
    // ============================================================================
    // Static Element Array Definition
    // [PLACEHOLDER] Uses same format as Vertex_PCUTBN (can be customized later)
    // ============================================================================

    const D3D12_INPUT_ELEMENT_DESC TerrainVertexLayout::s_elements[6] = {
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

    TerrainVertexLayout::TerrainVertexLayout()
        : VertexLayout("Terrain", 60)
    {
        CalculateHash(s_elements, 6);
    }

    // ============================================================================
    // VertexLayout Interface
    // ============================================================================

    const D3D12_INPUT_ELEMENT_DESC* TerrainVertexLayout::GetInputElements() const
    {
        return s_elements;
    }

    uint32_t TerrainVertexLayout::GetInputElementCount() const
    {
        return 6;
    }

    // ============================================================================
    // Static Type-Safe Access
    // ============================================================================

    const TerrainVertexLayout* TerrainVertexLayout::Get()
    {
        return static_cast<const TerrainVertexLayout*>(VertexLayoutRegistry::GetLayout("Terrain"));
    }
} // namespace enigma::graphic
