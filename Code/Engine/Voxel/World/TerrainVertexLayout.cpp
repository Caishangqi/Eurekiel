#include "TerrainVertexLayout.hpp"
#include "../../Graphic/Resource/VertexLayout/VertexLayoutRegistry.hpp"

namespace enigma::graphic
{
    // ============================================================================
    // Static Element Array Definition
    // 
    // Layout (56 bytes):
    // - POSITION:  float3 (12 bytes, offset 0)
    // - COLOR:     rgba8  (4 bytes, offset 12)
    // - TEXCOORD0: float2 (8 bytes, offset 16) - UV coordinates
    // - NORMAL:    float3 (12 bytes, offset 24)
    // - LIGHTMAP:  float2 (8 bytes, offset 36) - Lightmap coordinates
    // - TEXCOORD2: uint16 (2 bytes, offset 44) - Block entity ID (mc_Entity)
    // - PADDING:   2 bytes (offset 46) - Alignment padding
    // - TEXCOORD3: float2 (8 bytes, offset 48) - Texture center (mc_midTexCoord)
    // ============================================================================

    const D3D12_INPUT_ELEMENT_DESC TerrainVertexLayout::s_elements[7] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"LIGHTMAP", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R16_UINT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    // ============================================================================
    // Constructor
    // ============================================================================

    TerrainVertexLayout::TerrainVertexLayout()
        : VertexLayout("Terrain", 56) // 56 bytes: pos(12) + color(4) + uv(8) + normal(12) + lightmap(8) + entityId(2) + padding(2) + midTexCoord(8)
    {
        CalculateHash(s_elements, 7);
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
        return 7;
    }

    // ============================================================================
    // Static Type-Safe Access
    // ============================================================================

    const TerrainVertexLayout* TerrainVertexLayout::Get()
    {
        return static_cast<const TerrainVertexLayout*>(VertexLayoutRegistry::GetLayout("Terrain"));
    }
} // namespace enigma::graphic
