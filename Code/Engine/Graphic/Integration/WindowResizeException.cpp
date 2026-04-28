#include "Engine/Graphic/Integration/WindowResizeException.hpp"

#include <utility>

namespace enigma::graphic
{
    WindowResizeException::WindowResizeException(
        std::string message,
        WindowResizeTransactionPhase phase,
        WindowResizeFailureSeverity severity,
        std::string recoveryAction)
        : m_message(std::move(message))
        , m_phase(phase)
        , m_severity(severity)
        , m_recoveryAction(std::move(recoveryAction))
    {
    }

    const char* WindowResizeException::what() const noexcept
    {
        return m_message.c_str();
    }

    WindowResizeTransactionPhase WindowResizeException::GetPhase() const noexcept
    {
        return m_phase;
    }

    WindowResizeFailureSeverity WindowResizeException::GetSeverity() const noexcept
    {
        return m_severity;
    }

    const std::string& WindowResizeException::GetErrorMessage() const noexcept
    {
        return m_message;
    }

    const std::string& WindowResizeException::GetRecoveryAction() const noexcept
    {
        return m_recoveryAction;
    }

    WindowResizeFailure WindowResizeException::ToFailure() const
    {
        WindowResizeFailure failure;
        failure.phase = m_phase;
        failure.severity = m_severity;
        failure.message = m_message;
        failure.recoveryAction = m_recoveryAction;
        return failure;
    }

    WindowResizeQueueSyncException::WindowResizeQueueSyncException(
        std::string message,
        WindowResizeFailureSeverity severity)
        : WindowResizeException(
              std::move(message),
              WindowResizeTransactionPhase::QueueSynchronizing,
              severity,
              severity == WindowResizeFailureSeverity::Fatal
                  ? "Abort resize transaction before releasing stable renderer state"
                  : "Delay resize and keep the previous stable renderer state")
    {
    }

    SwapChainResizeException::SwapChainResizeException(
        std::string message,
        WindowResizeFailureSeverity severity)
        : WindowResizeException(
              std::move(message),
              WindowResizeTransactionPhase::ResizingSwapChain,
              severity,
              severity == WindowResizeFailureSeverity::Fatal
                  ? "Terminate because the swap chain cannot provide a stable back buffer"
                  : "Keep the previous stable swap chain and retry a later resize")
    {
    }

    RendererFrontendResizeException::RendererFrontendResizeException(
        std::string message,
        WindowResizeFailureSeverity severity)
        : WindowResizeException(
              std::move(message),
              WindowResizeTransactionPhase::ResizingRendererFrontend,
              severity,
              severity == WindowResizeFailureSeverity::Fatal
                  ? "Terminate because renderer frontend state is partially mutated"
                  : "Keep or restore the previous stable renderer frontend state")
    {
    }

    WindowModeTransitionException::WindowModeTransitionException(std::string message)
        : WindowResizeException(
              std::move(message),
              WindowResizeTransactionPhase::ApplyingWindowMode,
              WindowResizeFailureSeverity::Recoverable,
              "Keep the current window mode and retry on a later request")
    {
    }

    WindowResizeFatalInvariantException::WindowResizeFatalInvariantException(
        std::string message,
        WindowResizeTransactionPhase phase)
        : WindowResizeException(
              std::move(message),
              phase,
              WindowResizeFailureSeverity::Fatal,
              "Terminate because a window resize transaction invariant was violated")
    {
    }
} // namespace enigma::graphic
