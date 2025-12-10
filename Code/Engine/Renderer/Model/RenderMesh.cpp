#include "RenderMesh.hpp"
#include "../../Math/Mat44.hpp"
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

std::vector<const RenderFace*> RenderMesh::GetFaces(Direction direction) const
{
    std::vector<const RenderFace*> result;
    for (const auto& face : faces)
    {
        if (face.cullDirection == direction)
        {
            result.push_back(&face);
        }
    }
    return result;
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

void RenderMesh::TransformAndAppendTo(voxel::ChunkMesh* chunk_mesh, const Vec3& vec3)
{
    UNUSED(chunk_mesh)
    UNUSED(vec3)
}

void RenderMesh::Clear()
{
    faces.clear();
    gpuDataValid = false;
}

void RenderMesh::ApplyBlockRotation(int rotX, int rotY)
{
    // Skip if no rotation needed
    if (rotX == 0 && rotY == 0)
    {
        return;
    }

    // Block center for rotation pivot (model space is 0-1)
    constexpr float CENTER = 0.5f;
    const Vec3      pivot(CENTER, CENTER, CENTER);

    // Build rotation matrix
    // Coordinate system mapping:
    // - SimpleMiner: +X=Forward, +Y=Left, +Z=Up
    // - Minecraft:   +X=East,    +Y=Up,   +Z=South
    //
    // Minecraft's Y rotation (around vertical axis) maps to SimpleMiner's Z rotation
    // Minecraft's X rotation (around horizontal axis) maps to SimpleMiner's X rotation
    // But we need to negate because of coordinate handedness differences

    Mat44 rotationMatrix = Mat44::IDENTITY;

    // Apply Y rotation first (around Minecraft's Y = SimpleMiner's Z axis)
    if (rotY != 0)
    {
        rotationMatrix.AppendZRotation(static_cast<float>(-rotY));
    }

    // Apply X rotation (around Minecraft's X = SimpleMiner's X axis)
    if (rotX != 0)
    {
        rotationMatrix.AppendXRotation(static_cast<float>(-rotX));
    }

    // Transform all vertices in all faces
    for (auto& face : faces)
    {
        for (auto& vertex : face.vertices)
        {
            // Translate to origin (pivot at block center)
            Vec3 pos = vertex.m_position - pivot;

            // Apply rotation
            pos = rotationMatrix.TransformPosition3D(pos);

            // Translate back
            vertex.m_position = pos + pivot;
        }

        // Also rotate the cull direction
        // Map direction through rotation
        if (rotY != 0 || rotX != 0)
        {
            face.cullDirection = RotateDirection(face.cullDirection, rotX, rotY);
        }
    }

    // Invalidate GPU cache since mesh changed
    gpuDataValid = false;
}
