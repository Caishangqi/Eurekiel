#pragma once

#include "Engine/Core/Event/MulticastDelegate.hpp"
#include "Engine/Graphic/Integration/WindowResizeDiagnostics.hpp"

#include <optional>

class Window;

namespace enigma::graphic
{
    class RendererSubsystem;
    class WindowResizeException;
    enum class RendererResizeReason;

    class RendererWindowResizeCoordinator
    {
    public:
        RendererWindowResizeCoordinator() = default;
        ~RendererWindowResizeCoordinator();

        RendererWindowResizeCoordinator(const RendererWindowResizeCoordinator&) = delete;
        RendererWindowResizeCoordinator& operator=(const RendererWindowResizeCoordinator&) = delete;

        void SetServices(Window* window, RendererSubsystem* renderer) noexcept;
        void SubscribeEvents();
        void UnsubscribeEvents();

        void OnWindowClientSizeChanged(const enigma::window::WindowClientSizeEvent& event);
        void OnWindowModeRequested(const enigma::window::WindowModeRequest& request);
        void OnRendererBeginFrame();
        void OnRendererFrameSlotAcquired();

        [[nodiscard]] bool HasActiveTransaction() const noexcept;
        [[nodiscard]] WindowResizeDiagnosticsSnapshot GetDiagnosticsSnapshot() const;

    private:
        void mergePendingRequest(const WindowResizeRequestSnapshot& request);
        void tryProcessPendingRequest();
        void executeTransaction(WindowResizeRequestSnapshot request);
        void transitionActiveTransaction(WindowResizeTransactionPhase nextPhase, const char* reason);
        void completeActiveTransaction(const WindowResizeRequestSnapshot& appliedRequest);
        void failActiveTransaction(const WindowResizeException& exception);
        void releaseMutationGateIfOwned();

        [[nodiscard]] WindowResizeRequestSnapshot buildSnapshotFromClientEvent(
            const enigma::window::WindowClientSizeEvent& event) const;
        [[nodiscard]] WindowResizeRequestSnapshot buildSnapshotFromModeRequest(
            const enigma::window::WindowModeRequest& request) const;
        [[nodiscard]] WindowMode resolveTargetMode(enigma::window::WindowModeRequestType type) const;
        [[nodiscard]] bool isValidClientSize(const IntVec2& clientSize) const noexcept;
        [[nodiscard]] RendererResizeReason toRendererResizeReason(const WindowResizeRequestSnapshot& request) const noexcept;

        Window*             m_window = nullptr;
        RendererSubsystem*  m_renderer = nullptr;
        WindowResizeDiagnostics m_diagnostics;

        std::optional<WindowResizeRequestSnapshot> m_pendingRequest;
        std::optional<WindowResizeRequestSnapshot> m_activeRequest;
        WindowResizeTransactionId                  m_activeTransactionId = 0;
        WindowResizeTransactionId                  m_nextTransactionId = 1;
        WindowResizeTransactionPhase               m_activePhase = WindowResizeTransactionPhase::Idle;
        bool                                       m_hasMutationGate = false;

        enigma::event::DelegateHandle m_clientSizeChangedHandle = 0;
        enigma::event::DelegateHandle m_windowModeRequestedHandle = 0;
        enigma::event::DelegateHandle m_beginFrameHandle = 0;
        enigma::event::DelegateHandle m_frameSlotAcquiredHandle = 0;
    };
} // namespace enigma::graphic
