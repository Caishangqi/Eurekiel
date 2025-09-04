#include "ModelSubsystem.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Model/ModelResource.hpp"
#include "Engine/Resource/Atlas/AtlasManager.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <sstream>

#include "Engine/Core/Logger/LoggerAPI.hpp"

using namespace enigma::model;
using namespace enigma::core;
using namespace enigma::resource;

void ModelSubsystem::Startup()
{
    LogInfo("ModelSubsystem", "Starting up ModelSubsystem...");

    // Get dependencies on other subsystems
    m_resourceSubsystem = GEngine->GetSubsystem<ResourceSubsystem>();

    // Initialize builtin models
    InitializeBuiltinModels();

    LogInfo("ModelSubsystem", "ModelSubsystem startup complete");
}

void ModelSubsystem::Shutdown()
{
    LogInfo("ModelSubsystem", "Shutting down ModelSubsystem...");

    ClearCompiledCache();
    m_builtinModels.clear();

    m_statistics = Statistics(); // Reset statistics

    LogInfo("ModelSubsystem", "ModelSubsystem shutdown complete");
}

void ModelSubsystem::InitializeBuiltinModels()
{
    LogInfo("ModelSubsystem", "Initializing builtin models...");

    // Create builtin block/cube model
    auto cubeModel = CreateBuiltinCubeModel();
    if (cubeModel)
    {
        m_builtinModels["block/cube"] = cubeModel;
        m_statistics.builtinModelsCount++;
        LogInfo("ModelSubsystem", "Registered builtin model: block/cube");
    }
    else
    {
        LogError("ModelSubsystem", "Failed to create builtin block/cube model");
    }

    LogInfo("ModelSubsystem", "Builtin model initialization complete. Total: %zu models", m_builtinModels.size());
}

std::shared_ptr<ModelResource> ModelSubsystem::GetModel(const ResourceLocation& location)
{
    // First check builtin models
    std::string builtinKey = GetBuiltinKey(location);
    auto        builtinIt  = m_builtinModels.find(builtinKey);
    if (builtinIt != m_builtinModels.end())
    {
        return builtinIt->second;
    }

    // Try to load from file via ResourceSubsystem
    return LoadModelFromFile(location);
}

std::shared_ptr<RenderMesh> ModelSubsystem::CompileModel(const ResourceLocation& modelPath)
{
    return CompileBlockModel(modelPath, {});
}

std::shared_ptr<RenderMesh> ModelSubsystem::CompileBlockModel(const ResourceLocation& modelPath, const std::unordered_map<std::string, std::string>& properties)
{
    // Create cache key
    std::string cacheKey = CreateCacheKey(modelPath, properties);

    // Check cache first
    auto cachedMesh = GetCompiledMesh(cacheKey);
    if (cachedMesh)
    {
        m_statistics.cacheHits++;
        return cachedMesh;
    }

    m_statistics.cacheMisses++;

    // Get the model resource
    auto modelResource = GetModel(modelPath);
    if (!modelResource)
    {
        LogWarn("ModelSubsystem", "Failed to get model resource: %s", modelPath.ToString().c_str());
        return nullptr;
    }

    // TODO: Implement model compilation here

    // For now, create a placeholder mesh
    auto compiledMesh = std::make_shared<RenderMesh>();

    // Cache the result
    m_compiledMeshCache[cacheKey]  = compiledMesh;
    m_statistics.cachedMeshesCount = m_compiledMeshCache.size();

    LogInfo("ModelSubsystem", "Compiled model: %s (cache size: %zu)",
            modelPath.ToString().c_str(), m_statistics.cachedMeshesCount);

    return compiledMesh;
}

std::shared_ptr<RenderMesh> ModelSubsystem::GetCompiledMesh(const std::string& cacheKey)
{
    auto it = m_compiledMeshCache.find(cacheKey);
    return (it != m_compiledMeshCache.end()) ? it->second : nullptr;
}

void ModelSubsystem::ClearCompiledCache()
{
    m_compiledMeshCache.clear();
    m_statistics.cachedMeshesCount = 0;
    LogInfo("ModelSubsystem", "Compiled mesh cache cleared");
}

bool ModelSubsystem::IsBuiltinModel(const ResourceLocation& location) const
{
    std::string builtinKey = GetBuiltinKey(location);
    return m_builtinModels.find(builtinKey) != m_builtinModels.end();
}

void ModelSubsystem::OnResourceReload()
{
    LogInfo("ModelSubsystem", "Handling resource reload...");

    ClearCompiledCache();
    // Builtin models don't need to be reloaded as they're hardcoded

    LogInfo("ModelSubsystem", "Resource reload complete");
}

std::string ModelSubsystem::CreateCacheKey(const ResourceLocation&                             modelPath,
                                           const std::unordered_map<std::string, std::string>& properties) const
{
    std::ostringstream oss;
    oss << modelPath.ToString();

    // Add properties to cache key for uniqueness
    if (!properties.empty())
    {
        oss << "|";
        for (const auto& [key, value] : properties)
        {
            oss << key << "=" << value << ";";
        }
    }

    return oss.str();
}

std::shared_ptr<ModelResource> ModelSubsystem::CreateBuiltinCubeModel()
{
    LogInfo("ModelSubsystem", "Creating builtin block/cube model");

    // Create basic cube model structure
    // Note: This is a simplified placeholder - the full implementation
    // would create the actual 6-face cube geometry
    auto model = std::make_shared<ModelResource>(ResourceLocation("minecraft", "block/cube"));

    // The builtin cube model will be used as a template by the compiler
    // The actual geometry creation will be handled by the model compiler

    return model;
}

std::shared_ptr<ModelResource> ModelSubsystem::LoadModelFromFile(const ResourceLocation& location)
{
    if (!m_resourceSubsystem)
    {
        LogError("ModelSubsystem", "ResourceSubsystem not available for loading model: %s",
                 location.ToString().c_str());
        return nullptr;
    }

    // Use ResourceSubsystem to load the model file
    // TODO: Implement proper model loading via ResourceSubsystem
    LogInfo("ModelSubsystem", "Loading model from file: %s", location.ToString().c_str());
    ResourcePtr      resourcePtr = m_resourceSubsystem->GetResource(location);
    ModelResourcePtr model       = std::static_pointer_cast<ModelResource>(m_resourceSubsystem->GetResource(location));
    return model;
}

std::string ModelSubsystem::GetBuiltinKey(const ResourceLocation& location) const
{
    // Convert "minecraft:block/cube" or "block/cube" to "block/cube"
    std::string path = location.GetPath();

    // Handle namespace-less builtins
    if (location.GetNamespace() == "minecraft" || location.GetNamespace().empty())
    {
        return path;
    }

    return location.ToString();
}
