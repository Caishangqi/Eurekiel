#pragma once
#include "TerrainGenerator.hpp"
#include "../NoiseGenerator/RawNoiseGenerator.hpp"
#include "Engine/Math/IntVec2.hpp"
#include <memory>
#include <unordered_map>

namespace enigma::voxel
{
    /**
     * @brief Abstract base class for tree generation systems
     *
     * TreeGenerator provides core algorithms for tree placement using noise-based
     * local maximum detection. It is designed to work with TerrainGenerator to
     * place trees on generated terrain.
     *
     * Key Features:
     * - Noise-based tree placement using RawNoiseGenerator
     * - Local maximum detection for natural tree distribution
     * - Expanded chunk boundary calculation for cross-chunk tree generation
     * - Noise caching for performance optimization
     *
     * Based on Professor Squirrel's tree generation algorithm (conversation-0.txt:46)
     */
    class TreeGenerator
    {
    public:
        // Maximum tree radius for boundary expansion calculation
        static constexpr int MAX_TREE_RADIUS = 10;

    protected:
        // World seed for deterministic generation
        uint32_t m_worldSeed;

        // Noise generators for tree placement
        std::unique_ptr<RawNoiseGenerator> m_treeNoise; // Tree placement noise
        std::unique_ptr<RawNoiseGenerator> m_treeSizeNoise; // Tree size variation
        std::unique_ptr<RawNoiseGenerator> m_treeRotationNoise; // Tree rotation variation

        // Noise cache for performance optimization
        mutable std::unordered_map<IntVec2, float> m_treeNoiseCache;

        // Reference to terrain generator for ground height queries
        const TerrainGenerator* m_terrainGenerator;

    public:
        /**
         * @brief Constructor
         * @param worldSeed Seed for world generation
         * @param terrainGenerator Reference to terrain generator for ground height queries
         */
        explicit TreeGenerator(uint32_t worldSeed, const TerrainGenerator* terrainGenerator);

        /**
         * @brief Virtual destructor
         */
        virtual ~TreeGenerator() = default;

        /**
         * @brief Generate trees for a chunk
         * 
         * This method should be implemented by subclasses to place trees
         * in the given chunk based on biome, terrain, and noise values.
         * 
         * @param chunk Chunk to generate trees in
         * @param chunkX Chunk X coordinate
         * @param chunkY Chunk Y coordinate (Z in Minecraft terms)
         * @return true if generation succeeded
         */
        virtual bool GenerateTrees(Chunk* chunk, int32_t chunkX, int32_t chunkY) = 0;

    protected:
        /**
         * @brief Sample tree placement noise at position
         * 
         * Uses caching for performance optimization.
         * 
         * @param globalX World X coordinate
         * @param globalY World Y coordinate (Z in Minecraft terms)
         * @return Noise value in range [0, 1]
         */
        float SampleTreeNoise(int globalX, int globalY) const;

        /**
         * @brief Sample tree size noise at position
         * 
         * @param globalX World X coordinate
         * @param globalY World Y coordinate (Z in Minecraft terms)
         * @return Noise value in range [0, 1]
         */
        float SampleTreeSizeNoise(int globalX, int globalY) const;

        /**
         * @brief Sample tree rotation noise at position
         * 
         * @param globalX World X coordinate
         * @param globalY World Y coordinate (Z in Minecraft terms)
         * @return Noise value in range [0, 1]
         */
        float SampleTreeRotationNoise(int globalX, int globalY) const;

        /**
         * @brief Check if a position is a local maximum in tree noise
         * 
         * Checks 3x3 neighborhood to determine if the center position
         * has the highest noise value. This creates natural tree distribution.
         * 
         * Algorithm from Professor Squirrel (conversation-2.txt:20)
         * 
         * @param globalX World X coordinate
         * @param globalY World Y coordinate (Z in Minecraft terms)
         * @param noiseValue Noise value at center position
         * @return true if center is local maximum
         */
        bool IsLocalMaximum(int globalX, int globalY, float noiseValue) const;

        /**
         * @brief Calculate expanded chunk boundaries for tree generation
         * 
         * Trees can extend beyond chunk boundaries, so we need to check
         * neighboring positions when generating trees. This method calculates
         * the expanded search area.
         * 
         * Formula from Professor Squirrel (conversation-2.txt:20):
         * expandedMinX = chunkX * 16 - (MAX_TREE_RADIUS + 1)
         * expandedMaxX = (chunkX + 1) * 16 + (MAX_TREE_RADIUS + 1)
         * 
         * @param chunkX Chunk X coordinate
         * @param chunkY Chunk Y coordinate
         * @param outMinX Output: Expanded minimum X
         * @param outMaxX Output: Expanded maximum X
         * @param outMinY Output: Expanded minimum Y
         * @param outMaxY Output: Expanded maximum Y
         */
        void CalculateExpandedBounds(int32_t chunkX, int32_t chunkY,
                                     int&    outMinX, int&   outMaxX,
                                     int&    outMinY, int&   outMaxY) const;

        /**
         * @brief Get ground height at specific world position
         * 
         * Delegates to terrain generator's GetGroundHeightAt method.
         * 
         * @param globalX World X coordinate
         * @param globalY World Y coordinate (Z in Minecraft terms)
         * @return Z coordinate of the highest solid block
         */
        int GetGroundHeightAt(int globalX, int globalY) const;

        /**
         * @brief Clear noise cache
         * 
         * Should be called when switching to a new chunk to prevent
         * cache from growing indefinitely.
         */
        void ClearNoiseCache();
    };
}
