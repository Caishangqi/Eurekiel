#pragma once
#include "ChunkJob.hpp"
#include "Engine/Voxel/Generation/TerrainGenerator.hpp"

namespace enigma::voxel
{
    // Forward declaration
    class World;

    //-----------------------------------------------------------------------------------------------
    // Job for procedural chunk generation on ChunkGen threads
    // CPU-intensive work: noise generation, terrain shaping, block placement
    // [REFACTORED] No longer holds Chunk*, retrieves it via World::GetChunk() in Execute()
    //-----------------------------------------------------------------------------------------------
    class GenerateChunkJob : public ChunkJob
    {
    public:
        GenerateChunkJob(IntVec2 chunkCoords, World* world, TerrainGenerator* generator, uint32_t worldSeed)
            : ChunkJob(TaskTypeConstants::CHUNK_GEN, chunkCoords)
              , m_world(world)
              , m_generator(generator)
              , m_worldSeed(worldSeed)
        {
        }

        void Execute() override;

    private:
        World*            m_world; // [NEW] World instance to get Chunk via coordinates
        TerrainGenerator* m_generator; // TerrainGenerator instance (thread-safe, stateless)
        uint32_t          m_worldSeed; // World generation seed
    };
} // namespace enigma::voxel
