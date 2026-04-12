#pragma once

#include "Engine/Math/IntVec2.hpp"

#include <cstdint>

namespace enigma::voxel
{
    enum class ChunkMeshingCaptureHint : uint32_t
    {
        None                = 0u,
        CenterBlockData     = 1u << 0u,
        LightingData        = 1u << 1u,
        HorizontalNeighbors = 1u << 2u,
        DiagonalCorners     = 1u << 3u,
    };

    inline constexpr uint32_t ToChunkMeshingCaptureHintMask(ChunkMeshingCaptureHint hint) noexcept
    {
        return static_cast<uint32_t>(hint);
    }

    inline constexpr uint32_t kDefaultChunkMeshingCaptureHintMask =
        ToChunkMeshingCaptureHintMask(ChunkMeshingCaptureHint::CenterBlockData) |
        ToChunkMeshingCaptureHintMask(ChunkMeshingCaptureHint::LightingData) |
        ToChunkMeshingCaptureHintMask(ChunkMeshingCaptureHint::HorizontalNeighbors) |
        ToChunkMeshingCaptureHintMask(ChunkMeshingCaptureHint::DiagonalCorners);

    struct ChunkMeshingDispatchContext
    {
        IntVec2  chunkCoords       = IntVec2(0, 0);
        uint64_t chunkInstanceId   = 0;
        uint64_t buildVersion      = 0;
        uint64_t worldLifetimeToken = 0;
        uint32_t captureHintMask   = kDefaultChunkMeshingCaptureHintMask;
        bool     important         = false;

        bool IsValid() const noexcept
        {
            return chunkInstanceId != 0;
        }

        bool RequiresCaptureHint(ChunkMeshingCaptureHint hint) const noexcept
        {
            return (captureHintMask & ToChunkMeshingCaptureHintMask(hint)) != 0u;
        }
    };
}
