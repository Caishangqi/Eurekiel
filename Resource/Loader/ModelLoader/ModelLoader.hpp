#pragma once
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Resource/Loader/Loader.hpp"

struct Vec2;
struct Vec3;
class IRenderer;

// TODO: Hack the simple Mesh, need refactor in the future
struct FMesh
{
    // Temp
    std::vector<Vec3> vertexPosition;
    std::vector<Vec3> vertexNormal;
    std::vector<Vec2> uvTexCoords;
    // Complete
    std::vector<Vertex_PCUTBN> vertices;
};

class ModelLoader : public ResourceLoader<FMesh>
{
protected:
    IRenderer* m_renderer; // Cached renderer backend

public:
    ModelLoader(IRenderer* renderer) : m_renderer(renderer)
    {
    }

    virtual ~ModelLoader() = default;
};
