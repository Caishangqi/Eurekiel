#include "RenderMesh.hpp"

#include "Engine/Core/EngineCommon.hpp"
using namespace enigma::renderer::model;

void RenderMesh::AddFace(const RenderFace& face)
{
    faces.push_back(face);
    gpuDataValid = false;
}

size_t RenderMesh::GetVertexCount() const
{
    size_t count = 0;
    for (const auto& face : faces)
    {
        count += face.vertices.size();
    }
    return count;
}

size_t RenderMesh::GetIndexCount() const
{
    size_t count = 0;
    for (const auto& face : faces)
    {
        count += face.indices.size();
    }
    return count;
}

const RenderFace* RenderMesh::GetFace(Direction direction) const
{
    for (const auto& face : faces)
    {
        if (face.cullDirection == direction)
        {
            return &face;
        }
    }
    return nullptr;
}

void RenderMesh::CompileToGPU() const
{
}

std::shared_ptr<VertexBuffer> RenderMesh::GetVertexBuffer() const
{
    if (!gpuDataValid)
    {
        CompileToGPU();
    }
    return vertexBuffer;
}

std::shared_ptr<IndexBuffer> RenderMesh::GetIndexBuffer() const
{
    if (!gpuDataValid)
    {
        CompileToGPU();
    }
    return indexBuffer;
}

void RenderMesh::InvalidateGPUData()
{
    gpuDataValid = false;
}

void RenderMesh::TransformAndAppendTo(voxel::chunk::ChunkMesh* chunk_mesh, const Vec3& vec3)
{
    UNUSED(chunk_mesh)
    UNUSED(vec3)
}

void RenderMesh::Clear()
{
    faces.clear();
    gpuDataValid = false;
}
