#include "TreeGenerator.hpp"
#include "../Chunk/Chunk.hpp"
#include <cmath>

namespace enigma::voxel
{
    TreeGenerator::TreeGenerator(uint32_t worldSeed, const TerrainGenerator* terrainGenerator)
        : m_worldSeed(worldSeed)
          , m_terrainGenerator(terrainGenerator)
    {
        // Initialize noise generators with offset seeds
        // Using RawNoiseGenerator (not Perlin) as specified by Professor (conversation-0.txt:46)
        // Output range: [0, 1] (useNegOneToOne = false)
        m_treeNoise         = std::make_unique<RawNoiseGenerator>(worldSeed + 1000, false);
        m_treeSizeNoise     = std::make_unique<RawNoiseGenerator>(worldSeed + 2000, false);
        m_treeRotationNoise = std::make_unique<RawNoiseGenerator>(worldSeed + 3000, false);
    }

    float TreeGenerator::SampleTreeNoise(int globalX, int globalY) const
    {
        // Check cache first
        IntVec2 key(globalX, globalY);
        auto    it = m_treeNoiseCache.find(key);
        if (it != m_treeNoiseCache.end())
        {
            return it->second;
        }

        // Sample noise and cache result
        float noiseValue      = m_treeNoise->Sample2D(static_cast<float>(globalX), static_cast<float>(globalY));
        m_treeNoiseCache[key] = noiseValue;
        return noiseValue;
    }

    float TreeGenerator::SampleTreeSizeNoise(int globalX, int globalY) const
    {
        return m_treeSizeNoise->Sample2D(static_cast<float>(globalX), static_cast<float>(globalY));
    }

    float TreeGenerator::SampleTreeRotationNoise(int globalX, int globalY) const
    {
        return m_treeRotationNoise->Sample2D(static_cast<float>(globalX), static_cast<float>(globalY));
    }

    bool TreeGenerator::IsLocalMaximum(int globalX, int globalY, float noiseValue) const
    {
        // Check 3x3 neighborhood (8 neighbors)
        // If any neighbor has noise >= center, center is not a local maximum
        for (int dx = -1; dx <= 1; dx++)
        {
            for (int dy = -1; dy <= 1; dy++)
            {
                // Skip center position
                if (dx == 0 && dy == 0)
                    continue;

                // Sample neighbor noise
                float neighborNoise = SampleTreeNoise(globalX + dx, globalY + dy);

                // If neighbor is >= center, center is not a local maximum
                if (neighborNoise >= noiseValue)
                    return false;
            }
        }

        return true;
    }

    void TreeGenerator::CalculateExpandedBounds(int32_t chunkX, int32_t chunkY,
                                                int&    outMinX, int&   outMaxX,
                                                int&    outMinY, int&   outMaxY) const
    {
        // Formula from Professor Squirrel (conversation-2.txt:20)
        // Trees can extend beyond chunk boundaries, so we need to check
        // neighboring positions when generating trees.
        //
        // For a chunk at (chunkX, chunkY):
        // - Chunk occupies [chunkX * 16, (chunkX + 1) * 16) in X
        // - Chunk occupies [chunkY * 16, (chunkY + 1) * 16) in Y
        //
        // Expanded bounds include MAX_TREE_RADIUS + 1 on each side:
        // - expandedMinX = chunkX * 16 - (MAX_TREE_RADIUS + 1)
        // - expandedMaxX = (chunkX + 1) * 16 + (MAX_TREE_RADIUS + 1)
        //
        // Example: For chunk (0, 0) with MAX_TREE_RADIUS = 10:
        // - expandedMinX = 0 * 16 - 11 = -11
        // - expandedMaxX = 1 * 16 + 11 = 27
        // - Range: [-11, 27] (39 blocks wide)

        const int expansion = MAX_TREE_RADIUS + 1;

        outMinX = chunkX * Chunk::CHUNK_SIZE_X - expansion;
        outMaxX = (chunkX + 1) * Chunk::CHUNK_SIZE_X + expansion;
        outMinY = chunkY * Chunk::CHUNK_SIZE_Y - expansion;
        outMaxY = (chunkY + 1) * Chunk::CHUNK_SIZE_Y + expansion;
    }

    int TreeGenerator::GetGroundHeightAt(int globalX, int globalY) const
    {
        if (m_terrainGenerator)
        {
            return m_terrainGenerator->GetGroundHeightAt(globalX, globalY);
        }
        return 64; // Fallback to sea level if no terrain generator
    }

    void TreeGenerator::ClearNoiseCache()
    {
        m_treeNoiseCache.clear();
    }
}
