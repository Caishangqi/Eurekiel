#pragma once
#include "../ResourceCommon.hpp"
#include "../ResourceMetadata.hpp"
#include "AtlasConfig.hpp"
#include "ImageResource.hpp"
#include "../../Core/Image.hpp"
#include "../../Renderer/Texture.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace enigma::resource
{
    /**
     * TextureAtlas - Single atlas management class
     * Handles atlas building from ImageResource collection, sprite positioning/UV calculation,
     * and PNG export functionality. Integrates with existing Texture class for rendering.
     */
    class TextureAtlas : public IResource
    {
    public:
        // Constructor with configuration
        explicit TextureAtlas(const AtlasConfig& config);

        // Destructor
        virtual ~TextureAtlas() override;

        // IResource interface implementation
        const ResourceMetadata& GetMetadata() const override;
        ResourceType            GetType() const override;
        bool                    IsLoaded() const override;
        const void*             GetRawData() const override;
        size_t                  GetRawDataSize() const override;

        // Atlas building methods
        bool BuildAtlas(const std::vector<std::shared_ptr<ImageResource>>& images);
        void ClearAtlas();

        // Sprite access methods
        const AtlasSprite*              FindSprite(const ResourceLocation& location) const;
        const std::vector<AtlasSprite>& GetAllSprites() const;

        // Atlas properties
        IntVec2            GetAtlasDimensions() const;
        const AtlasConfig& GetConfig() const;
        const AtlasStats&  GetStats() const;

        // Texture integration
        const Image* GetAtlasImage() const;
        Texture*     GetAtlasTexture() const; // For rendering integration (will be created by renderer)

        // PNG export functionality
        bool ExportToPNG(const std::string& filepath) const;

        // Debug methods
        void        PrintAtlasInfo() const;
        std::string GetDebugString() const;

        // Compatibility methods for atlas system
        ResourceLocation GetResourceLocation() const;
        void             Unload();

    private:
        // Atlas building helpers
        bool    ValidateImages(const std::vector<std::shared_ptr<ImageResource>>& images);
        bool    PackSprites(const std::vector<std::shared_ptr<ImageResource>>& images);
        void    ScaleImageIfNeeded(const Image& source, Image& destination, int targetResolution) const;
        void    CopyImageToAtlas(const Image& source, const IntVec2& position);
        void    CalculateAllUVCoordinates();
        IntVec2 FindBestAtlasSize(int totalSprites, int spriteResolution) const;

        // Simple rectangle packing (can be upgraded to more sophisticated algorithms later)
        struct PackingRect
        {
            IntVec2 position;
            IntVec2 size;
            bool    occupied = false;

            PackingRect(IntVec2 pos, IntVec2 s) : position(pos), size(s)
            {
            }
        };

        bool TryPackSprite(IntVec2 spriteSize, IntVec2& outPosition);
        void InitializePackingGrid(IntVec2 atlasDimensions, int spriteSize);

    private:
        AtlasConfig      m_config; // Atlas configuration
        ResourceMetadata m_metadata; // Resource metadata
        ResourceLocation m_location; // Atlas resource location

        // Atlas data
        std::unique_ptr<Image>                       m_atlasImage; // Combined atlas image
        std::vector<AtlasSprite>                     m_sprites; // All sprites in atlas
        std::unordered_map<ResourceLocation, size_t> m_spriteLocationMap; // Quick lookup by ResourceLocation

        // Atlas metadata
        IntVec2    m_atlasDimensions; // Current atlas size
        AtlasStats m_stats; // Generation statistics
        bool       m_isBuilt; // Whether atlas has been built

        // Packing data (for simple grid-based packing)
        std::vector<std::vector<bool>> m_packingGrid; // Grid for sprite placement
        int                            m_gridResolution; // Resolution of each grid cell

        // Texture integration (managed externally by renderer)
        mutable Texture* m_atlasTexture; // Rendered texture (created by renderer)
    };
}
