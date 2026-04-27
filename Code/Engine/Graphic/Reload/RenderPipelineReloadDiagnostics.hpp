#pragma once

#include "Engine/Graphic/Reload/RenderPipelineReloadTypes.hpp"

#include <optional>
#include <string>
#include <vector>

namespace enigma::graphic
{
    struct RenderPipelineReloadPhaseTransitionSnapshot
    {
        uint64_t                         sequence = 0;
        RenderPipelineReloadTransactionId transactionId = 0;
        RenderPipelineReloadGeneration    generation;
        RenderPipelineReloadPhase         fromPhase = RenderPipelineReloadPhase::Idle;
        RenderPipelineReloadPhase         toPhase = RenderPipelineReloadPhase::Idle;
        std::string                       reason;
    };

    struct RenderPipelineReloadDiagnosticsSnapshot
    {
        bool                               hasActiveRequest = false;
        RenderPipelineReloadRequest        activeRequest;
        RenderPipelineReloadTransactionId  activeTransactionId = 0;
        RenderPipelineReloadGeneration     activeGeneration;
        RenderPipelineReloadPhase          activePhase = RenderPipelineReloadPhase::Idle;

        bool                               hasLastIgnoredRequest = false;
        RenderPipelineReloadRequest        lastIgnoredRequest;
        RenderPipelineReloadIgnoredReason  lastIgnoredReason = RenderPipelineReloadIgnoredReason::None;

        bool                               hasQueueSyncSnapshots = false;
        QueueSubmittedFenceSnapshot        submittedBeforeSync;
        QueueFenceSnapshot                 completedBeforeSync;
        QueueFenceSnapshot                 completedAfterSync;

        bool                               hasChunkGateSnapshot = false;
        RenderPipelineReloadChunkGateSnapshot chunkGateSnapshot;

        RenderPipelineReloadFailure        failure;
        std::string                        blockingReason;
        std::vector<RenderPipelineReloadPhaseTransitionSnapshot> recentPhaseTransitions;
    };

    class RenderPipelineReloadDiagnostics
    {
    public:
        void Reset();

        void RecordActiveRequest(RenderPipelineReloadTransactionId transactionId,
                                 const RenderPipelineReloadRequest& request,
                                 RenderPipelineReloadGeneration generation,
                                 RenderPipelineReloadPhase phase);
        void ClearActiveRequest();

        void RecordPhaseTransition(RenderPipelineReloadTransactionId transactionId,
                                   RenderPipelineReloadGeneration generation,
                                   RenderPipelineReloadPhase fromPhase,
                                   RenderPipelineReloadPhase toPhase,
                                   std::string reason = {});
        void RecordIgnoredRequest(const RenderPipelineReloadRequest& request,
                                  RenderPipelineReloadIgnoredReason reason);
        void RecordQueueSyncSnapshots(const QueueSubmittedFenceSnapshot& submittedBeforeSync,
                                      const QueueFenceSnapshot& completedBeforeSync,
                                      const QueueFenceSnapshot& completedAfterSync);
        void RecordChunkGateSnapshot(const RenderPipelineReloadChunkGateSnapshot& snapshot);
        void RecordFailure(const RenderPipelineReloadFailure& failure);

        void SetActivePhase(RenderPipelineReloadPhase phase) noexcept;
        void SetActiveGeneration(RenderPipelineReloadGeneration generation) noexcept;
        void SetBlockingReason(std::string reason);

        RenderPipelineReloadDiagnosticsSnapshot BuildSnapshot() const;

    private:
        static constexpr size_t kMaxRecentPhaseTransitions = 16;

        void trimPhaseTransitionHistory();

        RenderPipelineReloadDiagnosticsSnapshot m_snapshot;
        uint64_t                                m_nextTransitionSequence = 1;
    };
} // namespace enigma::graphic
