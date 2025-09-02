#pragma once
#include "../Block/BlockState.hpp"
#include "../Block/BlockPos.hpp"
#include "ChunkMesh.hpp"
#include <array>
#include <memory>
#include <atomic>

#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec2.hpp"

namespace enigma::voxel::chunk
{
    using namespace enigma::voxel::block;

    /**
     * @brief Represents a 16x16x(height) section of the world
     * 
     * TODO: This class needs to be implemented with the following functionality:
     * 
     * CORE DATA STRUCTURE:
     * - Standard Minecraft-style 16x16 horizontal, full height vertical
     * - Block storage: 3D array or optimized palette system
     * - Coordinate system: Local chunk coordinates (0-15) and world coordinates
     * 
     * REQUIRED CONSTANTS:
     * - static constexpr int32_t CHUNK_SIZE = 16;     // Horizontal size
     * - static constexpr int32_t CHUNK_HEIGHT = 128;  // Vertical size (adjust as needed)
     * - static constexpr int32_t BLOCKS_PER_CHUNK = CHUNK_SIZE * CHUNK_SIZE * CHUNK_HEIGHT;
     * 
     * REQUIRED MEMBER VARIABLES:
     * - int32_t m_chunkX, m_chunkZ;                    // Chunk coordinates in world
     * - BlockState* m_blocks[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];  // Block storage
     * - std::unique_ptr<ChunkMesh> m_mesh;            // Compiled mesh for rendering
     * - bool m_isDirty = true;                        // Needs mesh rebuild
     * - bool m_isGenerated = false;                   // Has been world-generated
     * - bool m_isPopulated = false;                   // Has decorations/structures
     * - std::atomic<bool> m_isLoading{false};         // Currently being loaded/generated
     * 
     * REQUIRED METHODS:
     * 
     * Block Access:
     * - BlockState* GetBlock(int32_t x, int32_t y, int32_t z)  // Local coordinates
     * - void SetBlock(int32_t x, int32_t y, int32_t z, BlockState* state)
     * - BlockState* GetBlockWorld(const BlockPos& worldPos)    // World coordinates
     * - void SetBlockWorld(const BlockPos& worldPos, BlockState* state)
     * 
     * Coordinate Conversion:
     * - BlockPos LocalToWorld(int32_t x, int32_t y, int32_t z)
     * - bool WorldToLocal(const BlockPos& worldPos, int32_t& x, int32_t& y, int32_t& z)
     * - bool ContainsWorldPos(const BlockPos& worldPos)
     * 
     * Mesh Management:
     * - void MarkDirty()  // Mark chunk as needing mesh rebuild
     * - void RebuildMesh()  // Regenerate ChunkMesh from block data
     * - ChunkMesh* GetMesh() const  // Get mesh for rendering
     * - bool NeedsMeshRebuild() const  // Check if mesh needs rebuilding
     * 
     * Neighbor Access (for mesh generation):
     * - BlockState* GetNeighborBlock(int32_t x, int32_t y, int32_t z, Direction dir)
     * - void SetNeighborChunks(Chunk* north, Chunk* south, Chunk* east, Chunk* west)
     * 
     * State Management:
     * - bool IsGenerated() const
     * - void SetGenerated(bool generated)
     * - bool IsPopulated() const 
     * - void SetPopulated(bool populated)
     * - bool IsLoading() const
     * - void SetLoading(bool loading)
     * 
     * Utility:
     * - void Clear()  // Clear all blocks to air
     * - int32_t GetChunkX() const
     * - int32_t GetChunkZ() const
     * - BlockPos GetWorldPos() const  // Bottom corner world position
     * 
     * MESH BUILDING INTEGRATION:
     * - Should call ChunkMeshBuilder when mesh needs rebuilding
     * - Must handle face culling between adjacent chunks
     * - Should batch mesh updates to avoid rebuilding every block change
     * 
     * THREADING CONSIDERATIONS:
     * - Block access should be thread-safe during generation
     * - Mesh building can happen on background thread
     * - Loading state should be atomic for thread safety
     * 
     * MEMORY OPTIMIZATION:
     * - Consider palette system for repeated block states
     * - Empty chunks could be represented more efficiently
     * - Consider compression for saved chunk data
     */
    class Chunk
    {
        // TODO: Implement according to comments above
    public:
        static constexpr int32_t CHUNK_SIZE       = 16;
        static constexpr int32_t CHUNK_HEIGHT     = 128; // Adjust based on world height needs
        static constexpr int32_t BLOCKS_PER_CHUNK = CHUNK_SIZE * CHUNK_SIZE * CHUNK_HEIGHT;

        Chunk(IntVec2 chunkCoords);
        ~Chunk();

        // Block Access - PUBLIC for World class access
        BlockState GetBlock(int32_t x, int32_t y, int32_t z); // Local coordinates
        void       SetBlock(int32_t x, int32_t y, int32_t z, BlockState state);
        BlockState GetBlockWorld(const BlockPos& worldPos); // World coordinates
        void       SetBlockWorld(const BlockPos& worldPos, BlockState state);

        // Coordinate Conversion - PUBLIC for World class
        BlockPos LocalToWorld(int32_t x, int32_t y, int32_t z);
        bool     WorldToLocal(const BlockPos& worldPos, int32_t& x, int32_t& y, int32_t& z);
        bool     ContainsWorldPos(const BlockPos& worldPos);

        // Mesh Management - PUBLIC for rendering system
        void       MarkDirty(); // Mark chunk as needing mesh rebuild
        void       RebuildMesh(); // Regenerate ChunkMesh from block data
        ChunkMesh* GetMesh() const; // Get mesh for rendering
        bool       NeedsMeshRebuild() const; // Check if mesh needs rebuilding

        // State Management - PUBLIC for World management
        bool IsGenerated() const { return m_isGenerated; }
        void SetGenerated(bool generated) { m_isGenerated = generated; }
        bool IsPopulated() const { return m_isPopulated; }
        void SetPopulated(bool populated) { m_isPopulated = populated; }
        bool IsLoading() const { return m_isLoading.load(); }
        void SetLoading(bool loading) { m_isLoading.store(loading); }

        //Update and Management:
        void Update(float deltaTime); // Update chunk
        void DebugDraw(IRenderer* renderer);

        // Utility - PUBLIC for World class
        void     Clear(); // Clear all blocks to air
        IntVec2  GetChunkCoords() const { return m_chunkCoords; }
        int32_t  GetChunkX() const { return m_chunkCoords.x; }
        int32_t  GetChunkZ() const { return m_chunkCoords.y; }
        BlockPos GetWorldPos() const; // Bottom corner world position

    private:
        IntVec2                    m_chunkCoords = IntVec2(0, 0);
        std::vector<BlockState>    m_blocks; // Block storage  
        std::unique_ptr<ChunkMesh> m_mesh; // Compiled mesh for rendering
        bool                       m_isDirty     = true; // Needs mesh rebuild
        bool                       m_isGenerated = false; // Has been world-generated
        bool                       m_isPopulated = false; // Has decorations/structures
        std::atomic<bool>          m_isLoading{false}; // Currently being loaded/generated

        /// Debug Drawing
        AABB3 m_chunkBounding; // Need generated based on chunk coords
    };
}
