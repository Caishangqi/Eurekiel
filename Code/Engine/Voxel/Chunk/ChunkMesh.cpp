#include "ChunkMesh.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
using namespace enigma::voxel;

void ChunkMesh::Clear()
{
    m_opaqueTerrainVertices.clear();
    m_cutoutTerrainVertices.clear();
    m_translucentTerrainVertices.clear();
    m_opaqueIndices.clear();
    m_cutoutIndices.clear();
    m_translucentIndices.clear();
    ReleaseGpuBuffers();
}

void ChunkMesh::Reserve(size_t opaqueQuads, size_t cutoutQuads, size_t translucentQuads)
{
    // Each quad has 4 vertices and 6 indices (2 triangles)
    m_opaqueTerrainVertices.reserve(opaqueQuads * 4);
    m_opaqueIndices.reserve(opaqueQuads * 6);
    m_cutoutTerrainVertices.reserve(cutoutQuads * 4);
    m_cutoutIndices.reserve(cutoutQuads * 6);
    m_translucentTerrainVertices.reserve(translucentQuads * 4);
    m_translucentIndices.reserve(translucentQuads * 6);
}

bool ChunkMesh::IsEmpty() const
{
    return m_opaqueTerrainVertices.empty() &&
        m_cutoutTerrainVertices.empty() &&
        m_translucentTerrainVertices.empty();
}

// ============================================================
// Opaque Statistics
// ============================================================

bool ChunkMesh::HasOpaqueGeometry() const
{
    return !m_opaqueTerrainVertices.empty();
}

size_t ChunkMesh::GetOpaqueVertexCount() const
{
    return m_opaqueTerrainVertices.size();
}

size_t ChunkMesh::GetOpaqueIndexCount() const
{
    return m_opaqueIndices.size();
}

size_t ChunkMesh::GetOpaqueTriangleCount() const
{
    return m_opaqueIndices.size() / 3;
}

// ============================================================
// Cutout Statistics
// ============================================================

bool ChunkMesh::HasCutoutGeometry() const
{
    return !m_cutoutTerrainVertices.empty();
}

size_t ChunkMesh::GetCutoutVertexCount() const
{
    return m_cutoutTerrainVertices.size();
}

size_t ChunkMesh::GetCutoutIndexCount() const
{
    return m_cutoutIndices.size();
}

size_t ChunkMesh::GetCutoutTriangleCount() const
{
    return m_cutoutIndices.size() / 3;
}

// ============================================================
// Translucent Statistics
// ============================================================

bool ChunkMesh::HasTranslucentGeometry() const
{
    return !m_translucentTerrainVertices.empty();
}

size_t ChunkMesh::GetTranslucentVertexCount() const
{
    return m_translucentTerrainVertices.size();
}

size_t ChunkMesh::GetTranslucentIndexCount() const
{
    return m_translucentIndices.size();
}

size_t ChunkMesh::GetTranslucentTriangleCount() const
{
    return m_translucentIndices.size() / 3;
}

// ============================================================
// [FLIPPED QUADS] Adaptive Quad Triangulation for Smooth AO
// ============================================================
// [SODIUM REF] ModelQuadOrientation.java - orientByBrightness() method
// File: net/caffeinemc/mods/sodium/client/model/quad/properties/ModelQuadOrientation.java
//
// When flipQuad is true, we change the triangulation from:
//   NORMAL: (0,1,2) + (0,2,3) - split along 0-2 diagonal
//   FLIP:   (0,1,3) + (1,2,3) - split along 1-3 diagonal
//
// This eliminates the "diagonal crease" artifact when AO values are anisotropic.
// ============================================================

void ChunkMesh::AddOpaqueTerrainQuad(const std::array<graphic::TerrainVertex, 4>& vertices, bool flipQuad)
{
    uint32_t baseIndex = static_cast<uint32_t>(m_opaqueTerrainVertices.size());

    // Add vertices
    for (const auto& vertex : vertices)
    {
        m_opaqueTerrainVertices.push_back(vertex);
    }

    // Add indices for two triangles (quad) with adaptive triangulation
    // [SODIUM REF] ModelQuadOrientation indices remapping
    if (flipQuad)
    {
        // FLIP: triangles (0,1,3) and (1,2,3) - split along 1-3 diagonal
        m_opaqueIndices.push_back(baseIndex + 0);
        m_opaqueIndices.push_back(baseIndex + 1);
        m_opaqueIndices.push_back(baseIndex + 3);
        m_opaqueIndices.push_back(baseIndex + 1);
        m_opaqueIndices.push_back(baseIndex + 2);
        m_opaqueIndices.push_back(baseIndex + 3);
    }
    else
    {
        // NORMAL: triangles (0,1,2) and (0,2,3) - split along 0-2 diagonal
        m_opaqueIndices.push_back(baseIndex + 0);
        m_opaqueIndices.push_back(baseIndex + 1);
        m_opaqueIndices.push_back(baseIndex + 2);
        m_opaqueIndices.push_back(baseIndex + 0);
        m_opaqueIndices.push_back(baseIndex + 2);
        m_opaqueIndices.push_back(baseIndex + 3);
    }

    InvalidateGPUData();
}

void ChunkMesh::AddCutoutTerrainQuad(const std::array<graphic::TerrainVertex, 4>& vertices, bool flipQuad)
{
    uint32_t baseIndex = static_cast<uint32_t>(m_cutoutTerrainVertices.size());

    // Add vertices
    for (const auto& vertex : vertices)
    {
        m_cutoutTerrainVertices.push_back(vertex);
    }

    // Add indices for two triangles (quad) with adaptive triangulation
    if (flipQuad)
    {
        // FLIP: triangles (0,1,3) and (1,2,3) - split along 1-3 diagonal
        m_cutoutIndices.push_back(baseIndex + 0);
        m_cutoutIndices.push_back(baseIndex + 1);
        m_cutoutIndices.push_back(baseIndex + 3);
        m_cutoutIndices.push_back(baseIndex + 1);
        m_cutoutIndices.push_back(baseIndex + 2);
        m_cutoutIndices.push_back(baseIndex + 3);
    }
    else
    {
        // NORMAL: triangles (0,1,2) and (0,2,3) - split along 0-2 diagonal
        m_cutoutIndices.push_back(baseIndex + 0);
        m_cutoutIndices.push_back(baseIndex + 1);
        m_cutoutIndices.push_back(baseIndex + 2);
        m_cutoutIndices.push_back(baseIndex + 0);
        m_cutoutIndices.push_back(baseIndex + 2);
        m_cutoutIndices.push_back(baseIndex + 3);
    }

    InvalidateGPUData();
}

void ChunkMesh::AddTranslucentTerrainQuad(const std::array<graphic::TerrainVertex, 4>& vertices, bool flipQuad)
{
    uint32_t baseIndex = static_cast<uint32_t>(m_translucentTerrainVertices.size());

    // Add vertices
    for (const auto& vertex : vertices)
    {
        m_translucentTerrainVertices.push_back(vertex);
    }

    // Add indices for two triangles (quad) with adaptive triangulation
    if (flipQuad)
    {
        // FLIP: triangles (0,1,3) and (1,2,3) - split along 1-3 diagonal
        m_translucentIndices.push_back(baseIndex + 0);
        m_translucentIndices.push_back(baseIndex + 1);
        m_translucentIndices.push_back(baseIndex + 3);
        m_translucentIndices.push_back(baseIndex + 1);
        m_translucentIndices.push_back(baseIndex + 2);
        m_translucentIndices.push_back(baseIndex + 3);
    }
    else
    {
        // NORMAL: triangles (0,1,2) and (0,2,3) - split along 0-2 diagonal
        m_translucentIndices.push_back(baseIndex + 0);
        m_translucentIndices.push_back(baseIndex + 1);
        m_translucentIndices.push_back(baseIndex + 2);
        m_translucentIndices.push_back(baseIndex + 0);
        m_translucentIndices.push_back(baseIndex + 2);
        m_translucentIndices.push_back(baseIndex + 3);
    }

    InvalidateGPUData();
}

// ============================================================
// [WATER BACKFACE] Generate backface for water surface (underwater view)
// ============================================================
// [SODIUM REF] DefaultFluidRenderer.java line 289:
//   int vertexIndex = flip ? (3 - i + 1) & 3 : i;
// This flips vertex order: 0->0, 1->3, 2->2, 3->1
//
// For our implementation, we flip the vertex order in the array itself:
//   Original: {v0, v1, v2, v3} with indices (0,1,2), (0,2,3)
//   Backface: {v0, v3, v2, v1} with same indices -> reversed winding
// ============================================================
void ChunkMesh::AddTranslucentTerrainQuadBackface(const std::array<graphic::TerrainVertex, 4>& vertices, bool flipQuad)
{
    uint32_t baseIndex = static_cast<uint32_t>(m_translucentTerrainVertices.size());

    // [SODIUM REF] Flip vertex order to reverse winding: {0,1,2,3} -> {0,3,2,1}
    // This changes the triangle winding from CCW to CW (or vice versa)
    // allowing the backface to pass culling when viewed from behind
    m_translucentTerrainVertices.push_back(vertices[0]);
    m_translucentTerrainVertices.push_back(vertices[3]);
    m_translucentTerrainVertices.push_back(vertices[2]);
    m_translucentTerrainVertices.push_back(vertices[1]);

    // Add indices for two triangles (quad) with adaptive triangulation
    // Note: We use the same index pattern, but vertices are already reordered
    if (flipQuad)
    {
        // FLIP: triangles (0,1,3) and (1,2,3) - split along 1-3 diagonal
        m_translucentIndices.push_back(baseIndex + 0);
        m_translucentIndices.push_back(baseIndex + 1);
        m_translucentIndices.push_back(baseIndex + 3);
        m_translucentIndices.push_back(baseIndex + 1);
        m_translucentIndices.push_back(baseIndex + 2);
        m_translucentIndices.push_back(baseIndex + 3);
    }
    else
    {
        // NORMAL: triangles (0,1,2) and (0,2,3) - split along 0-2 diagonal
        m_translucentIndices.push_back(baseIndex + 0);
        m_translucentIndices.push_back(baseIndex + 1);
        m_translucentIndices.push_back(baseIndex + 2);
        m_translucentIndices.push_back(baseIndex + 0);
        m_translucentIndices.push_back(baseIndex + 2);
        m_translucentIndices.push_back(baseIndex + 3);
    }

    InvalidateGPUData();
}

// ============================================================
// GPU Buffer Management
// ============================================================

void ChunkMesh::InvalidateGPUData()
{
    m_opaqueGpuDataValid = false;
    m_cutoutGpuDataValid = false;
    m_translucentGpuDataValid = false;
}

void ChunkMesh::ReleaseGpuBuffers(bool releaseOpaque, bool releaseCutout, bool releaseTranslucent)
{
    if (releaseOpaque)
    {
        m_d12OpaqueVertexBuffer.reset();
        m_d12OpaqueIndexBuffer.reset();
        m_opaqueGpuDataValid = false;
    }

    if (releaseCutout)
    {
        m_d12CutoutVertexBuffer.reset();
        m_d12CutoutIndexBuffer.reset();
        m_cutoutGpuDataValid = false;
    }

    if (releaseTranslucent)
    {
        m_d12TranslucentVertexBuffer.reset();
        m_d12TranslucentIndexBuffer.reset();
        m_translucentGpuDataValid = false;
    }
}

void ChunkMesh::CompileToGPU(bool compileOpaque, bool compileCutout, bool compileTranslucent)
{
    const bool needsOpaqueUpload = compileOpaque &&
        HasOpaqueGeometry() &&
        (!m_opaqueGpuDataValid || !m_d12OpaqueVertexBuffer || !m_d12OpaqueIndexBuffer);
    if (needsOpaqueUpload)
    {
        size_t opaqueVertexDataSize = sizeof(graphic::TerrainVertex) * m_opaqueTerrainVertices.size();
        size_t opaqueIndexDataSize  = sizeof(uint32_t) * m_opaqueIndices.size();
        m_d12OpaqueVertexBuffer     = graphic::D3D12RenderSystem::CreateVertexBuffer(opaqueVertexDataSize, sizeof(graphic::TerrainVertex), m_opaqueTerrainVertices.data());
        m_d12OpaqueIndexBuffer      = graphic::D3D12RenderSystem::CreateIndexBuffer(opaqueIndexDataSize, m_opaqueIndices.data());
        m_opaqueGpuDataValid        = m_d12OpaqueVertexBuffer != nullptr && m_d12OpaqueIndexBuffer != nullptr;
    }
    else if (compileOpaque && !HasOpaqueGeometry())
    {
        m_d12OpaqueVertexBuffer.reset();
        m_d12OpaqueIndexBuffer.reset();
        m_opaqueGpuDataValid = false;
    }

    const bool needsCutoutUpload = compileCutout &&
        HasCutoutGeometry() &&
        (!m_cutoutGpuDataValid || !m_d12CutoutVertexBuffer || !m_d12CutoutIndexBuffer);
    if (needsCutoutUpload)
    {
        size_t cutoutVertexDataSize = sizeof(graphic::TerrainVertex) * m_cutoutTerrainVertices.size();
        size_t cutoutIndexDataSize  = sizeof(uint32_t) * m_cutoutIndices.size();
        m_d12CutoutVertexBuffer     = graphic::D3D12RenderSystem::CreateVertexBuffer(cutoutVertexDataSize, sizeof(graphic::TerrainVertex), m_cutoutTerrainVertices.data());
        m_d12CutoutIndexBuffer      = graphic::D3D12RenderSystem::CreateIndexBuffer(cutoutIndexDataSize, m_cutoutIndices.data());
        m_cutoutGpuDataValid        = m_d12CutoutVertexBuffer != nullptr && m_d12CutoutIndexBuffer != nullptr;
    }
    else if (compileCutout && !HasCutoutGeometry())
    {
        m_d12CutoutVertexBuffer.reset();
        m_d12CutoutIndexBuffer.reset();
        m_cutoutGpuDataValid = false;
    }

    const bool needsTranslucentUpload = compileTranslucent &&
        HasTranslucentGeometry() &&
        (!m_translucentGpuDataValid || !m_d12TranslucentVertexBuffer || !m_d12TranslucentIndexBuffer);
    if (needsTranslucentUpload)
    {
        size_t translucentVertexDataSize = sizeof(graphic::TerrainVertex) * m_translucentTerrainVertices.size();
        size_t translucentIndexDataSize  = sizeof(uint32_t) * m_translucentIndices.size();
        m_d12TranslucentVertexBuffer     = graphic::D3D12RenderSystem::CreateVertexBuffer(translucentVertexDataSize, sizeof(graphic::TerrainVertex), m_translucentTerrainVertices.data());
        m_d12TranslucentIndexBuffer      = graphic::D3D12RenderSystem::CreateIndexBuffer(translucentIndexDataSize, m_translucentIndices.data());
        m_translucentGpuDataValid        = m_d12TranslucentVertexBuffer != nullptr && m_d12TranslucentIndexBuffer != nullptr;
    }
    else if (compileTranslucent && !HasTranslucentGeometry())
    {
        m_d12TranslucentVertexBuffer.reset();
        m_d12TranslucentIndexBuffer.reset();
        m_translucentGpuDataValid = false;
    }
}
