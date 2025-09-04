#pragma once
#include "../ResourceCommon.hpp"
#include "../../Math/Vec2.hpp"
#include "../../Math/IntVec2.hpp"
#include <string>
#include <vector>
#include <unordered_set>

namespace enigma::resource
{
    /**
     * AtlasSourceType - Defines how textures are collected for atlas building
     * Following Minecraft's atlas configuration patterns
     */
    enum class AtlasSourceType
    {
        DIRECTORY, // Collect all textures from a directory pattern (e.g., "textures/block/")
        SINGLE, // Single specific texture file
        FILTER // Filter-based collection with patterns
    };

    /**
     * AtlasSourceEntry - Configuration for a single source of textures
     * Similar to Minecraft's atlas JSON configuration
     */
    struct AtlasSourceEntry
    {
        AtlasSourceType          type = AtlasSourceType::DIRECTORY;
        std::string              source; // Directory path or file path
        std::string              prefix = ""; // Optional prefix for sprite names
        std::vector<std::string> namespaces; // Namespaces to include (empty = all)

        // For FILTER type
        std::vector<std::string> includePatterns;
        std::vector<std::string> excludePatterns;

        AtlasSourceEntry(AtlasSourceType sourceType, const std::string& sourcePath)
            : type(sourceType), source(sourcePath)
        {
        }
    };

    /**
     * AtlasSprite - Information about a sprite within an atlas
     * Contains UV coordinates, position, and metadata
     */
    struct AtlasSprite
    {
        ResourceLocation location; // Original resource location
        IntVec2          atlasPosition; // Position in atlas (pixels)
        IntVec2          size; // Size in pixels
        Vec2             uvMin; // UV coordinates (0.0-1.0)
        Vec2             uvMax; // UV coordinates (0.0-1.0)
        int              originalResolution; // Original texture resolution for validation
        int              atlasIndex; // Which atlas this sprite belongs to (for multi-atlas)

        AtlasSprite()
            : atlasPosition(IntVec2::ZERO)
              , size(IntVec2::ZERO)
              , uvMin(Vec2::ZERO)
              , uvMax(Vec2::ZERO)
              , originalResolution(0)
              , atlasIndex(0)
        {
        }

        AtlasSprite(const ResourceLocation& loc, IntVec2 pos, IntVec2 spriteSize, int resolution)
            : location(loc)
              , atlasPosition(pos)
              , size(spriteSize)
              , originalResolution(resolution)
              , atlasIndex(0)
        {
            // UV coordinates will be calculated by atlas builder
            uvMin = Vec2::ZERO;
            uvMax = Vec2::ZERO;
        }

        // Calculate UV coordinates based on atlas dimensions
        void CalculateUVCoordinates(IntVec2 atlasDimensions)
        {
            if (atlasDimensions.x > 0 && atlasDimensions.y > 0)
            {
                uvMin.x = static_cast<float>(atlasPosition.x) / static_cast<float>(atlasDimensions.x);
                uvMin.y = static_cast<float>(atlasPosition.y) / static_cast<float>(atlasDimensions.y);
                uvMax.x = static_cast<float>(atlasPosition.x + size.x) / static_cast<float>(atlasDimensions.x);
                uvMax.y = static_cast<float>(atlasPosition.y + size.y) / static_cast<float>(atlasDimensions.y);
            }
        }

        // Check if UV coordinates are valid
        bool HasValidUVs() const
        {
            return uvMin.x >= 0.0f && uvMin.y >= 0.0f && uvMax.x <= 1.0f && uvMax.y <= 1.0f &&
                uvMax.x > uvMin.x && uvMax.y > uvMin.y;
        }
    };

    /**
     * AtlasConfig - Configuration for atlas building
     * Similar to Minecraft's atlas JSON configuration with additional features
     */
    struct AtlasConfig
    {
        std::string                   name; // Atlas name (e.g., "blocks", "items")
        std::vector<AtlasSourceEntry> sources; // Sources of textures

        // CRITICAL: Resolution consistency (Minecraft constraint)
        int  requiredResolution = 16; // All textures must be this resolution
        bool autoScale          = true; // Auto-scale mismatched textures to required resolution
        bool rejectMismatched   = false; // Reject textures with wrong resolution instead of scaling

        // Atlas generation settings
        IntVec2 maxAtlasSize  = IntVec2(4096, 4096); // Maximum atlas size (GPU limit consideration)
        int     padding       = 0; // Padding between sprites (prevent bleeding)
        bool    allowRotation = false; // Allow sprite rotation for better packing (disabled for simplicity)

        // Export settings
        bool        exportPNG  = true; // Export atlas to PNG for debugging
        std::string exportPath = "debug/"; // Path for exported atlases

        // Performance settings
        bool generateMipmaps = false; // Generate mipmaps for atlas texture
        bool compressTexture = false; // Use texture compression

        // Default constructor
        AtlasConfig() = default;

        AtlasConfig(const std::string& atlasName) : name(atlasName)
        {
        }

        // Add a directory source
        void AddDirectorySource(const std::string& directory, const std::vector<std::string>& namespaces = {})
        {
            AtlasSourceEntry entry(AtlasSourceType::DIRECTORY, directory);
            entry.namespaces = namespaces;
            sources.push_back(entry);
        }

        // Add a single file source
        void AddSingleSource(const std::string& filePath)
        {
            sources.emplace_back(AtlasSourceType::SINGLE, filePath);
        }

        // Add a filter-based source
        void AddFilterSource(const std::vector<std::string>& includePatterns,
                             const std::vector<std::string>& excludePatterns = {})
        {
            AtlasSourceEntry entry(AtlasSourceType::FILTER, "");
            entry.includePatterns = includePatterns;
            entry.excludePatterns = excludePatterns;
            sources.push_back(entry);
        }

        // Validate configuration
        bool IsValid() const
        {
            return !name.empty() &&
                !sources.empty() &&
                requiredResolution > 0 &&
                maxAtlasSize.x > 0 && maxAtlasSize.y > 0 &&
                padding >= 0;
        }

        // Set resolution consistency mode
        void SetResolutionMode(int resolution, bool autoScaleMode, bool rejectMode = false)
        {
            requiredResolution = resolution;
            autoScale          = autoScaleMode;
            rejectMismatched   = rejectMode && !autoScaleMode; // Can't reject and scale at same time
        }
    };

    /**
     * AtlasStats - Statistics about atlas generation
     */
    struct AtlasStats
    {
        int    totalSprites      = 0; // Total number of sprites in atlas
        int    atlasWidth        = 0; // Final atlas width
        int    atlasHeight       = 0; // Final atlas height
        float  packingEfficiency = 0.0f; // Percentage of atlas actually used
        int    rejectedSprites   = 0; // Number of sprites rejected due to resolution mismatch
        int    scaledSprites     = 0; // Number of sprites that were scaled
        size_t atlasSizeBytes    = 0; // Final atlas size in bytes

        // Reset all stats
        void Reset()
        {
            totalSprites      = 0;
            atlasWidth        = 0;
            atlasHeight       = 0;
            packingEfficiency = 0.0f;
            rejectedSprites   = 0;
            scaledSprites     = 0;
            atlasSizeBytes    = 0;
        }

        // Calculate packing efficiency
        void CalculatePackingEfficiency(int usedPixels)
        {
            int totalPixels = atlasWidth * atlasHeight;
            if (totalPixels > 0)
            {
                packingEfficiency = static_cast<float>(usedPixels) / static_cast<float>(totalPixels) * 100.0f;
            }
        }
    };
}
