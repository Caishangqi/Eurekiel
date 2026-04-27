#pragma once

#include "Engine/Core/Schedule/ScheduleTaskTypes.hpp"
#include "Engine/Voxel/Chunk/ChunkJob.hpp"
#include "Engine/Voxel/Chunk/MeshBuild/ChunkMeshBuildInput.hpp"
#include "Engine/Voxel/Chunk/MeshBuild/ChunkMeshBuildResult.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace enigma::voxel
{
    class World;

    inline constexpr const char* kChunkMeshBuildTaskType   = enigma::core::TaskTypeConstants::MESH_BUILDING;
    inline constexpr const char* kChunkMeshBuildTaskDomain = "ChunkMeshBuild";

    inline uint64_t MakeChunkMeshBuildTaskValue(const IntVec2& chunkCoords) noexcept
    {
        const uint64_t x = static_cast<uint64_t>(static_cast<uint32_t>(chunkCoords.x));
        const uint64_t y = static_cast<uint64_t>(static_cast<uint32_t>(chunkCoords.y));
        return (y << 32ULL) | x;
    }

    inline enigma::core::TaskKey MakeChunkMeshBuildTaskKey(const IntVec2& chunkCoords)
    {
        return enigma::core::TaskKey{ kChunkMeshBuildTaskDomain, MakeChunkMeshBuildTaskValue(chunkCoords) };
    }

    class ChunkMeshBuildTask : public ChunkJob
    {
    public:
        explicit ChunkMeshBuildTask(ChunkMeshBuildInput input,
                                    World* world,
                                    enigma::core::TaskPriority priority = enigma::core::TaskPriority::Normal)
            : ChunkJob(kChunkMeshBuildTaskType, input.GetChunkCoords())
              , m_input(std::move(input))
              , m_world(world)
              , m_priority(priority)
        {
        }

        void Execute() override;

        const ChunkMeshBuildInput& GetInput() const noexcept { return m_input; }
        uint64_t                   GetChunkInstanceId() const noexcept { return m_input.GetChunkInstanceId(); }
        uint64_t                   GetBuildVersion() const noexcept { return m_input.GetBuildVersion(); }
        enigma::graphic::RenderPipelineReloadGeneration GetReloadGeneration() const noexcept { return m_input.GetReloadGeneration(); }
        bool                       IsImportant() const noexcept { return m_input.IsImportant(); }
        enigma::core::TaskPriority GetPriority() const noexcept { return m_priority; }

        bool HasResult() const noexcept
        {
            return m_result.has_value();
        }

        void SetResult(ChunkMeshBuildResult result)
        {
            m_result = std::move(result);
        }

        std::optional<ChunkMeshBuildResult> TakeResult() noexcept
        {
            return std::exchange(m_result, std::nullopt);
        }

    private:
        ChunkMeshBuildInput                m_input;
        World*                            m_world = nullptr;
        enigma::core::TaskPriority         m_priority = enigma::core::TaskPriority::Normal;
        std::optional<ChunkMeshBuildResult> m_result;
    };
}
