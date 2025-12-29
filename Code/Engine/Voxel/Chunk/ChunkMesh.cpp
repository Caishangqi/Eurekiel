#include "ChunkMesh.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp"
#include "Game/GameCommon.hpp"

using namespace enigma::voxel;

void ChunkMesh::Clear()
{
    m_opaqueTerrainVertices.clear();
    m_transparentTerrainVertices.clear();
    m_opaqueIndices.clear();
    m_transparentIndices.clear();
    InvalidateGPUData();
}

void ChunkMesh::Reserve(size_t opaqueQuads, size_t transparentQuads)
{
    // Each quad has 4 vertices and 6 indices (2 triangles)
    m_opaqueTerrainVertices.reserve(opaqueQuads * 4);
    m_opaqueIndices.reserve(opaqueQuads * 6);
    m_transparentTerrainVertices.reserve(transparentQuads * 4);
    m_transparentIndices.reserve(transparentQuads * 6);
}

bool ChunkMesh::IsEmpty() const
{
    return m_opaqueTerrainVertices.empty() && m_transparentTerrainVertices.empty();
}

bool ChunkMesh::HasOpaqueGeometry() const
{
    return !m_opaqueTerrainVertices.empty();
}

bool ChunkMesh::HasTransparentGeometry() const
{
    return !m_transparentTerrainVertices.empty();
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

size_t ChunkMesh::GetTransparentVertexCount() const
{
    return m_transparentTerrainVertices.size();
}

size_t ChunkMesh::GetTransparentIndexCount() const
{
    return m_transparentIndices.size();
}

size_t ChunkMesh::GetTransparentTriangleCount() const
{
    return m_transparentIndices.size() / 3;
}

void ChunkMesh::AddOpaqueTerrainQuad(const std::array<graphic::TerrainVertex, 4>& vertices)
{
    uint32_t baseIndex = static_cast<uint32_t>(m_opaqueTerrainVertices.size());

    // Add vertices
    for (const auto& vertex : vertices)
    {
        m_opaqueTerrainVertices.push_back(vertex);
    }

    // Add indices for two triangles (quad)
    m_opaqueIndices.push_back(baseIndex + 0);
    m_opaqueIndices.push_back(baseIndex + 1);
    m_opaqueIndices.push_back(baseIndex + 2);
    m_opaqueIndices.push_back(baseIndex + 0);
    m_opaqueIndices.push_back(baseIndex + 2);
    m_opaqueIndices.push_back(baseIndex + 3);

    InvalidateGPUData();
}

void ChunkMesh::AddTransparentTerrainQuad(const std::array<graphic::TerrainVertex, 4>& vertices)
{
    uint32_t baseIndex = static_cast<uint32_t>(m_transparentTerrainVertices.size());

    // Add vertices
    for (const auto& vertex : vertices)
    {
        m_transparentTerrainVertices.push_back(vertex);
    }

    // Add indices for two triangles (quad)
    m_transparentIndices.push_back(baseIndex + 0);
    m_transparentIndices.push_back(baseIndex + 1);
    m_transparentIndices.push_back(baseIndex + 2);
    m_transparentIndices.push_back(baseIndex + 0);
    m_transparentIndices.push_back(baseIndex + 2);
    m_transparentIndices.push_back(baseIndex + 3);

    InvalidateGPUData();
}

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

    // Create transparent geometry buffers
    if (HasTransparentGeometry())
    {
        size_t transparentVertexDataSize = sizeof(TerrainVertex) * m_transparentTerrainVertices.size();
        size_t transparentIndexDataSize  = sizeof(uint32_t) * m_transparentIndices.size();
        m_d12TransparentVertexBuffer     = D3D12RenderSystem::CreateVertexBuffer(transparentVertexDataSize, sizeof(TerrainVertex), m_transparentTerrainVertices.data());
        m_d12TransparentIndexBuffer      = D3D12RenderSystem::CreateIndexBuffer(transparentIndexDataSize, m_transparentIndices.data());
    }

    m_gpuDataValid = true;
}
