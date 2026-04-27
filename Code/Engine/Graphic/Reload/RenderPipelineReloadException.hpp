#pragma once

#include "Engine/Graphic/Reload/RenderPipelineReloadTypes.hpp"

#include <exception>
#include <string>

namespace enigma::graphic
{
    class RenderPipelineReloadException : public std::exception
    {
    public:
        RenderPipelineReloadException(std::string message,
                                      RenderPipelineReloadPhase phase,
                                      RenderPipelineReloadFailureSeverity severity,
                                      std::string recoveryAction);

        const char* what() const noexcept override;

        RenderPipelineReloadPhase GetPhase() const noexcept;
        RenderPipelineReloadFailureSeverity GetSeverity() const noexcept;
        const std::string& GetMessage() const noexcept;
        const std::string& GetRecoveryAction() const noexcept;

    private:
        std::string                         m_message;
        RenderPipelineReloadPhase           m_phase = RenderPipelineReloadPhase::Idle;
        RenderPipelineReloadFailureSeverity m_severity = RenderPipelineReloadFailureSeverity::None;
        std::string                         m_recoveryAction;
    };

    class ReloadRequestIgnoredException : public RenderPipelineReloadException
    {
    public:
        ReloadRequestIgnoredException(std::string message,
                                      RenderPipelineReloadIgnoredReason reason,
                                      RenderPipelineReloadPhase phase = RenderPipelineReloadPhase::RequestAccepted);

        RenderPipelineReloadIgnoredReason GetReason() const noexcept;

    private:
        RenderPipelineReloadIgnoredReason m_reason = RenderPipelineReloadIgnoredReason::None;
    };

    class ReloadQueueSyncException : public RenderPipelineReloadException
    {
    public:
        explicit ReloadQueueSyncException(
            std::string message,
            RenderPipelineReloadFailureSeverity severity = RenderPipelineReloadFailureSeverity::Fatal);
    };

    class ReloadFrontendApplyException : public RenderPipelineReloadException
    {
    public:
        explicit ReloadFrontendApplyException(std::string message);
    };

    class ReloadChunkGateException : public RenderPipelineReloadException
    {
    public:
        explicit ReloadChunkGateException(std::string message);
    };

    class ReloadRollbackException : public RenderPipelineReloadException
    {
    public:
        explicit ReloadRollbackException(std::string message);
    };

    class ReloadFatalInvariantException : public RenderPipelineReloadException
    {
    public:
        explicit ReloadFatalInvariantException(std::string message,
                                               RenderPipelineReloadPhase phase = RenderPipelineReloadPhase::Failed);
    };
} // namespace enigma::graphic
