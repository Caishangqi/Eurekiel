#pragma once
#include "../Chunk/Chunk.hpp"
#include <memory>

#include "Engine/Registry/Core/IRegistrable.hpp"
#include "Engine/Resource/ResourceCommon.hpp"

namespace enigma::voxel::generation
{
    using namespace enigma::core;
    using namespace enigma::resource;
    using namespace enigma::voxel::chunk;

    /**
     * @brief Abstract base class for world generators
     *
     * World generators are responsible for populating chunks with terrain data.
     * They can be registered in the RegisterSubsystem and selected at runtime.
     *
     * Based on Minecraft's world generation system, this provides a flexible
     * interface for different terrain generation algorithms.
     */
    class Generator : public IRegistrable
    {
    private:
        std::string      m_registryName;
        std::string      m_namespace;
        ResourceLocation m_resourceLocation;

    protected:
        /**
         * @brief Protected constructor requiring name for registration
         */
        Generator(const std::string& registryName, const std::string& namespaceName = "engine")
            : m_registryName(registryName)
              , m_namespace(namespaceName)
              , m_resourceLocation(namespaceName, registryName)
        {
        }

    public:
        ~Generator() override = default;

        // IRegistrable interface implementation
        const std::string& GetRegistryName() const override { return m_registryName; }
        const std::string& GetNamespace() const override { return m_namespace; }

        /**
         * @brief Get the ResourceLocation for this generator
         */
        const ResourceLocation& GetResourceLocation() const { return m_resourceLocation; }

        /**
         * @brief Generate terrain data for a chunk
         *
         * This is the main generation method that subclasses must implement.
         * It should populate the given chunk with appropriate block data.
         *
         * @param chunk The chunk to generate terrain for
         * @param chunkX The chunk's X coordinate in world chunk space
         * @param chunkZ The chunk's Z coordinate in world chunk space
         * @param worldSeed The world generation seed for deterministic results
         */
        virtual void GenerateChunk(Chunk* chunk, int32_t chunkX, int32_t chunkZ, uint32_t worldSeed) = 0;

        /**
         * @brief Get the sea level for this generator
         *
         * Used by various systems that need to know the "normal" water level.
         * Default implementation returns 64, but can be overridden.
         */
        virtual int32_t GetSeaLevel() const { return 64; }

        /**
         * @brief Get the base height for terrain generation
         *
         * This represents the typical ground level before terrain variation.
         * Default implementation returns 50, but can be overridden.
         */
        virtual int32_t GetBaseHeight() const { return 50; }

        /**
         * @brief Get the maximum height this generator can produce
         *
         * Used for optimization and validation purposes.
         * Default implementation returns the chunk height.
         */
        virtual int32_t GetMaxHeight() const { return Chunk::CHUNK_SIZE_Z - 1; }

        /**
         * @brief Get the minimum height this generator can produce
         *
         * Used for optimization and validation purposes.
         * Default implementation returns 0.
         */
        virtual int32_t GetMinHeight() const { return 0; }

        /**
         * @brief Check if this generator supports a specific feature
         *
         * Can be used to query generator capabilities.
         * Default implementation returns false for all features.
         *
         * @param featureName The name of the feature to check
         * @return true if the feature is supported, false otherwise
         */
        virtual bool SupportsFeature(const std::string& featureName) const
        {
            UNUSED(featureName)
            return false;
        }

        /**
         * @brief Get generator-specific configuration
         *
         * Returns a string description of this generator's configuration.
         * Useful for debugging and display purposes.
         */
        virtual std::string GetConfigDescription() const = 0;

        /**
         * @brief Initialize the generator with specific settings
         *
         * Called when the generator is selected or configuration changes.
         * Default implementation does nothing.
         *
         * @param seed The world seed to use for generation
         * @return true if initialization succeeded, false otherwise
         */
        virtual bool Initialize(uint32_t seed)
        {
            UNUSED(seed)
            return true;
        }

        /**
         * @brief Cleanup generator resources
         *
         * Called when the generator is no longer needed.
         * Default implementation does nothing.
         */
        virtual void Cleanup()
        {
        }

        /**
         * @brief Get the generator's display name
         *
         * Human-readable name for UI display.
         * Default implementation returns the registry name.
         */
        virtual std::string GetDisplayName() const { return m_registryName; }

        /**
         * @brief Get the generator's description
         *
         * Human-readable description for UI display.
         * Default implementation returns a generic description.
         */
        virtual std::string GetDescription() const { return "World terrain generator"; }
    };
}
