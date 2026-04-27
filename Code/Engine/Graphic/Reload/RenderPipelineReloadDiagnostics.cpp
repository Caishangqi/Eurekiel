#include "Engine/Graphic/Reload/RenderPipelineReloadDiagnostics.hpp"

#include <utility>

namespace enigma::graphic
{
    void RenderPipelineReloadDiagnostics::Reset()
    {
        m_snapshot = {};
        m_nextTransitionSequence = 1;
    }

    void RenderPipelineReloadDiagnostics::RecordActiveRequest(
        RenderPipelineReloadTransactionId transactionId,
        const RenderPipelineReloadRequest& request,
        RenderPipelineReloadGeneration generation,
        RenderPipelineReloadPhase phase)
    {
        m_snapshot.hasActiveRequest = true;
        m_snapshot.activeRequest = request;
        m_snapshot.activeTransactionId = transactionId;
        m_snapshot.activeGeneration = generation;
        m_snapshot.activePhase = phase;
    }

    void RenderPipelineReloadDiagnostics::ClearActiveRequest()
    {
        m_snapshot.hasActiveRequest = false;
        m_snapshot.activeRequest = {};
        m_snapshot.activeTransactionId = 0;
        m_snapshot.activeGeneration = {};
        m_snapshot.activePhase = RenderPipelineReloadPhase::Idle;
        m_snapshot.blockingReason.clear();
    }

    void RenderPipelineReloadDiagnostics::RecordPhaseTransition(
        RenderPipelineReloadTransactionId transactionId,
        RenderPipelineReloadGeneration generation,
        RenderPipelineReloadPhase fromPhase,
        RenderPipelineReloadPhase toPhase,
        std::string reason)
    {
        RenderPipelineReloadPhaseTransitionSnapshot transition;
        transition.sequence = m_nextTransitionSequence++;
        transition.transactionId = transactionId;
        transition.generation = generation;
        transition.fromPhase = fromPhase;
        transition.toPhase = toPhase;
        transition.reason = std::move(reason);

        m_snapshot.recentPhaseTransitions.push_back(std::move(transition));
        trimPhaseTransitionHistory();

        m_snapshot.activeTransactionId = transactionId;
        m_snapshot.activeGeneration = generation;
        m_snapshot.activePhase = toPhase;
    }

    void RenderPipelineReloadDiagnostics::RecordIgnoredRequest(
        const RenderPipelineReloadRequest& request,
        RenderPipelineReloadIgnoredReason reason)
    {
        m_snapshot.hasLastIgnoredRequest = true;
        m_snapshot.lastIgnoredRequest = request;
        m_snapshot.lastIgnoredReason = reason;
    }

    void RenderPipelineReloadDiagnostics::RecordQueueSyncSnapshots(
        const QueueSubmittedFenceSnapshot& submittedBeforeSync,
        const QueueFenceSnapshot& completedBeforeSync,
        const QueueFenceSnapshot& completedAfterSync)
    {
        m_snapshot.hasQueueSyncSnapshots = true;
        m_snapshot.submittedBeforeSync = submittedBeforeSync;
        m_snapshot.completedBeforeSync = completedBeforeSync;
        m_snapshot.completedAfterSync = completedAfterSync;
    }

    void RenderPipelineReloadDiagnostics::RecordChunkGateSnapshot(
        const RenderPipelineReloadChunkGateSnapshot& snapshot)
    {
        m_snapshot.hasChunkGateSnapshot = true;
        m_snapshot.chunkGateSnapshot = snapshot;
        m_snapshot.blockingReason = snapshot.blockingReason;
    }

    void RenderPipelineReloadDiagnostics::RecordFailure(const RenderPipelineReloadFailure& failure)
    {
        m_snapshot.failure = failure;
        if (failure.HasFailure())
        {
            m_snapshot.activePhase = RenderPipelineReloadPhase::Failed;
            m_snapshot.blockingReason = failure.message;
        }
    }

    void RenderPipelineReloadDiagnostics::SetActivePhase(RenderPipelineReloadPhase phase) noexcept
    {
        m_snapshot.activePhase = phase;
    }

    void RenderPipelineReloadDiagnostics::SetActiveGeneration(RenderPipelineReloadGeneration generation) noexcept
    {
        m_snapshot.activeGeneration = generation;
    }

    void RenderPipelineReloadDiagnostics::SetBlockingReason(std::string reason)
    {
        m_snapshot.blockingReason = std::move(reason);
    }

    RenderPipelineReloadDiagnosticsSnapshot RenderPipelineReloadDiagnostics::BuildSnapshot() const
    {
        return m_snapshot;
    }

    void RenderPipelineReloadDiagnostics::trimPhaseTransitionHistory()
    {
        if (m_snapshot.recentPhaseTransitions.size() <= kMaxRecentPhaseTransitions)
        {
            return;
        }

        const size_t removeCount = m_snapshot.recentPhaseTransitions.size() - kMaxRecentPhaseTransitions;
        m_snapshot.recentPhaseTransitions.erase(
            m_snapshot.recentPhaseTransitions.begin(),
            m_snapshot.recentPhaseTransitions.begin() + static_cast<std::vector<RenderPipelineReloadPhaseTransitionSnapshot>::difference_type>(removeCount));
    }
} // namespace enigma::graphic
