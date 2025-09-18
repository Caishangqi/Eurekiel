#pragma once
#include "ModelResource.hpp"
#include "../ResourceCommon.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace enigma::renderer::model
{
    class BlockRenderMesh;
}

namespace enigma::resource::model
{
    using namespace enigma::resource;
    using namespace enigma::renderer::model;

    /**
     * @brief Registry for managing model resources and compiled mesh cache
     * 
     * Handles both built-in models (like block/cube) and file-based models.
     * Provides caching for compiled BlockRenderMesh instances to avoid
     * recompilation of the same model multiple times.
     */
    class ModelRegistry
    {
    public:
        ModelRegistry() = default;
        ~ModelRegistry() = default;

        /**
         * @brief Initialize built-in models (like block/cube)
         * Should be called during engine initialization
         */
        void RegisterBuiltinModels();

        /**
         * @brief Get a model by resource location
         * 
         * First checks built-in models, then attempts to load from file.
         * Built-in models have priority over file-based models.
         * 
         * @param location Model resource location (e.g., "block/cube", "simpleminer:block/grass")
         * @return Shared pointer to ModelResource, nullptr if not found or failed to load
         */
        std::shared_ptr<ModelResource> GetModel(const ResourceLocation& location);

        /**
         * @brief Get a cached compiled mesh
         * 
         * @param key Cache key (usually model path + texture combination)
         * @return Cached mesh or nullptr if not found
         */
        std::shared_ptr<BlockRenderMesh> GetCompiledMesh(const std::string& key);

        /**
         * @brief Cache a compiled mesh for future use
         * 
         * @param key Cache key for lookup
         * @param mesh Compiled mesh to cache
         */
        void CacheCompiledMesh(const std::string& key, std::shared_ptr<BlockRenderMesh> mesh);

        /**
         * @brief Clear all cached meshes (for memory management)
         */
        void ClearCompiledCache();

        /**
         * @brief Check if a model is built-in (hardcoded)
         */
        bool IsBuiltinModel(const ResourceLocation& location) const;

        /**
         * @brief Get cache statistics for debugging
         */
        size_t GetCompiledCacheSize() const { return m_compiledMeshCache.size(); }
        size_t GetBuiltinModelCount() const { return m_builtinModels.size(); }

    private:
        // Built-in models (hardcoded, like block/cube)
        std::unordered_map<std::string, std::shared_ptr<ModelResource>> m_builtinModels;
        
        // File-based models cache
        std::unordered_map<ResourceLocation, std::shared_ptr<ModelResource>> m_fileModelCache;
        
        // Compiled mesh cache (key = model path + texture combination)
        std::unordered_map<std::string, std::shared_ptr<BlockRenderMesh>> m_compiledMeshCache;

        /**
         * @brief Create the built-in block/cube model
         */
        std::shared_ptr<ModelResource> CreateBuiltinCubeModel();

        /**
         * @brief Load a model from file system
         */
        std::shared_ptr<ModelResource> LoadModelFromFile(const ResourceLocation& location);

        /**
         * @brief Convert ResourceLocation to built-in key
         */
        std::string GetBuiltinKey(const ResourceLocation& location) const;
    };
}