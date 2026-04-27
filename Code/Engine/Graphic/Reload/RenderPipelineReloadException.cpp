#include "Engine/Graphic/Reload/RenderPipelineReloadException.hpp"

#include <utility>

namespace enigma::graphic
{
    RenderPipelineReloadException::RenderPipelineReloadException(
        std::string message,
        RenderPipelineReloadPhase phase,
        RenderPipelineReloadFailureSeverity severity,
        std::string recoveryAction)
        : m_message(std::move(message))
        , m_phase(phase)
        , m_severity(severity)
        , m_recoveryAction(std::move(recoveryAction))
    {
    }

    const char* RenderPipelineReloadException::what() const noexcept
    {
        return m_message.c_str();
    }

    RenderPipelineReloadPhase RenderPipelineReloadException::GetPhase() const noexcept
    {
        return m_phase;
    }

    RenderPipelineReloadFailureSeverity RenderPipelineReloadException::GetSeverity() const noexcept
    {
        return m_severity;
    }

    const std::string& RenderPipelineReloadException::GetMessage() const noexcept
    {
        return m_message;
    }

    const std::string& RenderPipelineReloadException::GetRecoveryAction() const noexcept
    {
        return m_recoveryAction;
    }

    ReloadRequestIgnoredException::ReloadRequestIgnoredException(
        std::string message,
        RenderPipelineReloadIgnoredReason reason,
        RenderPipelineReloadPhase phase)
        : RenderPipelineReloadException(
              std::move(message),
              phase,
              RenderPipelineReloadFailureSeverity::None,
              "Ignore request and keep the active reload transaction unchanged")
        , m_reason(reason)
    {
    }

    RenderPipelineReloadIgnoredReason ReloadRequestIgnoredException::GetReason() const noexcept
    {
        return m_reason;
    }

    ReloadQueueSyncException::ReloadQueueSyncException(
        std::string message,
        RenderPipelineReloadFailureSeverity severity)
        : RenderPipelineReloadException(
              std::move(message),
              RenderPipelineReloadPhase::QueueSynchronizing,
              severity,
              severity == RenderPipelineReloadFailureSeverity::Fatal
                  ? "Abort reload transaction before publishing any frontend state"
                  : "Abort reload transaction and keep previous stable frontend state")
    {
    }

    ReloadFrontendApplyException::ReloadFrontendApplyException(std::string message)
        : RenderPipelineReloadException(
              std::move(message),
              RenderPipelineReloadPhase::FrameSlotMutation,
              RenderPipelineReloadFailureSeverity::Recoverable,
              "Rollback to the previous stable shader bundle and frontend state")
    {
    }

    ReloadChunkGateException::ReloadChunkGateException(std::string message)
        : RenderPipelineReloadException(
              std::move(message),
              RenderPipelineReloadPhase::WaitingForChunkGate,
              RenderPipelineReloadFailureSeverity::Recoverable,
              "Fail the transaction and restore or keep a stable fallback rendering state")
    {
    }

    ReloadRollbackException::ReloadRollbackException(std::string message)
        : RenderPipelineReloadException(
              std::move(message),
              RenderPipelineReloadPhase::Failed,
              RenderPipelineReloadFailureSeverity::Fatal,
              "Terminate because no stable renderer state could be restored")
    {
    }

    ReloadFatalInvariantException::ReloadFatalInvariantException(std::string message,
                                                                 RenderPipelineReloadPhase phase)
        : RenderPipelineReloadException(
              std::move(message),
              phase,
              RenderPipelineReloadFailureSeverity::Fatal,
              "Terminate because a reload transaction invariant was violated")
    {
    }
} // namespace enigma::graphic
