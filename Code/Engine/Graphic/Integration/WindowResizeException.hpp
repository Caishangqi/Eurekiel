#pragma once

#include "Engine/Graphic/Integration/WindowResizeDiagnostics.hpp"

#include <exception>
#include <string>

namespace enigma::graphic
{
    class WindowResizeException : public std::exception
    {
    public:
        WindowResizeException(std::string message,
                              WindowResizeTransactionPhase phase,
                              WindowResizeFailureSeverity severity,
                              std::string recoveryAction);

        const char* what() const noexcept override;

        [[nodiscard]] WindowResizeTransactionPhase GetPhase() const noexcept;
        [[nodiscard]] WindowResizeFailureSeverity GetSeverity() const noexcept;
        [[nodiscard]] const std::string& GetErrorMessage() const noexcept;
        [[nodiscard]] const std::string& GetRecoveryAction() const noexcept;

        [[nodiscard]] WindowResizeFailure ToFailure() const;

    private:
        std::string                  m_message;
        WindowResizeTransactionPhase m_phase = WindowResizeTransactionPhase::Idle;
        WindowResizeFailureSeverity  m_severity = WindowResizeFailureSeverity::None;
        std::string                  m_recoveryAction;
    };

    class WindowResizeQueueSyncException : public WindowResizeException
    {
    public:
        explicit WindowResizeQueueSyncException(
            std::string message,
            WindowResizeFailureSeverity severity = WindowResizeFailureSeverity::Fatal);
    };

    class SwapChainResizeException : public WindowResizeException
    {
    public:
        explicit SwapChainResizeException(
            std::string message,
            WindowResizeFailureSeverity severity = WindowResizeFailureSeverity::Recoverable);
    };

    class RendererFrontendResizeException : public WindowResizeException
    {
    public:
        explicit RendererFrontendResizeException(
            std::string message,
            WindowResizeFailureSeverity severity = WindowResizeFailureSeverity::Recoverable);
    };

    class WindowModeTransitionException : public WindowResizeException
    {
    public:
        explicit WindowModeTransitionException(std::string message);
    };

    class WindowResizeFatalInvariantException : public WindowResizeException
    {
    public:
        explicit WindowResizeFatalInvariantException(
            std::string message,
            WindowResizeTransactionPhase phase = WindowResizeTransactionPhase::Failed);
    };
} // namespace enigma::graphic
