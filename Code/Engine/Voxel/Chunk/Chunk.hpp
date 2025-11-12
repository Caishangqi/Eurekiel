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

        // Bit masks for coordinate extraction (Assignment 02 specification)
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
        void       RebuildMesh(); // Regenerate ChunkMesh from block data using ChunkMeshBuilder
        void       SetMesh(std::unique_ptr<ChunkMesh> mesh); // Set new mesh (used by ChunkMeshBuilder)
        ChunkMesh* GetMesh() const; // Get mesh for rendering
        bool       NeedsMeshRebuild() const; // Check if mesh needs rebuilding

        //-------------------------------------------------------------------------------------------
        // State Management - PUBLIC for World management
        //-------------------------------------------------------------------------------------------

        // New: Atomic State Machine (Thread-Safe)
        ChunkState  GetState() const { return m_state.Load(); }
        bool        TrySetState(ChunkState expected, ChunkState desired) { return m_state.CompareAndSwap(expected, desired); }
        const char* GetStateName() const { return m_state.GetStateName(); }

        // Convenience state queries
        bool IsActive() const { return GetState() == ChunkState::Active; }

        bool IsLoading() const
        {
            ChunkState state = GetState();
            return state == ChunkState::Loading || state == ChunkState::Generating;
        }

        bool IsPendingWork() const
        {
            ChunkState state = GetState();
            return state == ChunkState::PendingLoad ||
                state == ChunkState::PendingGenerate ||
                state == ChunkState::PendingSave;
        }

        // Legacy compatibility (to be removed after full migration)
        bool IsGenerated() const
        {
            ChunkState state = GetState();
            return state == ChunkState::Active ||
                state == ChunkState::PendingSave ||
                state == ChunkState::Saving;
        }

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

        // 预留：扩展数据接口（类似NeoForge Attachments）
        ChunkAttachmentHolder&       GetAttachmentHolder() { return m_attachmentHolder; }
        const ChunkAttachmentHolder& GetAttachmentHolder() const { return m_attachmentHolder; }

        // [A05] Neighbor chunk access for BlockIterator
        void   SetChunkManager(class ChunkManager* manager) { m_chunkManager = manager; }
        Chunk* GetNorthNeighbor() const;
        Chunk* GetSouthNeighbor() const;
        Chunk* GetEastNeighbor() const;
        Chunk* GetWestNeighbor() const;

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
        class ChunkManager* m_chunkManager = nullptr;
    };
}
