#pragma once
#include "../ResourceCommon.hpp"
#include "../ResourceMetadata.hpp"
#include "../../Core/Json.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace enigma::resource::blockstate
{
    using namespace enigma::resource;
    using namespace enigma::core;
    
    /**
     * @brief Represents a model variant for a specific blockstate
     */
    struct BlockStateVariant
    {
        ResourceLocation model;      // Model to use (e.g., "minecraft:block/stone")
        int x = 0;                  // X rotation (0, 90, 180, 270)
        int y = 0;                  // Y rotation (0, 90, 180, 270)
        bool uvlock = false;        // UV lock for rotations
        int weight = 1;             // Weight for random selection
        
        BlockStateVariant() = default;
        BlockStateVariant(const ResourceLocation& modelPath) : model(modelPath) {}
        BlockStateVariant(const std::string& modelPath) : model(ResourceLocation(modelPath)) {}
        
        bool LoadFromJson(const JsonObject& json);
    };
    
    /**
     * @brief Condition for multipart blockstate selection
     */
    struct MultipartCondition
    {
        std::map<std::string, std::string> properties; // Property conditions (e.g., "facing": "north")
        
        bool LoadFromJson(const JsonObject& json);
        bool Matches(const std::map<std::string, std::string>& blockProperties) const;
    };
    
    /**
     * @brief A single case in a multipart blockstate
     */
    struct MultipartCase
    {
        std::vector<MultipartCondition> when;  // Conditions (OR logic between conditions)
        std::vector<BlockStateVariant> apply;  // Models to apply when conditions match
        
        bool LoadFromJson(const JsonObject& json);
        bool ShouldApply(const std::map<std::string, std::string>& blockProperties) const;
    };
    
    /**
     * @brief Resource representing a blockstate definition
     * 
     * Loads and parses blockstate JSON files that define how block properties
     * map to models (similar to Minecraft's blockstate system).
     */
    class BlockStateDefinition : public IResource
    {
    private:
        ResourceMetadata m_metadata;                                    // Resource metadata
        
        // Variants mode (simple property mapping)
        std::map<std::string, std::vector<BlockStateVariant>> m_variants;
        
        // Multipart mode (complex conditional logic)
        std::vector<MultipartCase> m_multipart;
        
        bool m_isMultipart = false;
        
    public:
        BlockStateDefinition() = default;
        explicit BlockStateDefinition(const ResourceLocation& location) 
        {
            m_metadata.location = location;
            m_metadata.type = ResourceType::BLOCKSTATE;
            m_metadata.state = ResourceState::NOT_LOADED;
        }
        
        // IResource interface
        const ResourceMetadata& GetMetadata() const override { return m_metadata; }
        ResourceType GetType() const override { return ResourceType::BLOCKSTATE; }
        bool IsLoaded() const override { return !m_variants.empty() || !m_multipart.empty(); }
        
        // Resource loading interface
        bool LoadFromFile(const std::filesystem::path& filePath);
        void Unload();
        
        // JSON loading
        bool LoadFromJson(const JsonObject& json);
        
        // Mode checking
        bool IsMultipart() const { return m_isMultipart; }
        bool IsVariants() const { return !m_isMultipart; }
        
        // Variants mode accessors
        const std::map<std::string, std::vector<BlockStateVariant>>& GetVariants() const { return m_variants; }
        
        /**
         * @brief Get variants for a specific property combination
         * @param propertyString String like "facing=north,powered=true"
         */
        const std::vector<BlockStateVariant>* GetVariants(const std::string& propertyString) const;
        
        /**
         * @brief Get variants for property map
         */
        const std::vector<BlockStateVariant>* GetVariants(const std::map<std::string, std::string>& properties) const;
        
        // Multipart mode accessors
        const std::vector<MultipartCase>& GetMultipart() const { return m_multipart; }
        
        /**
         * @brief Get all applicable variants for multipart blockstate
         */
        std::vector<BlockStateVariant> GetApplicableVariants(const std::map<std::string, std::string>& properties) const;
        
        // Utility methods
        bool HasVariant(const std::string& propertyString) const;
        bool HasDefaultVariant() const { return HasVariant(""); }
        
        const std::vector<BlockStateVariant>* GetDefaultVariants() const { return GetVariants(""); }
        
        /**
         * @brief Get all possible property combinations for variants mode
         */
        std::vector<std::string> GetAllVariantKeys() const;
        
        // Factory methods
        static std::shared_ptr<BlockStateDefinition> Create(const ResourceLocation& location);
        static std::shared_ptr<BlockStateDefinition> LoadFromFile(const ResourceLocation& location, const std::filesystem::path& filePath);
        
    private:
        std::string MapToPropertyString(const std::map<std::string, std::string>& properties) const;
    };
    
    using BlockStateDefinitionPtr = std::shared_ptr<BlockStateDefinition>;
}
