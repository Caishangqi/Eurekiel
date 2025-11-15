#pragma once
#include "../Block/BlockState.hpp"
#include "../Block/BlockPos.hpp"
#include "ChunkMesh.hpp"
#include "ChunkSerializationInterfaces.hpp"
#include "ChunkState.hpp"
#include <array>
#include <memory>
#include <atomic>

#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec2.hpp"

namespace enigma::voxel
{
    class World; // Forward declaration for dependency

    /**
     * @brief Represents a 16x16x256 section of the voxel world (Modified 2025-11-02: Increased from 128 to 256)
     *
     * ARCHITECTURE IMPLEMENTATION:
     *
     * CORE DATA STRUCTURE:
     * - Standard Minecraft-style 16x16 horizontal, 256 blocks vertical (Modified 2025-11-02)
     * - Block storage: Flat array with bit-optimized indexing
     * - Coordinate system: Local chunk coordinates (0-15) and world coordinates
     *
     * CONSTANTS:
     * - static constexpr int32_t CHUNK_SIZE_X/Y = 16;  // Horizontal size
     * - static constexpr int32_t CHUNK_SIZE_Z = 256;   // Vertical size (Modified 2025-11-02)
     * - static constexpr int32_t TOTAL_BLOCKS = 16 * 16 * 256 = 65536 (Modified 2025-11-02)
     *
     * MEMBER VARIABLES:
     * - IntVec2 m_chunkCoords;                         // Chunk coordinates (X, Y)
     * - BlockState* m_blocks[TOTAL_BLOCKS];            // Flat block storage
     * - std::unique_ptr<ChunkMesh> m_mesh;             // Compiled mesh for rendering
     * - bool m_isDirty = true;                         // Needs mesh rebuild
     * - bool m_isModified = false;                     // Has unsaved changes
     * - bool m_playerModified = false;                 // Modified by player (save strategy)
     * - std::atomic<ChunkState> m_chunkState;          // Lifecycle state (Inactive/Loading/Generating/Active/etc)
     *
     * IMPLEMENTED METHODS:
     *
     * Block Access:
     * - GetBlock(x, y, z) / SetBlock(x, y, z, state)        // Local coordinates
     * - GetBlockByPlayer() / SetBlockByPlayer()              // Player modification tracking
     * - Coordinate conversion and validation utilities
     *
     * Mesh Management:
     * - MarkDirty() / RebuildMesh()                          // Mesh rebuild workflow
     * - GetMesh() / IsDirty()                                // Mesh state queries
     *
     * State Management:
     * - ChunkState enum (Inactive, Loading, Generating, Active, Saving, Unloading)
     * - Atomic state transitions for thread safety
     * - IsModified() / IsPlayerModified()                    // Change tracking
     *
     * Serialization:
     * - Serialize() / Deserialize()                          // Chunk data persistence
     * - Integration with IChunkSerializer and IChunkStorage interfaces
     *
     * THREADING DESIGN:
     * - Block access is safe during single-threaded generation
     * - Mesh building happens on main thread (deferred via m_isDirty flag)
     * - State transitions use atomic operations for async loading system
     *
     * MEMORY OPTIMIZATION:
     * - Flat array storage with bit-shift indexing for performance
     * - Empty chunks still allocated (future: consider null optimization)
     * - Serialization uses RLE compression in ESFS format
     */
    class Chunk
    {
    public:
        // Chunk dimensions and bit operations for performance optimization (Assignment 02 specification)
        static constexpr int32_t CHUNK_BITS_X = 4; // 2^4 = 16
        static constexpr int32_t CHUNK_BITS_Y = 4; // 2^4 = 16
        static constexpr int32_t CHUNK_BITS_Z = 8; // 2^8 = 256 (Modified 2025-11-02: Increased world height from 128 to 256)

        static constexpr int32_t CHUNK_SIZE_X     = 1 << CHUNK_BITS_X; // 16
        static constexpr int32_t CHUNK_SIZE_Y     = 1 << CHUNK_BITS_Y; // 16
        static constexpr int32_t CHUNK_SIZE_Z     = 1 << CHUNK_BITS_Z; // 256 (Modified 2025-11-02)
        static constexpr int32_t CHUNK_MAX_X      = CHUNK_SIZE_X - 1;
        static constexpr int32_t CHUNK_MAX_Y      = CHUNK_SIZE_Y - 1;
        static constexpr int32_t CHUNK_MAX_Z      = CHUNK_SIZE_Z - 1;
        static constexpr int32_t BLOCKS_PER_CHUNK = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

        /**
         * @brief Bit masks for coordinate extraction from block_index (Assignment 02 specification)
         *
         * These masks are used by BlockIterator to efficiently manipulate coordinates
         * without division or modulo operations.
         *
         * MASK DEFINITIONS:
         *   CHUNK_MASK_X = 0x000F (binary: 0000 0000 0000 1111) - isolates bits 0-3
         *   CHUNK_MASK_Y = 0x00F0 (binary: 0000 0000 1111 0000) - isolates bits 4-7
         *   CHUNK_MASK_Z = 0xFF00 (binary: 1111 1111 0000 0000) - isolates bits 8-15
         *
         * USAGE EXAMPLES:
         *
         * 1. Extract coordinate (using AND):
         *    x = blockIndex & CHUNK_MASK_X
         *    y = (blockIndex & CHUNK_MASK_Y) >> CHUNK_BITS_X
         *    z = (blockIndex & CHUNK_MASK_Z) >> (CHUNK_BITS_X + CHUNK_BITS_Y)
         *
         * 2. Clear coordinate bits (using AND with NOT):
         *    blockIndex & ~CHUNK_MASK_Y   // Clears y-bits, sets y=0
         *    Used for NORTH boundary: crossing from y=15 to y=0 in next chunk
         *
         * 3. Set coordinate bits to maximum (using OR):
         *    blockIndex | CHUNK_MASK_Y    // Sets y-bits to 1111, y=15
         *    Used for SOUTH boundary: crossing from y=0 to y=15 in prev chunk
         *
         * 4. Replace coordinate bits (using AND to clear, then OR to set):
         *    (blockIndex & ~CHUNK_MASK_Y) | (newY << CHUNK_BITS_X)
         *    Used for within-chunk movement: y=8 → y=9
         *
         * CROSS-CHUNK BOUNDARY EXAMPLES:
         *   Example 1: NORTH boundary (y=15 → y=0)
         *     Original: blockIndex = 0x40FA (x=10, y=15, z=64)
         *     Operation: blockIndex & ~CHUNK_MASK_Y
         *              = 0x40FA & 0xFF0F
         *              = 0x400A (x=10, y=0, z=64) ✓
         *
         *   Example 2: SOUTH boundary (y=0 → y=15)
         *     Original: blockIndex = 0x400A (x=10, y=0, z=64)
         *     Operation: blockIndex | CHUNK_MASK_Y
         *              = 0x400A | 0x00F0
         *              = 0x40FA (x=10, y=15, z=64) ✓
         *
         * @see BlockIterator::GetNeighbor() for usage in cross-chunk navigation
         */
        static constexpr int32_t CHUNK_MASK_X = CHUNK_MAX_X;
        static constexpr int32_t CHUNK_MASK_Y = CHUNK_MAX_Y << CHUNK_BITS_X;
        static constexpr int32_t CHUNK_MASK_Z = CHUNK_MAX_Z << (CHUNK_BITS_X + CHUNK_BITS_Y);

        Chunk(IntVec2 chunkCoords);
        ~Chunk();

        // Block Access - PUBLIC for World class access
        BlockState* GetBlock(int32_t x, int32_t y, int32_t z); // Local coordinates
        void        SetBlock(int32_t x, int32_t y, int32_t z, BlockState* state); // World generation (no modify flag)
        void        SetBlockByPlayer(int32_t x, int32_t y, int32_t z, BlockState* state); // Player action (sets modify flag)
        BlockState* GetBlock(const BlockPos& worldPos); // World coordinates
        BlockState* GetTopBlock(const BlockPos& worldPos);
        void        SetBlockWorld(const BlockPos& worldPos, BlockState* state); // World generation (no modify flag)
        void        SetBlockWorldByPlayer(const BlockPos& worldPos, BlockState* state); // Player action (sets modify flag)
        int         GetTopBlockZ(const BlockPos& worldPos);

        // Optimized coordinate to index conversion using bit operations
        static size_t CoordsToIndex(int32_t x, int32_t y, int32_t z);
        static void   IndexToCoords(size_t index, int32_t& x, int32_t& y, int32_t& z);

        // Optimized chunk coordinate to world coordinate conversion
        static inline int32_t ChunkCoordsToWorld(int32_t chunkCoord) { return chunkCoord << CHUNK_BITS_X; }

        // Coordinate Conversion - PUBLIC for World class
        BlockPos LocalToWorld(int32_t x, int32_t y, int32_t z);
        bool     WorldToLocal(const BlockPos& worldPos, int32_t& x, int32_t& y, int32_t& z);
        bool     ContainsWorldPos(const BlockPos& worldPos);

        // Mesh Management - PUBLIC for rendering system
        void       MarkDirty(); // Mark chunk as needing mesh rebuild
        void       RebuildMesh(); // Regenerate ChunkMesh from block data using ChunkMeshHelper
        void       SetMesh(std::unique_ptr<ChunkMesh> mesh); // Set new mesh (used by ChunkMeshHelper)
        ChunkMesh* GetMesh() const; // Get mesh for rendering
        bool       NeedsMeshRebuild() const; // Check if mesh needs rebuilding

        //-------------------------------------------------------------------------------------------
        // State Management - PUBLIC for World management
        //-------------------------------------------------------------------------------------------

        // New: Atomic State Machine (Thread-Safe)
        ChunkState  GetState() const { return m_state.Load(); }
        void        SetState(ChunkState newState); // [Phase 2] Moved to cpp for state transition detection
        bool        TrySetState(ChunkState expected, ChunkState desired) { return m_state.CompareAndSwap(expected, desired); }
        const char* GetStateName() const { return m_state.GetStateName(); }

        // Convenience state queries
        bool IsActive() const { return GetState() == ChunkState::Active; }

        void SetGenerated(bool generated) { (void)generated; } // Suppress warning
        bool IsPopulated() const { return m_isPopulated; }
        void SetPopulated(bool populated) { m_isPopulated = populated; }

        // Modified flag (only accessed by main thread)
        bool IsModified() const { return m_isModified; }
        void SetModified(bool modified) { m_isModified = modified; }
        void MarkModified() { m_isModified = true; }

        // Player-modified flag (for PlayerModifiedOnly save strategy)
        bool IsPlayerModified() const { return m_playerModified; }
        void SetPlayerModified(bool playerModified) { m_playerModified = playerModified; }
        void MarkPlayerModified() { m_playerModified = true; }

        //Update and Management:
        void Update(float deltaTime); // Update chunk
        void Render(IRenderer* renderer) const; // Render chunk mesh
        void DebugDraw(IRenderer* renderer);

        // Utility - PUBLIC for World class
        void     Clear(); // Clear all blocks to air
        IntVec2  GetChunkCoords() const { return m_chunkCoords; }
        int32_t  GetChunkX() const { return m_chunkCoords.x; }
        int32_t  GetChunkY() const { return m_chunkCoords.y; }
        BlockPos GetWorldPos() const; // Bottom corner world position

        ChunkAttachmentHolder&       GetAttachmentHolder() { return m_attachmentHolder; }
        const ChunkAttachmentHolder& GetAttachmentHolder() const { return m_attachmentHolder; }

        // [A05] Neighbor chunk access for BlockIterator
        void   SetWorld(class World* world) { m_world = world; }
        Chunk* GetNorthNeighbor() const;
        Chunk* GetSouthNeighbor() const;
        Chunk* GetEastNeighbor() const;
        Chunk* GetWestNeighbor() const;

        // [A05] Lighting System
        void InitializeLighting(World* world);

    private:
        //-------------------------------------------------------------------------------------------
        // Core Data
        //-------------------------------------------------------------------------------------------
        IntVec2                    m_chunkCoords = IntVec2(0, 0);
        std::vector<BlockState*>   m_blocks; // Block storage
        std::unique_ptr<ChunkMesh> m_mesh; // Compiled mesh for rendering

        //-------------------------------------------------------------------------------------------
        // State Management (Multi-threaded loading system)
        //-------------------------------------------------------------------------------------------
        AtomicChunkState m_state{ChunkState::Inactive}; // Thread-safe state machine

        //-------------------------------------------------------------------------------------------
        // Sub-states (Only accessed by main thread)
        //-------------------------------------------------------------------------------------------
        bool m_isDirty        = true; // Needs mesh rebuild (main thread only)
        bool m_isModified     = false; // Needs to be saved to disk (main thread only)
        bool m_playerModified = false; // Modified by player (for PlayerModifiedOnly save strategy)
        bool m_isPopulated    = false; // Has decorations/structures (legacy, main thread only)

        //-------------------------------------------------------------------------------------------
        // Extensions
        //-------------------------------------------------------------------------------------------
        mutable ChunkAttachmentHolder m_attachmentHolder;

        /// Debug Drawing
        AABB3 m_chunkBounding;

        //-------------------------------------------------------------------------------------------
        // [A05] Neighbor Access
        //-------------------------------------------------------------------------------------------
        class World* m_world = nullptr;

        //-------------------------------------------------------------------------------------------
        // [Phase 2] Neighbor Notification for Cross-Chunk Hidden Face Culling
        //-------------------------------------------------------------------------------------------
        /**
         * @brief Notify 4 horizontal neighbors to rebuild mesh
         *
         * Called when this chunk transitions from non-Active to Active state.
         * Neighbors need to rebuild their mesh because boundary block faces
         * may change (hidden face culling depends on neighbor activation).
         *
         * Implementation:
         * - Checks if neighbor is active (GetEastNeighbor()->IsActive())
         * - Calls World::MarkChunkDirty() to add neighbor to dirty queue
         * - Only notifies ACTIVE neighbors (inactive ones will rebuild later)
         *
         * @see Task 2.1 - BuildMesh() checks neighbor activation
         * @see Task 2.2 - SetState() detects activation event
         */
        void NotifyNeighborsDirty();

        //-------------------------------------------------------------------------------------------
        // [A05] Lighting System - Boundary Block Marking
        //-------------------------------------------------------------------------------------------
        /**
         * @brief Mark boundary blocks as dirty for lighting propagation
         *
         * Called during InitializeLighting() (Task 6.2) to mark non-opaque blocks
         * at chunk boundaries as dirty. These blocks need lighting updates because
         * they may receive light from neighboring chunks.
         *
         * Implementation:
         * - Checks 4 horizontal neighbors (East, West, North, South)
         * - For each active neighbor, scans boundary blocks (x=0, x=15, y=0, y=15)
         * - Marks non-opaque blocks as dirty using World::MarkLightingDirty()
         * - Uses BlockIterator for cross-chunk boundary handling
         *
         * @param world World pointer for accessing MarkLightingDirty()
         */
        void MarkBoundaryBlocksDirty(World* world);
    };
}
