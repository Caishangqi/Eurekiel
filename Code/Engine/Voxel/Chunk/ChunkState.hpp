#pragma once
#include <atomic>
#include <cstdint>

namespace enigma::voxel
{
    enum class ChunkState : uint8_t
    {
        Inactive = 0,
        CheckingDisk,
        PendingLoad,
        Loading,
        PendingGenerate,
        Generating,
        Active,
        PendingSave,
        Saving,
        PendingMeshRebuild,
        BuildingMesh,
        PendingUnload,
        Unloading,
        COUNT
    };

    class AtomicChunkState
    {
    public:
        explicit    AtomicChunkState(ChunkState initialState = ChunkState::Inactive);
        ChunkState  Load() const;
        bool        CompareAndSwap(ChunkState expected, ChunkState desired);
        void        Store(ChunkState newState);
        const char* GetStateName() const;

    private:
        std::atomic<ChunkState> m_state;
    };

    const char* ChunkStateToString(ChunkState state);
} // namespace enigma::voxel
