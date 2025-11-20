/**
 * @file SpriteAtlas.hpp
 * @brief SpriteAtlas class - Sprite atlas management
 *
 * Teaching Focus:
 * 1. Atlas technique reduces Draw Calls
 * 2. UV coordinate slicing and management
 * 3. Resource sharing and lifetime management
 *
 * Architecture Design:
 * - Holds single D12Texture, manages UV regions for multiple Sprites
 * - Uses SpriteData to store metadata for each Sprite
 * - Provides interface to find and create Sprites by name
 */

#pragma once

#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/IntVec2.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

class Image;

namespace enigma::graphic
{
    /**
     * @brief SpriteData helper struct
     * @details Internal data structure for SpriteAtlas, stores only Sprite metadata
     *
     * Design Notes:
     * - Does not hold texture pointer, textures managed uniformly by SpriteAtlas
     * - GetSprite method dynamically creates Sprite objects based on SpriteData
     * - POD struct design, all members have public access
     *
     * Core Members:
     * - name: Sprite name (for lookup)
     * - uvBounds: UV coordinate bounds (normalized coordinates 0.0-1.0)
     * - pivot: Anchor point (normalized coordinates, default 0.5, 0.5 center)
     * - dimensions: Pixel dimensions (width, height)
     */
    struct SpriteData
    {
        std::string name; ///< Sprite name
        AABB2       uvBounds; ///< UV coordinate bounds
        Vec2        pivot; ///< Anchor point (default 0.5, 0.5)
        IntVec2     dimensions; ///< Pixel dimensions

        /**
         * @brief Default constructor
         */
        SpriteData() = default;

        /**
         * @brief Full constructor
         * @param spriteName Sprite name
         * @param uv UV coordinate bounds
         * @param pivotPoint Anchor point (default center)
         * @param dims Pixel dimensions (default 0, 0)
         */
        SpriteData(
            const std::string& spriteName
            , const AABB2&     uv
            , const Vec2&      pivotPoint = Vec2(0.5f, 0.5f)
            , const IntVec2&   dims       = IntVec2(0, 0)
        )
            : name(spriteName)
              , uvBounds(uv)
              , pivot(pivotPoint)
              , dimensions(dims)
        {
        }

        /**
         * @brief Equality comparison operator (based on name)
         * @param other Another SpriteData to compare
         * @return Returns true if names are identical
         */
        bool operator==(const SpriteData& other) const
        {
            return name == other.name;
        }

        /**
         * @brief Inequality comparison operator (based on name)
         * @param other Another SpriteData to compare
         * @return Returns true if names are different
         */
        bool operator!=(const SpriteData& other) const
        {
            return name != other.name;
        }
    };

    // ==================== Forward Declarations ====================
    class D12Texture;
    class Sprite;

    /**
     * @brief Packing mode enumeration
     */
    enum class PackingMode
    {
        SimpleGrid, ///< Simple grid layout
        MaxRects, ///< Tight packing algorithm (Phase 2)
        Auto ///< Auto select
    };

    /**
     * @brief SpriteAtlas class - Sprite atlas management
     * @details Manages single texture atlas and UV regions for multiple Sprites
     *
     * Core Features:
     * 1. Grid slicing - Slice from single image by grid
     * 2. Multi-image packing - Pack multiple images into one atlas
     * 3. Sprite access - Get Sprite by name or index
     *
     * Usage Example:
     * @code
     *   // Grid slicing
     *   SpriteAtlas atlas("PlayerAtlas");
     *   atlas.BuildFromGrid("player_sheet.png", IntVec2(4, 4), "player");
     *   Sprite sprite = atlas.GetSprite("player_0");
     *
     *   // Multi-image packing
     *   SpriteAtlas atlas2("ItemAtlas");
     *   atlas2.AddSprite("sword", "sword.png");
     *   atlas2.AddSprite("shield", "shield.png");
     *   atlas2.PackAtlas();
     * @endcode
     */
    class SpriteAtlas
    {
    public:
        // ==================== 构造函数 ====================

        /**
         * @brief Default constructor
         */
        SpriteAtlas() = default;

        /**
         * @brief constructor
         * @param atlasName Atlas name
         */
        explicit SpriteAtlas(const std::string& atlasName);

        // ==================== Grid 切割接口 ====================

        /**
         * @brief Cut by grid from image file
         * @param imagePath image file path
         * @param gridLayout grid layout (number of columns, number of rows)
         * @param spritePrefix Sprite name prefix (default "sprite")
         */
        void BuildFromGrid(
            const char*    imagePath,
            const IntVec2& gridLayout,
            const char*    spritePrefix = "sprite"
        );

        /**
         * @brief Cut by grid from Image object
         * @param image image object
         * @param gridLayout grid layout (number of columns, number of rows)
         * @param spritePrefix Sprite name prefix
         */
        void BuildFromGrid(
            const Image&   image,
            const IntVec2& gridLayout,
            const char*    spritePrefix
        );

        /**
         * @brief Cut by grid from image file (custom name)
         * @param imagePath image file path
         * @param gridLayout grid layout (number of columns, number of rows)
         * @param customNames Custom Sprite name list
         */
        void BuildFromGrid(
            const char*                     imagePath,
            const IntVec2&                  gridLayout,
            const std::vector<std::string>& customNames
        );

        // ==================== Multi-image packaging interface ====================

        /**
         * @brief Add Sprite (from file path)
         * @param spriteName Sprite name
         * @param imagePath image file path
         * @param pivot anchor point (default center point)
         */
        void AddSprite(
            const std::string& spriteName,
            const char*        imagePath,
            const Vec2&        pivot = Vec2(0.5f, 0.5f)
        );

        /**
         * @brief Add Sprite (from Image object)
         * @param spriteName Sprite name
         * @param image image object
         * @param pivot anchor point (default center point)
         */
        void AddSprite(
            const std::string& spriteName,
            const Image&       image,
            const Vec2&        pivot = Vec2(0.5f, 0.5f)
        );

        /**
         * @brief packaged photo album
         * @param mode packaging mode (default SimpleGrid)
         */
        void PackAtlas(PackingMode mode = PackingMode::SimpleGrid);

        // ==================== Sprite access interface ====================

        /**
         * @brief Get Sprite by name
         * @param name Sprite name
         * @return Sprite object
         */
        Sprite GetSprite(const std::string& name) const;

        /**
         * @brief Get Sprite by index
         * @param index Sprite index
         * @return Sprite object
         */
        Sprite GetSprite(int index) const;

        /**
         * @brief Check whether there is a Sprite with the specified name
         * @param name Sprite name
         * @return exists and returns true
         */
        bool HasSprite(const std::string& name) const;

        // ==================== Query interface ====================

        /**
         * @brief Get the number of Sprites
         * @return Sprite quantity
         */
        int GetSpriteCount() const { return static_cast<int>(m_spriteData.size()); }

        /**
         * @brief Get all Sprite names
         * @return Sprite name list
         */
        std::vector<std::string> GetAllSpriteNames() const;

        // ==================== Management interface ====================

        /**
         * @brief Remove the Sprite with the specified name
         * @param name Sprite name
         * @return Returns true successfully
         */
        bool RemoveSprite(const std::string& name);

        /**
         * @brief Clear all Sprites
         */
        void Clear();

        /**
         * @brief Set the maximum album size
         * @param maxSize maximum size
         */
        void SetMaxAtlasSize(const IntVec2& maxSize) { m_maxAtlasSize = maxSize; }

    private:
        // ==================== 私有辅助方法 ====================

        /**
         * @brief Calculate UV coordinates
         * @param row row index
         * @param col column index
         * @param gridLayout grid layout
         * @param textureDimensions texture dimensions
         * @return UV border
         */
        AABB2 CalculateUVCoordinates(
            int            row,
            int            col,
            const IntVec2& gridLayout,
            const IntVec2& textureDimensions
        ) const;

        /**
         * @brief Simple grid packaging
         */
        void PackSimpleGrid();

        /**
         * @brief MaxRects packing algorithm (Phase 2)
         */
        void PackMaxRects();

        // ==================== Data members ====================
        std::string                             m_atlasName; ///<Atlas name
        std::shared_ptr<D12Texture>             m_atlasTexture; ///< atlas texture
        std::vector<SpriteData>                 m_spriteData; ///< Sprite data list
        std::unordered_map<std::string, size_t> m_nameToIndex; ///< name to index mapping
        IntVec2                                 m_atlasDimensions; ///<Atlas Dimensions
        IntVec2                                 m_maxAtlasSize; ///< Maximum atlas size
        bool                                    m_isPacked; ///< Whether it has been packed
        std::vector<std::unique_ptr<Image>>     m_pendingImages; ///< Images to be packed
    };
} // namespace enigma::graphic
