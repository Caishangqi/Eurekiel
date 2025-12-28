#include "TerrainVertexLayout.hpp"
#include "../../Graphic/Resource/VertexLayout/VertexLayoutRegistry.hpp"

namespace enigma::graphic
{
    // ============================================================================
    // Static Element Array Definition
    // 
    // Layout (44 bytes):
    // - POSITION:  float3 (12 bytes, offset 0)
    // - COLOR:     rgba8  (4 bytes, offset 12)
    // - TEXCOORD0: float2 (8 bytes, offset 16) - UV coordinates
    // - NORMAL:    float3 (12 bytes, offset 24)
    // - TEXCOORD1: float2 (8 bytes, offset 36) - Lightmap coordinates
    // ============================================================================

    const D3D12_INPUT_ELEMENT_DESC TerrainVertexLayout::s_elements[5] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    // ============================================================================
    // Constructor
    // ============================================================================

    TerrainVertexLayout::TerrainVertexLayout()
        : VertexLayout("Terrain", 44) // 44 bytes: pos(12) + color(4) + uv(8) + normal(12) + lightmap(8)
    {
        CalculateHash(s_elements, 5);
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
        return 5;
    }

    // ============================================================================
    // Static Type-Safe Access
    // ============================================================================

    const TerrainVertexLayout* TerrainVertexLayout::Get()
    {
        return static_cast<const TerrainVertexLayout*>(VertexLayoutRegistry::GetLayout("Terrain"));
    }
} // namespace enigma::graphic
