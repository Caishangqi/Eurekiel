#include "ChunkMesh.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp"
#include "Game/GameCommon.hpp"

using namespace enigma::voxel;

void ChunkMesh::Clear()
{
    m_opaqueTerrainVertices.clear();
    m_cutoutTerrainVertices.clear();
    m_translucentTerrainVertices.clear();
    m_opaqueIndices.clear();
    m_cutoutIndices.clear();
    m_translucentIndices.clear();
    InvalidateGPUData();
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
// GPU Buffer Management
// ============================================================

void ChunkMesh::InvalidateGPUData()
{
    m_gpuDataValid = false;
}

void ChunkMesh::CompileToGPU()
{
    if (m_gpuDataValid) return;

    // Create opaque geometry buffers
    if (HasOpaqueGeometry())
    {
        size_t opaqueVertexDataSize = sizeof(TerrainVertex) * m_opaqueTerrainVertices.size();
        size_t opaqueIndexDataSize  = sizeof(uint32_t) * m_opaqueIndices.size();
        m_d12OpaqueVertexBuffer     = D3D12RenderSystem::CreateVertexBuffer(opaqueVertexDataSize, sizeof(TerrainVertex), m_opaqueTerrainVertices.data());
        m_d12OpaqueIndexBuffer      = D3D12RenderSystem::CreateIndexBuffer(opaqueIndexDataSize, m_opaqueIndices.data());
    }

    // Create cutout geometry buffers (alpha test, no sorting needed)
    if (HasCutoutGeometry())
    {
        size_t cutoutVertexDataSize = sizeof(TerrainVertex) * m_cutoutTerrainVertices.size();
        size_t cutoutIndexDataSize  = sizeof(uint32_t) * m_cutoutIndices.size();
        m_d12CutoutVertexBuffer     = D3D12RenderSystem::CreateVertexBuffer(cutoutVertexDataSize, sizeof(TerrainVertex), m_cutoutTerrainVertices.data());
        m_d12CutoutIndexBuffer      = D3D12RenderSystem::CreateIndexBuffer(cutoutIndexDataSize, m_cutoutIndices.data());
    }

    // Create translucent geometry buffers (alpha blend, requires depth sorting)
    if (HasTranslucentGeometry())
    {
        size_t translucentVertexDataSize = sizeof(TerrainVertex) * m_translucentTerrainVertices.size();
        size_t translucentIndexDataSize  = sizeof(uint32_t) * m_translucentIndices.size();
        m_d12TranslucentVertexBuffer     = D3D12RenderSystem::CreateVertexBuffer(translucentVertexDataSize, sizeof(TerrainVertex), m_translucentTerrainVertices.data());
        m_d12TranslucentIndexBuffer      = D3D12RenderSystem::CreateIndexBuffer(translucentIndexDataSize, m_translucentIndices.data());
    }

    m_gpuDataValid = true;
}
