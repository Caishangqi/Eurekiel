#include "GlbModelLoader.hpp"

#include "Engine/Core/EngineCommon.hpp"

GlbModelLoader::GlbModelLoader(IRenderer* renderer) : ModelLoader(renderer)
{
}

bool GlbModelLoader::CanLoad(const std::string& extension) const
{
    UNUSED(extension)
    // TODO: Check whether or not have meta file and extension matches
    return true;
}

std::unique_ptr<FMesh> GlbModelLoader::Load(const ResourceLocation& location, const std::string& filePath)
{
    FMesh mesh;

    return std::make_unique<FMesh>(mesh);
}
