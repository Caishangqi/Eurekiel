#include "ModelCompiler.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include "../../Math/Vec2.hpp"
#include "../../Core/Vertex_PCU.hpp"
#include <sstream>

using namespace enigma::renderer::model;

std::shared_ptr<RenderMesh> ModelCompiler::Compile(std::shared_ptr<ModelResource> model)
{
    if (!model)
    {
        LogError("ModelCompiler", "Cannot compile null model");
        return nullptr;
    }

    // Create new render mesh
    auto renderMesh = std::make_shared<RenderMesh>();

    // TODO: Implement full model compilation once ModelResource interface is complete
    LogInfo("ModelCompiler", "Created placeholder compiled mesh for model");

    return renderMesh;
}

std::shared_ptr<RenderMesh> ModelCompiler::CompileWithCache(std::shared_ptr<ModelResource> model)
{
    if (!model)
    {
        return nullptr;
    }

    // Generate cache key
    std::string cacheKey = GenerateCacheKey(model);

    // Check cache first
    auto cacheIt = m_meshCache.find(cacheKey);
    if (cacheIt != m_meshCache.end())
    {
        auto cachedMesh = cacheIt->second.lock();
        if (cachedMesh)
        {
            return cachedMesh;
        }
        else
        {
            // Remove expired weak_ptr
            m_meshCache.erase(cacheIt);
        }
    }

    // Compile new mesh
    auto compiledMesh = Compile(model);
    if (compiledMesh)
    {
        // Store in cache
        m_meshCache[cacheKey] = compiledMesh;
    }

    return compiledMesh;
}

std::vector<RenderFace> ModelCompiler::CompileElements(
    const std::vector<ModelElement>&               elements,
    const std::map<std::string, ResourceLocation>& resolvedTextures)
{
    std::vector<RenderFace> faces;
    // TODO: Implement once ModelElement interface is complete
    UNUSED(elements)
    UNUSED(resolvedTextures)
    return faces;
}

std::vector<RenderFace> ModelCompiler::CompileElement(
    const ModelElement&                            element,
    const std::map<std::string, ResourceLocation>& resolvedTextures)
{
    std::vector<RenderFace> faces;
    // TODO: Implement once ModelElement interface is complete
    UNUSED(element)
    UNUSED(resolvedTextures)
    return faces;
}

RenderFace ModelCompiler::CompileFace(
    const std::string&                             faceDirection,
    const ModelFace&                               face,
    const ModelElement&                            element,
    const std::map<std::string, ResourceLocation>& resolvedTextures)
{
    // Convert direction string to enum
    Direction dir = StringToDirection(faceDirection);

    // Create placeholder render face
    RenderFace renderFace;
    renderFace.cullDirection = dir;
    // TODO: Implement once ModelFace and ModelElement interfaces are complete
    UNUSED(face)
    UNUSED(element)
    UNUSED(resolvedTextures)
    return renderFace;
}

ResourceLocation ModelCompiler::ResolveTextureVariable(
    const std::string&                             textureVar,
    const std::map<std::string, ResourceLocation>& resolvedTextures)
{
    // If it starts with #, it's a variable
    if (!textureVar.empty() && textureVar[0] == '#')
    {
        std::string varName = textureVar.substr(1);
        auto        it      = resolvedTextures.find(varName);
        if (it != resolvedTextures.end())
        {
            return it->second;
        }
        // Return missing texture if variable not found
        return ResourceLocation("minecraft", "missingno");
    }

    // Direct texture reference
    return ResourceLocation::Parse(textureVar);
}

std::pair<Vec2, Vec2> ModelCompiler::GetAtlasUV(const ResourceLocation& texturePath, const Vec4& modelUV)
{
    if (!m_atlas)
    {
        // Return default UVs if no atlas
        return {Vec2(0, 0), Vec2(1, 1)};
    }

    // TODO: Implement atlas UV mapping once TextureAtlas interface is complete
    UNUSED(texturePath)
    UNUSED(modelUV)
    return {Vec2(0, 0), Vec2(1, 1)};
}

enigma::voxel::property::Direction ModelCompiler::StringToDirection(const std::string& direction)
{
    using namespace enigma::voxel::property;
    if (direction == "down") return Direction::DOWN;
    if (direction == "up") return Direction::UP;
    if (direction == "north") return Direction::NORTH;
    if (direction == "south") return Direction::SOUTH;
    if (direction == "west") return Direction::WEST;
    if (direction == "east") return Direction::EAST;
    return Direction::NORTH; // Default
}

std::vector<Vertex_PCU> ModelCompiler::CreateFaceVertices(
    voxel::property::Direction faceDir,
    const Vec3&                from,
    const Vec3&                to,
    const Vec2&                uvMin,
    const Vec2&                uvMax,
    int                        rotation)
{
    std::vector<Vertex_PCU> vertices(4);

    // TODO: Implement proper vertex generation based on direction and bounds
    // This is a placeholder implementation
    UNUSED(faceDir)
    UNUSED(from)
    UNUSED(to)
    UNUSED(uvMin)
    UNUSED(uvMax)
    UNUSED(rotation)

    return vertices;
}

Vec2 ModelCompiler::RotateUV(const Vec2& uv, int rotation)
{
    // TODO: Implement UV rotation
    UNUSED(rotation)
    return uv;
}

std::string ModelCompiler::GenerateCacheKey(std::shared_ptr<ModelResource> model)
{
    if (!model)
    {
        return "";
    }

    // TODO: Implement proper cache key generation once ModelResource interface is complete
    return "placeholder_key";
}
