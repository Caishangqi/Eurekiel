#include "ModelRegistry.hpp"
#include "../../Renderer/Model/BlockRenderMesh.hpp"
#include "../../Core/StringUtils.hpp"
#include "../../Core/FileUtils.hpp"
#include <filesystem>

using namespace enigma::resource::model;
using namespace enigma::renderer::model;

void ModelRegistry::RegisterBuiltinModels()
{
    core::LogInfo("ModelRegistry", "Registering built-in models...");

    // Register block/cube model
    auto cubeModel = CreateBuiltinCubeModel();
    if (cubeModel)
    {
        m_builtinModels["block/cube"] = cubeModel;
        core::LogInfo("ModelRegistry", "Registered builtin model: block/cube");
    }
    else
    {
        core::LogError("ModelRegistry", "Failed to create builtin block/cube model");
    }

    core::LogInfo("ModelRegistry", "Built-in model registration complete. Total: %zu models", m_builtinModels.size());
}

std::shared_ptr<ModelResource> ModelRegistry::GetModel(const ResourceLocation& location)
{
    // First check built-in models
    std::string builtinKey = GetBuiltinKey(location);
    auto builtinIt = m_builtinModels.find(builtinKey);
    if (builtinIt != m_builtinModels.end())
    {
        return builtinIt->second;
    }

    // Check file model cache
    auto cacheIt = m_fileModelCache.find(location);
    if (cacheIt != m_fileModelCache.end())
    {
        return cacheIt->second;
    }

    // Try to load from file
    auto model = LoadModelFromFile(location);
    if (model)
    {
        m_fileModelCache[location] = model;
    }

    return model;
}

std::shared_ptr<BlockRenderMesh> ModelRegistry::GetCompiledMesh(const std::string& key)
{
    auto it = m_compiledMeshCache.find(key);
    return (it != m_compiledMeshCache.end()) ? it->second : nullptr;
}

void ModelRegistry::CacheCompiledMesh(const std::string& key, std::shared_ptr<BlockRenderMesh> mesh)
{
    if (mesh)
    {
        m_compiledMeshCache[key] = mesh;
    }
}

void ModelRegistry::ClearCompiledCache()
{
    m_compiledMeshCache.clear();
    core::LogInfo("ModelRegistry", "Compiled mesh cache cleared");
}

bool ModelRegistry::IsBuiltinModel(const ResourceLocation& location) const
{
    std::string builtinKey = GetBuiltinKey(location);
    return m_builtinModels.find(builtinKey) != m_builtinModels.end();
}

std::shared_ptr<ModelResource> ModelRegistry::CreateBuiltinCubeModel()
{
    // For now, create a minimal cube model that will be populated by JSON-like data
    // This needs to be enhanced when we have proper model building support
    
    core::LogInfo("ModelRegistry", "Creating built-in block/cube model");
    
    // Create the model with basic structure
    auto model = std::make_shared<ModelResource>(ResourceLocation("minecraft", "block/cube"));
    
    // Note: The actual cube geometry will need to be set through the proper ModelResource interface
    // or we need to add friend class access. For now, we create a minimal model that indicates
    // it's a built-in cube model. The BlockModelCompiler will handle creating the actual geometry.
    
    return model;
}

std::shared_ptr<ModelResource> ModelRegistry::LoadModelFromFile(const ResourceLocation& location)
{
    // Construct file path from resource location
    std::string filePath = "Run/Data/models/" + location.GetPath() + ".json";
    
    if (!std::filesystem::exists(filePath))
    {
        core::LogWarning("ModelRegistry", "Model file not found: %s", filePath.c_str());
        return nullptr;
    }

    auto model = std::make_shared<ModelResource>(location);
    
    // Load JSON data
    std::string jsonContent;
    if (!core::LoadFileToString(filePath, jsonContent))
    {
        core::LogError("ModelRegistry", "Failed to read model file: %s", filePath.c_str());
        return nullptr;
    }

    JsonObject jsonObj;
    if (!jsonObj.ParseFromString(jsonContent))
    {
        core::LogError("ModelRegistry", "Failed to parse JSON in model file: %s", filePath.c_str());
        return nullptr;
    }

    if (!model->LoadFromJson(jsonObj))
    {
        core::LogError("ModelRegistry", "Failed to load model from JSON: %s", filePath.c_str());
        return nullptr;
    }

    core::LogInfo("ModelRegistry", "Loaded model from file: %s", location.ToString().c_str());
    return model;
}

std::string ModelRegistry::GetBuiltinKey(const ResourceLocation& location) const
{
    // Convert "minecraft:block/cube" or "block/cube" to "block/cube"
    std::string path = location.GetPath();
    
    // Handle namespace-less built-ins
    if (location.GetNamespace() == "minecraft" || location.GetNamespace().empty())
    {
        return path;
    }
    
    return location.ToString();
}