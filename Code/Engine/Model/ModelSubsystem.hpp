#pragma once
#include "Engine/Core/SubsystemManager.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Resource/ResourceCommon.hpp"
#include "Engine/Renderer/Model/RenderMesh.hpp"
#include <memory>
#include <string>
#include <unordered_map>

// Forward declarations
namespace enigma::resource
{
    class AtlasManager;
    class ResourceSubsystem;
}

namespace enigma::resource::model
{
    class ModelResource;
}

namespace enigma::model
{
    using namespace enigma::resource;
    using namespace enigma::renderer::model;

    /**
     * @class ModelSubsystem
     * @brief Independent subsystem for model compilation and caching
     * 
     * Responsibilities:
     * - Manage builtin models (like block/cube)
     * - Compile ModelResource -> RenderMesh 
     * - Cache compiled meshes to avoid recompilation
     * - Provide models to BlockStateBuilder and other systems
     * 
     * This follows Minecraft's ModelBakery pattern where models are compiled
     * from JSON resources into runtime-optimized mesh data.
     */
    class ModelSubsystem : public enigma::core::EngineSubsystem
    {
    public:
        DECLARE_SUBSYSTEM(ModelSubsystem, "model", 200)

        ModelSubsystem()           = default;
        ~ModelSubsystem() override = default;

        // EngineSubsystem interface
        void Startup() override;
        void Shutdown() override;
        bool RequiresGameLoop() const override { return false; }

        /**
         * @brief Initialize builtin models (like block/cube)
         * Creates hardcoded model templates that don't exist as files
         */
        void InitializeBuiltinModels();

        /**
         * @brief Get a model resource by location
         * First checks builtins, then loads from file via ResourceSubsystem
         */
        std::shared_ptr<resource::model::ModelResource> GetModel(const ResourceLocation& location);

        /**
         * @brief Compile a model resource into a renderable mesh
         * Uses caching to avoid recompilation of identical models
         */
        std::shared_ptr<RenderMesh> CompileModel(const ResourceLocation& modelPath);

        /**
         * @brief Compile a model with specific properties (for BlockStates)
         */
        std::shared_ptr<RenderMesh> CompileBlockModel(
            const ResourceLocation&                             modelPath,
            const std::unordered_map<std::string, std::string>& properties = {}
        );

        /**
         * @brief Get cached compiled mesh by key
         */
        std::shared_ptr<RenderMesh> GetCompiledMesh(const std::string& cacheKey);

        /**
         * @brief Clear all cached meshes (for resource reload)
         */
        void ClearCompiledCache();

        /**
         * @brief Check if a model is builtin (hardcoded)
         */
        bool IsBuiltinModel(const ResourceLocation& location) const;

        /**
         * @brief Get statistics for debugging
         */
        struct Statistics
        {
            size_t builtinModelsCount = 0;
            size_t cachedMeshesCount  = 0;
            size_t cacheHits          = 0;
            size_t cacheMisses        = 0;
            float  totalCompileTime   = 0.0f;
        };

        const Statistics& GetStatistics() const { return m_statistics; }

        /**
         * @brief Handle resource reload events
         */
        void OnResourceReload();

    private:
        // Dependencies on other subsystems
        ResourceSubsystem* m_resourceSubsystem = nullptr;

        // Builtin model templates (hardcoded, like block/cube)
        std::unordered_map<std::string, std::shared_ptr<resource::model::ModelResource>> m_builtinModels;

        // Compiled mesh cache (key = model location + properties hash)
        std::unordered_map<std::string, std::shared_ptr<RenderMesh>> m_compiledMeshCache;

        // Statistics tracking
        mutable Statistics m_statistics;

        /**
         * @brief Create cache key for model + properties combination
         */
        std::string CreateCacheKey(const ResourceLocation&                             modelPath,
                                   const std::unordered_map<std::string, std::string>& properties) const;

        /**
         * @brief Create the builtin block/cube model template
         */
        std::shared_ptr<resource::model::ModelResource> CreateBuiltinCubeModel();

        /**
         * @brief Load model from file via ResourceSubsystem
         */
        std::shared_ptr<resource::model::ModelResource> LoadModelFromFile(const ResourceLocation& location);

        /**
         * @brief Convert ResourceLocation to builtin key
         */
        std::string GetBuiltinKey(const ResourceLocation& location) const;
    };
}
