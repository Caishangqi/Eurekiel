#pragma once
#include "ChunkJob.hpp"
#include "Engine/Voxel/Generation/TerrainGenerator.hpp"

namespace enigma::voxel
{
    //-----------------------------------------------------------------------------------------------
    // Job for procedural chunk generation on ChunkGen threads
    // CPU-intensive work: noise generation, terrain shaping, block placement
    //-----------------------------------------------------------------------------------------------
    class GenerateChunkJob : public ChunkJob
    {
    public:
        GenerateChunkJob(IntVec2 chunkCoords, Chunk* chunk, TerrainGenerator* generator, uint32_t worldSeed)
            : ChunkJob(TaskTypeConstants::CHUNK_GEN, chunkCoords)
              , m_chunk(chunk)
              , m_generator(generator)
              , m_worldSeed(worldSeed)
        {
        }

        void Execute() override;

    private:
        Chunk*            m_chunk; // Target chunk (owned by main thread, only worker reads/writes blocks)
        TerrainGenerator* m_generator; // TerrainGenerator instance (thread-safe, stateless)
        uint32_t          m_worldSeed; // World generation seed
    };
} // namespace enigma::voxel
