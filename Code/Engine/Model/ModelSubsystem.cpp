#include "ModelSubsystem.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Model/ModelResource.hpp"
#include "Engine/Resource/Atlas/AtlasManager.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include <sstream>

#include "Builtin/ModelBuiltin.hpp"
#include "Compiler/BlockModelCompiler.hpp"
#include "Compiler/GenericModelCompiler.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Voxel/Block/BlockState.hpp"

using namespace enigma::model;
using namespace enigma::core;
using namespace enigma::resource;

void ModelSubsystem::Startup()
{
    LogInfo("ModelSubsystem", "Starting up ModelSubsystem...");

    // Get dependencies on other subsystems
    m_resourceSubsystem = GEngine->GetSubsystem<ResourceSubsystem>();

    RegisterCompilers();

    // Initialize builtin models
    InitializeBuiltinModels();

    LogInfo("ModelSubsystem", "ModelSubsystem startup complete");
}

void ModelSubsystem::Shutdown()
{
    LogInfo("ModelSubsystem", "Shutting down ModelSubsystem...");

    ClearCompiledCache();

    m_statistics = Statistics(); // Reset statistics

    LogInfo("ModelSubsystem", "ModelSubsystem shutdown complete");
}

void ModelSubsystem::InitializeBuiltinModels()
{
    LogInfo("ModelSubsystem", "Initializing builtin models...");

    ModelResourcePtr blockCube = ModelBuiltin::CreateBlockCube();
    m_resourceSubsystem->LoadResource(ResourceLocation("block/cube"), blockCube);
}


std::shared_ptr<RenderMesh> ModelSubsystem::CompileModel(const ResourceLocation& modelPath)
{
    ModelResourcePtr modelResource = std::dynamic_pointer_cast<ModelResource>(m_resourceSubsystem->GetResource(modelPath));
    if (!modelResource)
    {
        LogWarn("ModelSubsystem", "Failed to get model resource: %s", modelPath.ToString().c_str());
        return nullptr;
    }
    // TODO: Right now we assume we only compile block, so we use block atlas.
    CompilerContext context;
    context.atlasManager   = m_resourceSubsystem->GetAtlasManager();
    context.blockAtlas     = m_resourceSubsystem->GetAtlas("blocks");
    context.modelSubsystem = this;
    context.enableLogging  = true; // Enable logging to debug atlas contents

    std::string                     identifier   = modelResource->GetParent()->GetPath();
    std::shared_ptr<IModelCompiler> compiler     = GetCompiler(identifier);
    std::shared_ptr<RenderMesh>     compiledMesh = compiler->Compile(modelResource, context);

    return compiledMesh;
}

std::shared_ptr<RenderMesh> ModelSubsystem::GetCompiledMesh(const std::string& cacheKey)
{
    auto it = m_compiledMeshCache.find(cacheKey);
    return (it != m_compiledMeshCache.end()) ? it->second : nullptr;
}

std::shared_ptr<IModelCompiler> ModelSubsystem::RegisterCompiler(const std::string& templateName, std::shared_ptr<IModelCompiler> compiler)
{
    m_compilers.emplace(templateName, compiler);
    LogInfo("ModelSubsystem", "Registered model compiler: %s", templateName.c_str());
    return compiler;
}

void ModelSubsystem::ClearCompiledCache()
{
    m_compiledMeshCache.clear();
    m_statistics.cachedMeshesCount = 0;
    LogInfo("ModelSubsystem", "Compiled mesh cache cleared");
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

void ModelSubsystem::RegisterCompilers()
{
    RegisterCompiler("null/empty", std::make_unique<GenericModelCompiler>());
    RegisterCompiler("block/cube", std::make_unique<BlockModelCompiler>());
}

/**
 * Retrieves a shared pointer to an IModelCompiler based on the given identifier.
 *
 * This method searches for the specified compiler identifier in the collection of compilers.
 * If the identifier is found, it returns the associated IModelCompiler. If the identifier
 * is not found, a default "null/empty" compiler is returned.
 *
 * @param identifier The unique string identifier for the desired compiler.
 * @return A shared pointer to the IModelCompiler corresponding to the given identifier,
 *         or a shared pointer to a default "null/empty" compiler if the identifier is not found.
 */
std::shared_ptr<IModelCompiler> ModelSubsystem::GetCompiler(const std::string& identifier)
{
    auto it = m_compilers.find(identifier);
    return (it != m_compilers.end()) ? it->second : m_compilers["null/empty"];
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

void ModelSubsystem::CompileAllBlockModels()
{
    LogInfo("ModelSubsystem", "Starting automatic block model compilation...");

    // Get all registered blocks
    auto allBlocks = enigma::registry::block::BlockRegistry::GetAllBlocks();

    if (allBlocks.empty())
    {
        LogWarn("ModelSubsystem", "No blocks registered for model compilation");
        return;
    }

    // Get AtlasManager from ResourceSubsystem
    auto* atlasManager = m_resourceSubsystem ? m_resourceSubsystem->GetAtlasManager() : nullptr;
    if (!atlasManager)
    {
        LogError("ModelSubsystem", "AtlasManager not available - cannot compile block models");
        return;
    }

    LogInfo("ModelSubsystem", "Compiling models for %zu registered blocks...", allBlocks.size());

    int totalCompiled = 0;
    int totalFailed   = 0;

    // Compile models for each registered block
    for (auto& block : allBlocks)
    {
        if (block)
        {
            LogDebug("ModelSubsystem", "Compiling models for block: %s:%s",
                     block->GetNamespace().c_str(), block->GetRegistryName().c_str());

            // Use Block's CompileModels method which handles all states
            block->CompileModels(this, atlasManager);

            // Count successful/failed compilations
            auto allStates = block->GetAllStates();
            for (auto* state : allStates)
            {
                if (state && state->GetRenderMesh())
                {
                    totalCompiled++;
                }
                else
                {
                    totalFailed++;
                }
            }
        }
    }

    LogInfo("ModelSubsystem", "Block model compilation complete: compiled=%d, failed=%d",
            totalCompiled, totalFailed);

    // Update statistics
    m_statistics.cachedMeshesCount = totalCompiled;
}
