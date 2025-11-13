#include "ChunkState.hpp"

namespace enigma::voxel
{
    const char* ChunkStateToString(ChunkState state)
    {
        switch (state)
        {
        case ChunkState::Inactive: return "Inactive";
        case ChunkState::CheckingDisk: return "CheckingDisk";
        case ChunkState::PendingLoad: return "PendingLoad";
        case ChunkState::Loading: return "Loading";
        case ChunkState::PendingGenerate: return "PendingGenerate";
        case ChunkState::Generating: return "Generating";
        case ChunkState::Active: return "Active";
        case ChunkState::PendingSave: return "PendingSave";
        case ChunkState::Saving: return "Saving";
        case ChunkState::PendingUnload: return "PendingUnload";
        case ChunkState::Unloading: return "Unloading";
        default: return "Unknown";
        }
    }

    AtomicChunkState::AtomicChunkState(ChunkState initialState)
        : m_state(initialState)
    {
    }

    ChunkState AtomicChunkState::Load() const
    {
        return m_state.load(std::memory_order_acquire);
    }

    bool AtomicChunkState::CompareAndSwap(ChunkState expected, ChunkState desired)
    {
        return m_state.compare_exchange_strong(expected, desired,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
    }

    void AtomicChunkState::Store(ChunkState newState)
    {
        m_state.store(newState, std::memory_order_release);
    }

    const char* AtomicChunkState::GetStateName() const
    {
        return ChunkStateToString(Load());
    }
} // namespace enigma::voxel
