#pragma once
#include "Engine/Core/SubsystemManager.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Resource/ResourceCommon.hpp"
#include "Engine/Renderer/Model/RenderMesh.hpp"
#include <memory>
#include <string>
#include <unordered_map>

#include "Engine/Core/LogCategory/LogCategory.hpp"
DECLARE_LOG_CATEGORY_EXTERN(LogModel)

namespace enigma::renderer::model
{
    class IModelCompiler;
}

namespace enigma::renderer::model
{
}

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
    using namespace enigma::resource::model;
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
         * @brief Compile a model resource into a renderable mesh
         * Uses caching to avoid recompilation of identical models
         */
        std::shared_ptr<RenderMesh> CompileModel(const ResourceLocation& modelPath);

        /**
         * @brief Get cached compiled mesh by key
         */
        std::shared_ptr<RenderMesh> GetCompiledMesh(const std::string& cacheKey);

        std::shared_ptr<IModelCompiler> RegisterCompiler(const std::string& templateName, std::shared_ptr<IModelCompiler> compiler);

        /**
         * @brief Clear all cached meshes (for resource reload)
         */
        void ClearCompiledCache();

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

        /**
         * @brief Compile models for all registered blocks
         * 
         * This method should be called after all blocks are registered to compile
         * the models for all block states. Follows the Minecraft Forge pattern where
         * model compilation happens automatically in the engine after registration.
         */
        void CompileAllBlockModels();

    private:
        // Dependencies on other subsystems
        ResourceSubsystem* m_resourceSubsystem = nullptr;

        // Compiled mesh cache (key = model location + properties hash)
        std::unordered_map<std::string, std::shared_ptr<RenderMesh>> m_compiledMeshCache;

        // Compilers for different model templates
        std::unordered_map<std::string, std::shared_ptr<IModelCompiler>> m_compilers;

        // Statistics tracking
        mutable Statistics m_statistics;

        /**
         * @brief Create cache key for model + properties combination
         */
        std::string CreateCacheKey(const ResourceLocation&                             modelPath,
                                   const std::unordered_map<std::string, std::string>& properties) const;

        void RegisterCompilers();

        std::shared_ptr<IModelCompiler> GetCompiler(const std::string& identifier);

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
