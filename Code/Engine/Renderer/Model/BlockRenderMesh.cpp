#include "BlockRenderMesh.hpp"
#include "../../Voxel/Chunk/ChunkMesh.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"


using namespace enigma::renderer::model;
using namespace enigma::core;

void BlockRenderMesh::CreateCube(const std::array<Vec4, 6>& faceUVs, const std::array<Rgba8, 6>& faceColors)
{
    Clear();

    LogInfo("BlockRenderMesh", "Creating cube mesh with %zu face UVs", faceUVs.size());

    // Face order: [down, up, north, south, west, east]
    // Create each face using RenderFace helper methods from parent class

    // Down face (Y=0, looking up)
    Vec2 downUVMin = GetUVMin(faceUVs[0]);
    Vec2 downUVMax = GetUVMax(faceUVs[0]);
    auto downFace  = RenderFace::CreateDownFace(Vec3(0, 0, 0), downUVMin, downUVMax);
    // Override color for all vertices
    for (auto& vertex : downFace.vertices)
    {
        vertex.m_color = faceColors[0];
    }
    AddFace(downFace);

    // Up face (Y=1, looking down)
    Vec2 upUVMin = GetUVMin(faceUVs[1]);
    Vec2 upUVMax = GetUVMax(faceUVs[1]);
    auto upFace  = RenderFace::CreateUpFace(Vec3(0, 0, 0), upUVMin, upUVMax);
    for (auto& vertex : upFace.vertices)
    {
        vertex.m_color = faceColors[1];
    }
    AddFace(upFace);

    // North face (Z=0, looking south)
    Vec2 northUVMin = GetUVMin(faceUVs[2]);
    Vec2 northUVMax = GetUVMax(faceUVs[2]);
    auto northFace  = RenderFace::CreateNorthFace(Vec3(0, 0, 0), northUVMin, northUVMax);
    for (auto& vertex : northFace.vertices)
    {
        vertex.m_color = faceColors[2];
    }
    AddFace(northFace);

    // South face (Z=1, looking north)
    Vec2 southUVMin = GetUVMin(faceUVs[3]);
    Vec2 southUVMax = GetUVMax(faceUVs[3]);
    auto southFace  = RenderFace::CreateSouthFace(Vec3(0, 0, 0), southUVMin, southUVMax);
    for (auto& vertex : southFace.vertices)
    {
        vertex.m_color = faceColors[3];
    }
    AddFace(southFace);

    // West face (X=0, looking east)
    Vec2 westUVMin = GetUVMin(faceUVs[4]);
    Vec2 westUVMax = GetUVMax(faceUVs[4]);
    auto westFace  = RenderFace::CreateWestFace(Vec3(0, 0, 0), westUVMin, westUVMax);
    for (auto& vertex : westFace.vertices)
    {
        vertex.m_color = faceColors[4];
    }
    AddFace(westFace);

    // East face (X=1, looking west)
    Vec2 eastUVMin = GetUVMin(faceUVs[5]);
    Vec2 eastUVMax = GetUVMax(faceUVs[5]);
    auto eastFace  = RenderFace::CreateEastFace(Vec3(0, 0, 0), eastUVMin, eastUVMax);
    for (auto& vertex : eastFace.vertices)
    {
        vertex.m_color = faceColors[5];
    }
    AddFace(eastFace);

    LogInfo("BlockRenderMesh", "Cube mesh created successfully. Faces: %zu, Vertices: %zu, Triangles: %zu",
            faces.size(), GetVertexCount(), GetTriangleCount());
}

void BlockRenderMesh::TransformAndAppendTo(ChunkMesh* chunkMesh, const Vec3& blockPos) const
{
    if (!chunkMesh || IsEmpty())
    {
        return;
    }

    LogInfo("BlockRenderMesh", "Transforming and appending mesh to chunk at position (%f, %f, %f)",
            blockPos.x, blockPos.y, blockPos.z);

    // Transform each face and add to chunk mesh
    for (const auto& face : faces)
    {
        if (face.vertices.size() >= 4) // Ensure we have a complete quad
        {
            std::array<Vertex_PCU, 4> transformedQuad;

            // Transform the first 4 vertices (assuming quad)
            for (int i = 0; i < 4; ++i)
            {
                transformedQuad[i] = face.vertices[i];
                transformedQuad[i].m_position += blockPos;
            }

            // Add the transformed quad to chunk mesh
            chunkMesh->AddOpaqueQuad(transformedQuad);
        }
    }
}

void BlockRenderMesh::CreateSimpleCube(const Vec4& uv, const Rgba8& color)
{
    // Create uniform array for all faces
    std::array<Vec4, 6>  uniformUVs;
    std::array<Rgba8, 6> uniformColors;

    uniformUVs.fill(uv);
    uniformColors.fill(color);

    CreateCube(uniformUVs, uniformColors);
}
