#include "Engine/Graphic/Integration/RendererWindowResizeCoordinator.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Integration/RendererEvents.hpp"
#include "Engine/Graphic/Integration/RendererFrontendMutationGate.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Integration/WindowResizeException.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Window/WindowEvents.hpp"

namespace enigma::graphic
{
    RendererWindowResizeCoordinator::~RendererWindowResizeCoordinator()
    {
        UnsubscribeEvents();
        releaseMutationGateIfOwned();
    }

    void RendererWindowResizeCoordinator::SetServices(Window* window, RendererSubsystem* renderer) noexcept
    {
        m_window = window;
        m_renderer = renderer;
    }

    void RendererWindowResizeCoordinator::SubscribeEvents()
    {
        if (m_clientSizeChangedHandle == 0)
        {
            m_clientSizeChangedHandle = enigma::window::WindowEvents::OnClientSizeChanged.Add(
                this,
                &RendererWindowResizeCoordinator::OnWindowClientSizeChanged);
        }

        if (m_windowModeRequestedHandle == 0)
        {
            m_windowModeRequestedHandle = enigma::window::WindowModeRequestEvents::OnWindowModeRequested.Add(
                this,
                &RendererWindowResizeCoordinator::OnWindowModeRequested);
        }

        if (m_beginFrameHandle == 0)
        {
            m_beginFrameHandle = RendererEvents::OnBeginFrame.Add(
                this,
                &RendererWindowResizeCoordinator::OnRendererBeginFrame);
        }

        if (m_frameSlotAcquiredHandle == 0)
        {
            m_frameSlotAcquiredHandle = RendererEvents::OnFrameSlotAcquired.Add(
                this,
                &RendererWindowResizeCoordinator::OnRendererFrameSlotAcquired);
        }
    }

    void RendererWindowResizeCoordinator::UnsubscribeEvents()
    {
        if (m_clientSizeChangedHandle != 0)
        {
            enigma::window::WindowEvents::OnClientSizeChanged.Remove(m_clientSizeChangedHandle);
            m_clientSizeChangedHandle = 0;
        }

        if (m_windowModeRequestedHandle != 0)
        {
            enigma::window::WindowModeRequestEvents::OnWindowModeRequested.Remove(m_windowModeRequestedHandle);
            m_windowModeRequestedHandle = 0;
        }

        if (m_beginFrameHandle != 0)
        {
            RendererEvents::OnBeginFrame.Remove(m_beginFrameHandle);
            m_beginFrameHandle = 0;
        }

        if (m_frameSlotAcquiredHandle != 0)
        {
            RendererEvents::OnFrameSlotAcquired.Remove(m_frameSlotAcquiredHandle);
            m_frameSlotAcquiredHandle = 0;
        }
    }

    void RendererWindowResizeCoordinator::OnWindowClientSizeChanged(
        const enigma::window::WindowClientSizeEvent& event)
    {
        WindowResizeRequestSnapshot request = buildSnapshotFromClientEvent(event);
        m_diagnostics.RecordAppliedState(request.clientSize, request.targetMode, request.isMinimized);

        if (request.isMinimized)
        {
            m_diagnostics.RecordIgnoredRequest(request, WindowResizeIgnoredReason::Minimized);
            if (m_pendingRequest.has_value())
            {
                m_pendingRequest->clientSize = request.clientSize;
                m_pendingRequest->isMinimized = true;
                m_diagnostics.RecordPendingRequest(m_pendingRequest.value());
            }
            return;
        }

        if (!isValidClientSize(request.clientSize))
        {
            m_diagnostics.RecordIgnoredRequest(request, WindowResizeIgnoredReason::ZeroSizedClient);
            return;
        }

        mergePendingRequest(request);
    }

    void RendererWindowResizeCoordinator::OnWindowModeRequested(const enigma::window::WindowModeRequest& request)
    {
        WindowResizeRequestSnapshot snapshot = buildSnapshotFromModeRequest(request);
        if (!isValidClientSize(snapshot.clientSize) || snapshot.isMinimized)
        {
            m_diagnostics.RecordIgnoredRequest(
                snapshot,
                snapshot.isMinimized ? WindowResizeIgnoredReason::Minimized : WindowResizeIgnoredReason::ZeroSizedClient);
        }

        mergePendingRequest(snapshot);
    }

    void RendererWindowResizeCoordinator::OnRendererBeginFrame()
    {
        tryProcessPendingRequest();
    }

    void RendererWindowResizeCoordinator::OnRendererFrameSlotAcquired()
    {
        if (m_activeRequest.has_value())
        {
            m_diagnostics.SetActivePhase(m_activePhase);
        }
    }

    bool RendererWindowResizeCoordinator::HasActiveTransaction() const noexcept
    {
        return m_activeRequest.has_value();
    }

    WindowResizeDiagnosticsSnapshot RendererWindowResizeCoordinator::GetDiagnosticsSnapshot() const
    {
        return m_diagnostics.BuildSnapshot();
    }

    void RendererWindowResizeCoordinator::mergePendingRequest(const WindowResizeRequestSnapshot& request)
    {
        WindowResizeRequestSnapshot merged = request;
        if (m_pendingRequest.has_value() && m_pendingRequest->hasModeRequest && !request.hasModeRequest)
        {
            merged.hasModeRequest = true;
            merged.modeRequestType = m_pendingRequest->modeRequestType;
            merged.targetMode = m_pendingRequest->targetMode;
        }

        m_pendingRequest = merged;
        m_diagnostics.RecordPendingRequest(merged);
    }

    void RendererWindowResizeCoordinator::tryProcessPendingRequest()
    {
        if (!m_pendingRequest.has_value() || m_activeRequest.has_value())
        {
            return;
        }

        if (m_pendingRequest->isMinimized || !isValidClientSize(m_pendingRequest->clientSize))
        {
            const WindowResizeIgnoredReason reason =
                m_pendingRequest->isMinimized
                    ? WindowResizeIgnoredReason::Minimized
                    : WindowResizeIgnoredReason::ZeroSizedClient;
            m_diagnostics.RecordIgnoredRequest(m_pendingRequest.value(), reason);
            if (!m_pendingRequest->hasModeRequest)
            {
                m_pendingRequest.reset();
                m_diagnostics.ClearPendingRequest();
            }
            return;
        }

        RendererFrontendMutationGate& gate = GetRendererFrontendMutationGate();
        if (!gate.TryBegin(RendererFrontendMutationOwner::WindowResize, "WindowResize"))
        {
            const RendererFrontendMutationGateSnapshot gateSnapshot = gate.GetDiagnosticsSnapshot();
            m_diagnostics.RecordIgnoredRequest(
                m_pendingRequest.value(),
                gateSnapshot.owner == RendererFrontendMutationOwner::ShaderBundleReload
                    ? WindowResizeIgnoredReason::ReloadActive
                    : WindowResizeIgnoredReason::MutationGateBusy);
            m_diagnostics.SetBlockingReason(Stringf(
                "Renderer frontend mutation gate busy: owner=%s reason=%s",
                ToString(gateSnapshot.owner),
                gateSnapshot.reason.c_str()));
            return;
        }

        m_hasMutationGate = true;
        WindowResizeRequestSnapshot request = m_pendingRequest.value();
        m_pendingRequest.reset();
        m_diagnostics.ClearPendingRequest();
        executeTransaction(request);
    }

    void RendererWindowResizeCoordinator::executeTransaction(WindowResizeRequestSnapshot request)
    {
        m_activeTransactionId = m_nextTransactionId++;
        m_activeRequest = request;
        m_activePhase = WindowResizeTransactionPhase::Pending;
        m_diagnostics.RecordActiveTransaction(m_activeTransactionId, request, m_activePhase);

        try
        {
            if (m_window == nullptr)
            {
                throw WindowResizeFatalInvariantException(
                    "Window resize coordinator has no Window service",
                    WindowResizeTransactionPhase::Pending);
            }

            if (m_renderer == nullptr)
            {
                throw WindowResizeFatalInvariantException(
                    "Window resize coordinator has no RendererSubsystem service",
                    WindowResizeTransactionPhase::Pending);
            }

            if (request.hasModeRequest)
            {
                transitionActiveTransaction(
                    WindowResizeTransactionPhase::ApplyingWindowMode,
                    "Applying requested window mode");
                enigma::window::WindowModeRequest modeRequest;
                modeRequest.type = request.modeRequestType;
                modeRequest.source = "RendererWindowResizeCoordinator";
                if (!m_window->ApplyWindowModeRequest(modeRequest))
                {
                    throw WindowModeTransitionException("Failed to apply requested window mode");
                }

                request.clientSize = m_window->GetClientDimensions();
                request.targetMode = m_window->GetWindowMode();
                request.resizeReason = enigma::window::WindowResizeReason::ModeChanged;
                request.isMinimized = m_window->IsMinimized();
                m_activeRequest = request;

                if (request.isMinimized || !isValidClientSize(request.clientSize))
                {
                    throw WindowModeTransitionException("Window mode request produced an invalid client size");
                }
            }

            transitionActiveTransaction(
                WindowResizeTransactionPhase::QueueSynchronizing,
                "Synchronizing active queues before window resize");
            const QueueSubmittedFenceSnapshot submittedBeforeSync =
                D3D12RenderSystem::GetLastSubmittedFenceSnapshot();
            const QueueFenceSnapshot completedBeforeSync =
                D3D12RenderSystem::GetCompletedFenceSnapshot();
            if (!D3D12RenderSystem::SynchronizeActiveQueues("WindowResize"))
            {
                throw WindowResizeQueueSyncException(
                    "Window resize failed while synchronizing active queues",
                    WindowResizeFailureSeverity::Fatal);
            }
            const QueueFenceSnapshot completedAfterSync =
                D3D12RenderSystem::GetCompletedFenceSnapshot();
            m_diagnostics.RecordQueueSyncSnapshots(
                submittedBeforeSync,
                completedBeforeSync,
                completedAfterSync);

            transitionActiveTransaction(
                WindowResizeTransactionPhase::ResizingSwapChain,
                "Resizing DX12 swap chain");
            if (!D3D12RenderSystem::ResizeSwapChain(
                    static_cast<uint32_t>(request.clientSize.x),
                    static_cast<uint32_t>(request.clientSize.y)))
            {
                throw SwapChainResizeException(Stringf(
                    "DX12 swap chain resize failed for %dx%d",
                    request.clientSize.x,
                    request.clientSize.y));
            }

            transitionActiveTransaction(
                WindowResizeTransactionPhase::ResizingRendererFrontend,
                "Resizing renderer frontend state");
            if (!m_renderer->ResizeFrontendState(
                    static_cast<uint32_t>(request.clientSize.x),
                    static_cast<uint32_t>(request.clientSize.y),
                    toRendererResizeReason(request)))
            {
                throw RendererFrontendResizeException(Stringf(
                    "Renderer frontend resize failed for %dx%d",
                    request.clientSize.x,
                    request.clientSize.y));
            }

            completeActiveTransaction(request);
        }
        catch (const WindowResizeException& exception)
        {
            failActiveTransaction(exception);
        }
        catch (const std::exception& exception)
        {
            failActiveTransaction(RendererFrontendResizeException(Stringf(
                "Unexpected window resize transaction failure: %s",
                exception.what())));
        }
    }

    void RendererWindowResizeCoordinator::transitionActiveTransaction(
        WindowResizeTransactionPhase nextPhase,
        const char* reason)
    {
        const WindowResizeTransactionPhase previousPhase = m_activePhase;
        if (previousPhase == nextPhase)
        {
            m_diagnostics.SetActivePhase(nextPhase);
            return;
        }

        m_activePhase = nextPhase;
        m_diagnostics.RecordPhaseTransition(
            m_activeTransactionId,
            previousPhase,
            nextPhase,
            reason ? reason : "");
    }

    void RendererWindowResizeCoordinator::completeActiveTransaction(
        const WindowResizeRequestSnapshot& appliedRequest)
    {
        transitionActiveTransaction(
            WindowResizeTransactionPhase::Completed,
            "Window resize transaction completed");
        m_diagnostics.RecordAppliedState(
            appliedRequest.clientSize,
            appliedRequest.targetMode,
            appliedRequest.isMinimized);
        releaseMutationGateIfOwned();
        m_activeRequest.reset();
        m_activeTransactionId = 0;
        m_activePhase = WindowResizeTransactionPhase::Idle;

        if (m_pendingRequest.has_value() &&
            m_pendingRequest->clientSize == appliedRequest.clientSize &&
            m_pendingRequest->targetMode == appliedRequest.targetMode)
        {
            m_pendingRequest.reset();
            m_diagnostics.ClearPendingRequest();
        }

        m_diagnostics.ClearActiveTransaction();
    }

    void RendererWindowResizeCoordinator::failActiveTransaction(const WindowResizeException& exception)
    {
        m_diagnostics.RecordFailure(exception.ToFailure());
        releaseMutationGateIfOwned();
        m_activeRequest.reset();
        m_activeTransactionId = 0;
        m_activePhase = WindowResizeTransactionPhase::Idle;

        if (exception.GetSeverity() == WindowResizeFailureSeverity::Fatal)
        {
            ERROR_AND_DIE(exception.GetErrorMessage())
            return;
        }

        ERROR_RECOVERABLE(exception.GetErrorMessage());
    }

    void RendererWindowResizeCoordinator::releaseMutationGateIfOwned()
    {
        if (!m_hasMutationGate)
        {
            return;
        }

        GetRendererFrontendMutationGate().End(RendererFrontendMutationOwner::WindowResize);
        m_hasMutationGate = false;
    }

    WindowResizeRequestSnapshot RendererWindowResizeCoordinator::buildSnapshotFromClientEvent(
        const enigma::window::WindowClientSizeEvent& event) const
    {
        WindowResizeRequestSnapshot request;
        request.clientSize = event.newClientSize;
        request.targetMode = event.mode;
        request.resizeReason = event.reason;
        request.isMinimized = event.isMinimized;
        return request;
    }

    WindowResizeRequestSnapshot RendererWindowResizeCoordinator::buildSnapshotFromModeRequest(
        const enigma::window::WindowModeRequest& request) const
    {
        WindowResizeRequestSnapshot snapshot;
        snapshot.clientSize = m_window ? m_window->GetClientDimensions() : IntVec2();
        snapshot.targetMode = resolveTargetMode(request.type);
        snapshot.resizeReason = enigma::window::WindowResizeReason::ModeChanged;
        snapshot.modeRequestType = request.type;
        snapshot.hasModeRequest = true;
        snapshot.isMinimized = m_window ? m_window->IsMinimized() : false;
        return snapshot;
    }

    WindowMode RendererWindowResizeCoordinator::resolveTargetMode(
        enigma::window::WindowModeRequestType type) const
    {
        switch (type)
        {
        case enigma::window::WindowModeRequestType::ToggleFullscreen:
            return (m_window != nullptr && m_window->IsInWindowedMode())
                       ? WindowMode::BorderlessFullscreen
                       : WindowMode::Windowed;
        case enigma::window::WindowModeRequestType::SetWindowed:
            return WindowMode::Windowed;
        case enigma::window::WindowModeRequestType::SetBorderlessFullscreen:
            return WindowMode::BorderlessFullscreen;
        }

        return WindowMode::Windowed;
    }

    bool RendererWindowResizeCoordinator::isValidClientSize(const IntVec2& clientSize) const noexcept
    {
        return clientSize.x > 0 && clientSize.y > 0;
    }

    RendererResizeReason RendererWindowResizeCoordinator::toRendererResizeReason(
        const WindowResizeRequestSnapshot& request) const noexcept
    {
        return request.hasModeRequest || request.resizeReason == enigma::window::WindowResizeReason::ModeChanged
                   ? RendererResizeReason::WindowModeChanged
                   : RendererResizeReason::WindowClientSizeChanged;
    }
} // namespace enigma::graphic
