#pragma once
#include "../../Resource/Model/ModelResource.hpp"
#include "../../Renderer/Model/BlockRenderMesh.hpp"
#include "../../Resource/Atlas/TextureAtlas.hpp"
#include "../../Resource/Atlas/AtlasManager.hpp"
#include <memory>
#include <string>
#include <map>
#include <set>
#include <optional>

// Forward declaration
namespace enigma::model
{
    class ModelSubsystem;
}

namespace enigma::renderer::model
{
    using namespace enigma::resource::model;
    using namespace enigma::resource;

    /**
     * @brief Specialized compiler for block models, uses ModelCompiler as base
     * 
     * Handles:
     * - Parent model inheritance (e.g., grass.json inherits from block/cube)
     * - Texture variable resolution (#down ¡ú simpleminer:block/aether_dirt)  
     * - UV coordinate mapping using TextureAtlas
     * - Block-specific cube geometry generation
     * - Integration with ModelSubsystem for builtin models
     */
    class BlockModelCompiler
    {
    public:
        /**
         * @brief Compiler context for caching and resource access
         */
        struct CompilerContext
        {
            enigma::model::ModelSubsystem* modelSubsystem = nullptr; // For getting parent models
            const AtlasManager*            atlasManager   = nullptr; // For texture UV lookup
            const TextureAtlas*            blockAtlas     = nullptr; // Specific block texture atlas
            bool                           enableLogging  = false; // Debug logging

            CompilerContext(enigma::model::ModelSubsystem* subsystem, const AtlasManager* atlas, bool logging = false)
                : modelSubsystem(subsystem), atlasManager(atlas), enableLogging(logging)
            {
                // Get the blocks atlas specifically
                if (atlasManager)
                {
                    blockAtlas = atlasManager->GetAtlas("blocks");
                }
            }
        };

        /**
         * @brief Compile a block model into a BlockRenderMesh
         * 
         * @param model The model to compile (e.g., grass.json)
         * @param context Compiler context with registries and atlases
         * @return Compiled block mesh or nullptr on failure
         */
        static std::shared_ptr<BlockRenderMesh> CompileBlockModel(
            const std::shared_ptr<ModelResource>& model,
            const CompilerContext&                context);

        /**
         * @brief Create a cache key for the compiled model
         * 
         * Combines model location with texture assignments to create unique cache key.
         * 
         * @param model The model resource
         * @return Cache key string
         */
        static std::string CreateCacheKey(const std::shared_ptr<ModelResource>& model);

    private:
        /**
         * @brief Resolve texture variables in model (e.g., #down ¡ú actual texture location)
         * 
         * @param model Model to resolve textures for
         * @param context Compiler context
         * @return Map of resolved textures (variable name ¡ú ResourceLocation)
         */
        static std::map<std::string, ResourceLocation> ResolveTextureVariables(
            const std::shared_ptr<ModelResource>& model,
            const CompilerContext&                context);

        /**
         * @brief Compile built-in cube model geometry using BlockRenderMesh
         * 
         * Creates the standard 6-face cube with resolved textures.
         * Uses BlockRenderMesh's CreateCube method for block-specific geometry.
         * 
         * @param resolvedTextures Map of face names to texture locations
         * @param context Compiler context
         * @return Compiled block mesh
         */
        static std::shared_ptr<BlockRenderMesh> CompileCubeModel(
            const std::map<std::string, ResourceLocation>& resolvedTextures,
            const CompilerContext&                         context);

        /**
         * @brief Get UV coordinates for a texture from atlas
         * 
         * @param textureLocation Resource location of texture
         * @param context Compiler context
         * @return UV coordinates as Vec4 (minX, minY, maxX, maxY)
         */
        static Vec4 GetTextureUV(const ResourceLocation& textureLocation, const CompilerContext& context);

        /**
         * @brief Resolve a single texture variable
         * 
         * Handles recursive texture variable resolution (e.g., #side ¡ú #all ¡ú actual texture)
         * 
         * @param variable Variable to resolve (e.g., "#down")
         * @param textureMap Map of texture variable assignments
         * @param visited Set of visited variables (for cycle detection)
         * @return Resolved texture location, or empty optional if unresolved
         */
        static std::optional<ResourceLocation> ResolveTextureVariable(
            const std::string&                             variable,
            const std::map<std::string, ResourceLocation>& textureMap,
            std::set<std::string>&                         visited);

        /**
         * @brief Get parent model recursively using ModelSubsystem
         * 
         * @param model Current model
         * @param context Compiler context
         * @return Parent model or nullptr if no parent
         */
        static std::shared_ptr<ModelResource> GetParentModel(
            const std::shared_ptr<ModelResource>& model,
            const CompilerContext&                context);
    };
}
