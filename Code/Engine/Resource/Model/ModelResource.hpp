#pragma once
#include "../ResourceCommon.hpp"
#include "../ResourceMetadata.hpp"
#include "../../Core/Json.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Vec4.hpp"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <filesystem>

namespace enigma::resource::model
{
    using namespace enigma::resource;
    using namespace enigma::core;
    
    /**
     * @brief Represents a face of a model element
     */
    struct ModelFace
    {
        std::string texture;        // Texture variable reference (e.g., "#side")
        Vec4 uv = Vec4(0, 0, 16, 16); // UV coordinates in texture space
        int rotation = 0;           // Rotation in 90-degree increments (0, 90, 180, 270)
        std::optional<std::string> cullface; // Face to cull against ("up", "down", "north", etc.)
        bool tintindex = false;     // Whether to apply tint
        
        ModelFace() = default;
        ModelFace(const std::string& tex) : texture(tex) {}
        
        bool LoadFromJson(const JsonObject& json);
    };
    
    /**
     * @brief Represents a cuboid element in a model
     */
    struct ModelElement
    {
        Vec3 from = Vec3(0, 0, 0);  // Start position (0-16 scale)
        Vec3 to = Vec3(16, 16, 16); // End position (0-16 scale)
        std::map<std::string, ModelFace> faces; // Faces by direction ("up", "down", "north", etc.)
        
        // Optional rotation
        struct Rotation
        {
            Vec3 origin = Vec3(8, 8, 8); // Rotation origin
            std::string axis = "y";      // Rotation axis ("x", "y", "z")
            float angle = 0.0f;          // Rotation angle
            bool rescale = false;        // Whether to rescale after rotation
            
            bool LoadFromJson(const JsonObject& json);
        };
        std::optional<Rotation> rotation;
        
        bool shade = true;              // Whether to apply shading
        
        ModelElement() = default;
        bool LoadFromJson(const JsonObject& json);
    };
    
    /**
     * @brief Display settings for different contexts (gui, ground, etc.)
     */
    struct ModelDisplay
    {
        Vec3 rotation = Vec3(0, 0, 0);    // Rotation in degrees
        Vec3 translation = Vec3(0, 0, 0); // Translation
        Vec3 scale = Vec3(1, 1, 1);       // Scale
        
        bool LoadFromJson(const JsonObject& json);
    };
    
    /**
     * @brief Resource representing a Minecraft-style block/item model
     * 
     * Loads and parses JSON model files similar to Minecraft's format.
     */
    class ModelResource : public IResource
    {
    private:
        ResourceMetadata m_metadata;                                    // Resource metadata
        std::optional<ResourceLocation> m_parent;                    // Parent model to inherit from
        std::map<std::string, ResourceLocation> m_textures;          // Texture variable assignments
        std::vector<ModelElement> m_elements;                       // Model geometry
        std::map<std::string, ModelDisplay> m_display;              // Display settings
        bool m_ambientocclusion = true;                             // Whether to apply ambient occlusion
        std::string m_gui_light = "side";                           // GUI lighting mode
        
        // Resolved data (after parent inheritance)
        mutable bool m_resolved = false;
        mutable std::map<std::string, ResourceLocation> m_resolvedTextures;
        mutable std::vector<ModelElement> m_resolvedElements;
        
    public:
        ModelResource() = default;
        explicit ModelResource(const ResourceLocation& location) 
        {
            m_metadata.location = location;
            m_metadata.type = ResourceType::MODEL;
            m_metadata.state = ResourceState::NOT_LOADED;
        }
        
        // IResource interface
        const ResourceMetadata& GetMetadata() const override { return m_metadata; }
        ResourceType GetType() const override { return ResourceType::MODEL; }
        bool IsLoaded() const override { return !m_elements.empty() || m_parent.has_value(); }
        
        // Resource loading interface
        bool LoadFromFile(const std::filesystem::path& filePath);
        void Unload();
        
        
        // JSON loading
        bool LoadFromJson(const JsonObject& json);
        
        // Accessors (raw data)
        const std::optional<ResourceLocation>& GetParent() const { return m_parent; }
        const std::map<std::string, ResourceLocation>& GetTextures() const { return m_textures; }
        const std::vector<ModelElement>& GetElements() const { return m_elements; }
        const std::map<std::string, ModelDisplay>& GetDisplay() const { return m_display; }
        bool GetAmbientOcclusion() const { return m_ambientocclusion; }
        const std::string& GetGuiLight() const { return m_gui_light; }
        
        // Resolved accessors (with parent inheritance)
        const std::map<std::string, ResourceLocation>& GetResolvedTextures() const;
        const std::vector<ModelElement>& GetResolvedElements() const;
        
        // Texture resolution
        ResourceLocation ResolveTexture(const std::string& textureVariable) const;
        
        // Utility methods
        bool HasParent() const { return m_parent.has_value(); }
        bool HasElements() const { return !m_elements.empty(); }
        bool HasDisplay(const std::string& context) const { return m_display.find(context) != m_display.end(); }
        
        const ModelDisplay* GetDisplay(const std::string& context) const
        {
            auto it = m_display.find(context);
            return it != m_display.end() ? &it->second : nullptr;
        }
        
        // Factory methods
        static std::shared_ptr<ModelResource> Create(const ResourceLocation& location);
        static std::shared_ptr<ModelResource> LoadFromFile(const ResourceLocation& location, const std::filesystem::path& filePath);
        
    private:
        void ResolveInheritance() const;
        void ResolveParentRecursive(const ModelResource* parentModel) const;
    };
    
    using ModelResourcePtr = std::shared_ptr<ModelResource>;
}
