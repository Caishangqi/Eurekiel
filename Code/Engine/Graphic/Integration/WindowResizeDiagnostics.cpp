#include "Engine/Graphic/Integration/WindowResizeDiagnostics.hpp"

#include <utility>

namespace enigma::graphic
{
    void WindowResizeDiagnostics::Reset()
    {
        m_snapshot = {};
        m_nextTransitionSequence = 1;
    }

    void WindowResizeDiagnostics::RecordActiveTransaction(
        WindowResizeTransactionId transactionId,
        const WindowResizeRequestSnapshot& request,
        WindowResizeTransactionPhase phase)
    {
        m_snapshot.hasActiveTransaction = true;
        m_snapshot.activeTransactionId = transactionId;
        m_snapshot.activeRequest = request;
        m_snapshot.activePhase = phase;
    }

    void WindowResizeDiagnostics::ClearActiveTransaction()
    {
        m_snapshot.hasActiveTransaction = false;
        m_snapshot.activeTransactionId = 0;
        m_snapshot.activeRequest = {};
        m_snapshot.activePhase = WindowResizeTransactionPhase::Idle;
        m_snapshot.blockingReason.clear();
    }

    void WindowResizeDiagnostics::RecordPendingRequest(const WindowResizeRequestSnapshot& request)
    {
        m_snapshot.hasPendingRequest = true;
        m_snapshot.pendingRequest = request;
    }

    void WindowResizeDiagnostics::ClearPendingRequest()
    {
        m_snapshot.hasPendingRequest = false;
        m_snapshot.pendingRequest = {};
    }

    void WindowResizeDiagnostics::RecordIgnoredRequest(
        const WindowResizeRequestSnapshot& request,
        WindowResizeIgnoredReason reason)
    {
        m_snapshot.hasLastIgnoredRequest = true;
        m_snapshot.lastIgnoredRequest = request;
        m_snapshot.lastIgnoredReason = reason;
    }

    void WindowResizeDiagnostics::RecordPhaseTransition(
        WindowResizeTransactionId transactionId,
        WindowResizeTransactionPhase fromPhase,
        WindowResizeTransactionPhase toPhase,
        std::string reason)
    {
        WindowResizePhaseTransitionSnapshot transition;
        transition.sequence = m_nextTransitionSequence++;
        transition.transactionId = transactionId;
        transition.fromPhase = fromPhase;
        transition.toPhase = toPhase;
        transition.reason = std::move(reason);

        m_snapshot.recentPhaseTransitions.push_back(std::move(transition));
        trimPhaseTransitionHistory();

        m_snapshot.activeTransactionId = transactionId;
        m_snapshot.activePhase = toPhase;
    }

    void WindowResizeDiagnostics::RecordQueueSyncSnapshots(
        const QueueSubmittedFenceSnapshot& submittedBeforeSync,
        const QueueFenceSnapshot& completedBeforeSync,
        const QueueFenceSnapshot& completedAfterSync)
    {
        m_snapshot.hasQueueSyncSnapshots = true;
        m_snapshot.submittedBeforeSync = submittedBeforeSync;
        m_snapshot.completedBeforeSync = completedBeforeSync;
        m_snapshot.completedAfterSync = completedAfterSync;
    }

    void WindowResizeDiagnostics::RecordAppliedState(const IntVec2& clientSize, WindowMode mode, bool isMinimized)
    {
        m_snapshot.lastAppliedClientSize = clientSize;
        m_snapshot.currentMode = mode;
        m_snapshot.isMinimized = isMinimized;
    }

    void WindowResizeDiagnostics::RecordFailure(const WindowResizeFailure& failure)
    {
        m_snapshot.failure = failure;
        if (failure.HasFailure())
        {
            m_snapshot.activePhase = WindowResizeTransactionPhase::Failed;
            m_snapshot.blockingReason = failure.message;
        }
    }

    void WindowResizeDiagnostics::SetBlockingReason(std::string reason)
    {
        m_snapshot.blockingReason = std::move(reason);
    }

    void WindowResizeDiagnostics::SetActivePhase(WindowResizeTransactionPhase phase) noexcept
    {
        m_snapshot.activePhase = phase;
    }

    WindowResizeDiagnosticsSnapshot WindowResizeDiagnostics::BuildSnapshot() const
    {
        return m_snapshot;
    }

    void WindowResizeDiagnostics::trimPhaseTransitionHistory()
    {
        if (m_snapshot.recentPhaseTransitions.size() <= kMaxRecentPhaseTransitions)
        {
            return;
        }

        const size_t removeCount = m_snapshot.recentPhaseTransitions.size() - kMaxRecentPhaseTransitions;
        m_snapshot.recentPhaseTransitions.erase(
            m_snapshot.recentPhaseTransitions.begin(),
            m_snapshot.recentPhaseTransitions.begin() + static_cast<std::vector<WindowResizePhaseTransitionSnapshot>::difference_type>(removeCount));
    }
} // namespace enigma::graphic
