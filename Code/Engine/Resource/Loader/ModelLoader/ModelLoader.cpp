#include "ModelLoader.hpp"

#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Renderer/IRenderer.hpp"

bool FMaterial::HasTexture(EMaterialChannel channel) const
{
    auto it = textures.find(channel);
    return it != textures.end() && it->second != nullptr;
}
Texture* FMaterial::GetTexture(EMaterialChannel channel) const
{
    auto it = textures.find(channel);
    return (it != textures.end()) ? it->second.get() : nullptr;
}


void FMaterial::SetTexture(EMaterialChannel channel, std::unique_ptr<Texture> texture)
{
    textures[channel] = std::move(texture);
}

int FMaterial::GetTextureCoordSet(EMaterialChannel channel) const
{
    auto it = textureCoordSets.find(channel);
    return (it != textureCoordSets.end()) ? it->second : 0;
}

FMesh::FMesh()
{
}

FMesh::~FMesh()
{
}

void FMesh::EnsureGPUBuffers(IRenderer* renderer) const
{
    if (!vertexBuffer && !m_vertices.empty())
    {
        vertexBuffer = std::shared_ptr<VertexBuffer>(renderer->CreateVertexBuffer(sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN)));
        vertexBuffer->Resize(static_cast<int>(m_vertices.size() * sizeof(Vertex_PCUTBN)));
        renderer->CopyCPUToGPU(m_vertices.data(), static_cast<int>(m_vertices.size() * sizeof(Vertex_PCUTBN)), vertexBuffer.get());
    }

    if (!indexBuffer && !m_indices.empty())
    {
        indexBuffer = std::shared_ptr<IndexBuffer>(renderer->CreateIndexBuffer(sizeof(unsigned int)));
        indexBuffer->Resize(static_cast<int>(m_indices.size() * sizeof(unsigned int)));
        renderer->CopyCPUToGPU(m_indices.data(), static_cast<int>(m_indices.size() * sizeof(unsigned int)), indexBuffer.get());
    }
}

ModelLoader::ModelLoader(IRenderer* renderer) : m_renderer(renderer)
{
}
