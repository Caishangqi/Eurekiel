#include "BlockModelCompiler.hpp"
#include "../../Model/ModelSubsystem.hpp"
#include <set>

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Renderer/Model/BlockRenderMesh.hpp"

using namespace enigma::renderer::model;
using namespace enigma::model;

std::shared_ptr<BlockRenderMesh> BlockModelCompiler::CompileBlockModel(
    const std::shared_ptr<ModelResource>& model,
    const CompilerContext&                context)
{
    if (!model || !context.modelSubsystem)
    {
        LogError("BlockModelCompiler", "Invalid model or context");
        return nullptr;
    }

    if (context.enableLogging)
    {
        LogInfo("BlockModelCompiler", "Compiling block model: %s",
                model->GetMetadata().location.ToString().c_str());
    }

    // Check if model has parent and needs inheritance
    auto parentModel = GetParentModel(model, context);
    if (parentModel)
    {
        if (context.enableLogging)
        {
            LogInfo("BlockModelCompiler", "Model has parent: %s",
                    model->GetParent()->ToString().c_str());
        }
    }

    // Resolve texture variables
    auto resolvedTextures = ResolveTextureVariables(model, context);

    if (resolvedTextures.empty())
    {
        LogWarn("BlockModelCompiler", "No textures resolved for model: %s",
                model->GetMetadata().location.ToString().c_str());
    }

    // For Assignment 1, we only handle cube models
    // Check if this model (or its parent) is the built-in cube
    bool isCubeModel = false;
    if (parentModel && context.modelSubsystem->IsBuiltinModel(ResourceLocation("minecraft", "block/cube")))
    {
        isCubeModel = true;
    }
    else if (context.modelSubsystem->IsBuiltinModel(model->GetMetadata().location))
    {
        isCubeModel = true;
    }

    if (!isCubeModel)
    {
        LogWarn("BlockModelCompiler", "Non-cube models not supported in Assignment 1: %s",
                model->GetMetadata().location.ToString().c_str());
        return nullptr;
    }

    // Compile cube model using BlockRenderMesh
    auto compiledMesh = CompileCubeModel(resolvedTextures, context);

    if (context.enableLogging && compiledMesh)
    {
        LogInfo("BlockModelCompiler", "Successfully compiled block model: %s (vertices: %zu, triangles: %zu)",
                model->GetMetadata().location.ToString().c_str(),
                compiledMesh->GetVertexCount(), compiledMesh->GetTriangleCount());
    }

    return compiledMesh;
}

std::string BlockModelCompiler::CreateCacheKey(const std::shared_ptr<ModelResource>& model)
{
    if (!model)
    {
        return "";
    }

    // Simple cache key: model location + texture hash
    std::string key = model->GetMetadata().location.ToString();

    // Add texture information to make cache key unique
    const auto& textures = model->GetTextures();
    for (const auto& texture : textures)
    {
        key += "_" + texture.first + "=" + texture.second.ToString();
    }

    return key;
}

std::map<std::string, ResourceLocation> BlockModelCompiler::ResolveTextureVariables(
    const std::shared_ptr<ModelResource>& model,
    const CompilerContext&                context)
{
    std::map<std::string, ResourceLocation> resolved;

    if (!model)
    {
        return resolved;
    }

    // Start with the model's own texture assignments
    auto textureMap = model->GetTextures();

    // If model has parent, inherit parent's textures first
    auto parentModel = GetParentModel(model, context);
    if (parentModel)
    {
        auto parentTextures = parentModel->GetTextures();
        // Parent textures are base, child overrides
        for (const auto& parentTexture : parentTextures)
        {
            textureMap[parentTexture.first] = parentTexture.second;
        }
        // Apply child textures over parent
        const auto& childTextures = model->GetTextures();
        for (const auto& childTexture : childTextures)
        {
            textureMap[childTexture.first] = childTexture.second;
        }
    }

    // Resolve variables for standard cube faces
    std::vector<std::string> faceNames = {"down", "up", "north", "south", "west", "east"};

    for (const std::string& faceName : faceNames)
    {
        std::set<std::string> visited;
        auto                  resolvedTexture = ResolveTextureVariable("#" + faceName, textureMap, visited);

        if (resolvedTexture.has_value())
        {
            resolved[faceName] = resolvedTexture.value();

            if (context.enableLogging)
            {
                LogInfo("BlockModelCompiler", "Resolved texture %s ¡ú %s",
                        faceName.c_str(), resolvedTexture->ToString().c_str());
            }
        }
        else
        {
            // Fallback to missing texture
            resolved[faceName] = ResourceLocation("minecraft", "missingno");
            LogWarn("BlockModelCompiler", "Failed to resolve texture for face: %s", faceName.c_str());
        }
    }

    return resolved;
}

std::shared_ptr<BlockRenderMesh> BlockModelCompiler::CompileCubeModel(
    const std::map<std::string, ResourceLocation>& resolvedTextures,
    const CompilerContext&                         context)
{
    auto mesh = std::make_shared<BlockRenderMesh>();

    // Get UV coordinates for each face
    std::array<Vec4, 6>      faceUVs;
    std::vector<std::string> faceNames = {"down", "up", "north", "south", "west", "east"};

    for (size_t i = 0; i < 6; ++i)
    {
        auto textureIt = resolvedTextures.find(faceNames[i]);
        if (textureIt != resolvedTextures.end())
        {
            faceUVs[i] = GetTextureUV(textureIt->second, context);
        }
        else
        {
            // Default UV coordinates (full texture)
            faceUVs[i] = Vec4(0.0f, 0.0f, 1.0f, 1.0f);
        }
    }

    // Create cube with resolved UVs
    mesh->CreateCube(faceUVs);

    return mesh;
}

Vec4 BlockModelCompiler::GetTextureUV(const ResourceLocation& textureLocation, const CompilerContext& context)
{
    if (!context.blockAtlas)
    {
        LogWarn("BlockModelCompiler", "No block atlas available for UV lookup");
        return Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Find sprite in atlas
    const AtlasSprite* sprite = context.blockAtlas->FindSprite(textureLocation);
    if (!sprite)
    {
        LogWarn("BlockModelCompiler", "Texture not found in atlas: %s",
                textureLocation.ToString().c_str());
        return Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Return UV coordinates as Vec4 (minX, minY, maxX, maxY)
    return Vec4(sprite->uvMin.x, sprite->uvMin.y, sprite->uvMax.x, sprite->uvMax.y);
}

std::optional<ResourceLocation> BlockModelCompiler::ResolveTextureVariable(
    const std::string&                             variable,
    const std::map<std::string, ResourceLocation>& textureMap,
    std::set<std::string>&                         visited)
{
    // Prevent infinite recursion
    if (visited.find(variable) != visited.end())
    {
        LogWarn("BlockModelCompiler", "Circular texture reference detected: %s", variable.c_str());
        return std::nullopt;
    }
    visited.insert(variable);

    // Remove # prefix for lookup
    std::string lookupKey = variable;
    if (!lookupKey.empty() && lookupKey[0] == '#')
    {
        lookupKey = lookupKey.substr(1);
    }

    auto it = textureMap.find(lookupKey);
    if (it == textureMap.end())
    {
        return std::nullopt;
    }

    const ResourceLocation& value = it->second;

    // Check if value is another variable (starts with #)
    const std::string& valuePath = value.GetPath();
    if (!valuePath.empty() && valuePath[0] == '#')
    {
        return ResolveTextureVariable(value.GetPath(), textureMap, visited);
    }

    // Direct texture reference
    return value;
}

std::shared_ptr<ModelResource> BlockModelCompiler::GetParentModel(
    const std::shared_ptr<ModelResource>& model,
    const CompilerContext&                context)
{
    if (!model || !model->HasParent() || !context.modelSubsystem)
    {
        return nullptr;
    }

    return context.modelSubsystem->GetModel(model->GetParent().value());
}
