#pragma once

#include "Engine/Graphic/Resource/CommandQueueTypes.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Window/WindowEvents.hpp"

#include <string>
#include <vector>

namespace enigma::graphic
{
    using WindowResizeTransactionId = uint64_t;

    enum class WindowResizeTransactionPhase
    {
        Idle,
        Pending,
        ApplyingWindowMode,
        QueueSynchronizing,
        ResizingSwapChain,
        ResizingRendererFrontend,
        Completed,
        Failed
    };

    enum class WindowResizeIgnoredReason
    {
        None,
        Minimized,
        ZeroSizedClient,
        DuplicatePending,
        TransactionActive,
        MutationGateBusy,
        ReloadActive,
        InvalidRequest
    };

    enum class WindowResizeFailureSeverity
    {
        None,
        Recoverable,
        Fatal
    };

    struct WindowResizeRequestSnapshot
    {
        IntVec2                              clientSize = IntVec2();
        WindowMode                           targetMode = WindowMode::Windowed;
        enigma::window::WindowResizeReason   resizeReason = enigma::window::WindowResizeReason::Unknown;
        enigma::window::WindowModeRequestType modeRequestType = enigma::window::WindowModeRequestType::ToggleFullscreen;
        bool                                 hasModeRequest = false;
        bool                                 isMinimized = false;
    };

    struct WindowResizeFailure
    {
        WindowResizeTransactionPhase phase = WindowResizeTransactionPhase::Idle;
        WindowResizeFailureSeverity  severity = WindowResizeFailureSeverity::None;
        std::string                  message;
        std::string                  recoveryAction;

        [[nodiscard]] bool HasFailure() const noexcept
        {
            return severity != WindowResizeFailureSeverity::None;
        }
    };

    struct WindowResizePhaseTransitionSnapshot
    {
        uint64_t                       sequence = 0;
        WindowResizeTransactionId       transactionId = 0;
        WindowResizeTransactionPhase    fromPhase = WindowResizeTransactionPhase::Idle;
        WindowResizeTransactionPhase    toPhase = WindowResizeTransactionPhase::Idle;
        std::string                     reason;
    };

    struct WindowResizeDiagnosticsSnapshot
    {
        bool                         hasActiveTransaction = false;
        WindowResizeTransactionId     activeTransactionId = 0;
        WindowResizeTransactionPhase  activePhase = WindowResizeTransactionPhase::Idle;
        WindowResizeRequestSnapshot   activeRequest;

        bool                         hasPendingRequest = false;
        WindowResizeRequestSnapshot   pendingRequest;

        bool                         hasLastIgnoredRequest = false;
        WindowResizeRequestSnapshot   lastIgnoredRequest;
        WindowResizeIgnoredReason     lastIgnoredReason = WindowResizeIgnoredReason::None;

        bool                         hasQueueSyncSnapshots = false;
        QueueSubmittedFenceSnapshot   submittedBeforeSync;
        QueueFenceSnapshot            completedBeforeSync;
        QueueFenceSnapshot            completedAfterSync;

        IntVec2                       lastAppliedClientSize = IntVec2();
        WindowMode                    currentMode = WindowMode::Windowed;
        bool                          isMinimized = false;
        WindowResizeFailure           failure;
        std::string                   blockingReason;
        std::vector<WindowResizePhaseTransitionSnapshot> recentPhaseTransitions;
    };

    class WindowResizeDiagnostics
    {
    public:
        void Reset();

        void RecordActiveTransaction(WindowResizeTransactionId transactionId,
                                     const WindowResizeRequestSnapshot& request,
                                     WindowResizeTransactionPhase phase);
        void ClearActiveTransaction();
        void RecordPendingRequest(const WindowResizeRequestSnapshot& request);
        void ClearPendingRequest();
        void RecordIgnoredRequest(const WindowResizeRequestSnapshot& request,
                                  WindowResizeIgnoredReason reason);
        void RecordPhaseTransition(WindowResizeTransactionId transactionId,
                                   WindowResizeTransactionPhase fromPhase,
                                   WindowResizeTransactionPhase toPhase,
                                   std::string reason = {});
        void RecordQueueSyncSnapshots(const QueueSubmittedFenceSnapshot& submittedBeforeSync,
                                      const QueueFenceSnapshot& completedBeforeSync,
                                      const QueueFenceSnapshot& completedAfterSync);
        void RecordAppliedState(const IntVec2& clientSize, WindowMode mode, bool isMinimized);
        void RecordFailure(const WindowResizeFailure& failure);
        void SetBlockingReason(std::string reason);
        void SetActivePhase(WindowResizeTransactionPhase phase) noexcept;

        [[nodiscard]] WindowResizeDiagnosticsSnapshot BuildSnapshot() const;

    private:
        static constexpr size_t kMaxRecentPhaseTransitions = 16;

        void trimPhaseTransitionHistory();

        WindowResizeDiagnosticsSnapshot m_snapshot;
        uint64_t                        m_nextTransitionSequence = 1;
    };
} // namespace enigma::graphic
