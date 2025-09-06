#include "ChunkMesh.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <array>

#include "Engine/Renderer/IRenderer.hpp"
#include "Game/GameCommon.hpp"

using namespace enigma::voxel::chunk;

void ChunkMesh::Clear()
{
    m_opaqueVertices.clear();
    m_opaqueIndices.clear();
    m_transparentVertices.clear();
    m_transparentIndices.clear();
    InvalidateGPUData();
}

void ChunkMesh::Reserve(size_t opaqueQuads, size_t transparentQuads)
{
    // Each quad has 4 vertices and 6 indices (2 triangles)
    m_opaqueVertices.reserve(opaqueQuads * 4);
    m_opaqueIndices.reserve(opaqueQuads * 6);
    m_transparentVertices.reserve(transparentQuads * 4);
    m_transparentIndices.reserve(transparentQuads * 6);
}

bool ChunkMesh::IsEmpty() const
{
    return m_opaqueVertices.empty() && m_transparentVertices.empty();
}

bool ChunkMesh::HasOpaqueGeometry() const
{
    return !m_opaqueVertices.empty();
}

bool ChunkMesh::HasTransparentGeometry() const
{
    return !m_transparentVertices.empty();
}

size_t ChunkMesh::GetOpaqueVertexCount() const
{
    return m_opaqueVertices.size();
}

size_t ChunkMesh::GetOpaqueTriangleCount() const
{
    return m_opaqueIndices.size() / 3;
}

size_t ChunkMesh::GetTransparentVertexCount() const
{
    return m_transparentVertices.size();
}

size_t ChunkMesh::GetTransparentTriangleCount() const
{
    return m_transparentIndices.size() / 3;
}

void ChunkMesh::AddOpaqueQuad(const std::array<Vertex_PCU, 4>& vertices)
{
    uint32_t baseIndex = static_cast<uint32_t>(m_opaqueVertices.size());

    // Add vertices
    for (const auto& vertex : vertices)
    {
        m_opaqueVertices.push_back(vertex);
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

void ChunkMesh::AddTransparentQuad(const std::array<Vertex_PCU, 4>& vertices)
{
    uint32_t baseIndex = static_cast<uint32_t>(m_transparentVertices.size());

    // Add vertices
    for (const auto& vertex : vertices)
    {
        m_transparentVertices.push_back(vertex);
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

    // Create opaque geometry buffers if we have opaque data
    if (HasOpaqueGeometry())
    {
        // Create vertex buffer
        size_t opaqueVertexDataSize = sizeof(Vertex_PCU) * m_opaqueVertices.size();
        m_opaqueVertexBuffer        = std::shared_ptr<VertexBuffer>(
            g_theRenderer->CreateVertexBuffer(opaqueVertexDataSize, sizeof(Vertex_PCU))
        );

        // Upload vertex data
        g_theRenderer->CopyCPUToGPU(m_opaqueVertices.data(), opaqueVertexDataSize, m_opaqueVertexBuffer.get());

        // Create index buffer
        size_t opaqueIndexDataSize = sizeof(unsigned int) * m_opaqueIndices.size();
        m_opaqueIndexBuffer        = std::shared_ptr<IndexBuffer>(
            g_theRenderer->CreateIndexBuffer(opaqueIndexDataSize)
        );

        // Upload index data
        g_theRenderer->CopyCPUToGPU(m_opaqueIndices.data(), opaqueIndexDataSize, m_opaqueIndexBuffer.get());
    }

    // Create transparent geometry buffers if we have transparent data
    if (HasTransparentGeometry())
    {
        // Create vertex buffer
        size_t transparentVertexDataSize = sizeof(Vertex_PCU) * m_transparentVertices.size();
        m_transparentVertexBuffer        = std::shared_ptr<VertexBuffer>(
            g_theRenderer->CreateVertexBuffer(transparentVertexDataSize, sizeof(Vertex_PCU))
        );

        // Upload vertex data
        g_theRenderer->CopyCPUToGPU(m_transparentVertices.data(), transparentVertexDataSize, m_transparentVertexBuffer.get());

        // Create index buffer
        size_t transparentIndexDataSize = sizeof(unsigned int) * m_transparentIndices.size();
        m_transparentIndexBuffer        = std::shared_ptr<IndexBuffer>(
            g_theRenderer->CreateIndexBuffer(transparentIndexDataSize)
        );

        // Upload index data
        g_theRenderer->CopyCPUToGPU(m_transparentIndices.data(), transparentIndexDataSize, m_transparentIndexBuffer.get());
    }

    m_gpuDataValid = true;
}

void ChunkMesh::RenderOpaque(IRenderer* renderer)
{
    if (!HasOpaqueGeometry()) return;

    // Ensure GPU data is compiled
    if (!m_gpuDataValid)
    {
        CompileToGPU();
    }

    // Render using DrawVertexIndexed as specified by Assignment01
    if (m_opaqueVertexBuffer && m_opaqueIndexBuffer)
    {
        renderer->DrawVertexIndexed(
            m_opaqueVertexBuffer.get(),
            m_opaqueIndexBuffer.get(),
            static_cast<unsigned>(m_opaqueIndices.size())
        );
    }
}

void ChunkMesh::RenderTransparent(IRenderer* renderer)
{
    if (!HasTransparentGeometry()) return;

    // Ensure GPU data is compiled
    if (!m_gpuDataValid)
    {
        CompileToGPU();
    }

    // Render using DrawVertexIndexed
    if (m_transparentVertexBuffer && m_transparentIndexBuffer)
    {
        renderer->DrawVertexIndexed(
            m_transparentVertexBuffer.get(),
            m_transparentIndexBuffer.get(),
            static_cast<unsigned>(m_transparentIndices.size())
        );
    }
}

void ChunkMesh::RenderAll(IRenderer* renderer)
{
    if (HasOpaqueGeometry())
    {
        RenderOpaque(renderer);
    }

    if (HasTransparentGeometry())
    {
        RenderTransparent(renderer);
    }
}

std::shared_ptr<VertexBuffer> ChunkMesh::GetOpaqueVertexBuffer()
{
    if (!m_gpuDataValid)
    {
        CompileToGPU();
    }
    return m_opaqueVertexBuffer;
}

std::shared_ptr<IndexBuffer> ChunkMesh::GetOpaqueIndexBuffer()
{
    if (!m_gpuDataValid)
    {
        CompileToGPU();
    }
    return m_opaqueIndexBuffer;
}

std::shared_ptr<VertexBuffer> ChunkMesh::GetTransparentVertexBuffer()
{
    if (!m_gpuDataValid)
    {
        CompileToGPU();
    }
    return m_transparentVertexBuffer;
}

std::shared_ptr<IndexBuffer> ChunkMesh::GetTransparentIndexBuffer()
{
    if (!m_gpuDataValid)
    {
        CompileToGPU();
    }
    return m_transparentIndexBuffer;
}
