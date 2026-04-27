#include "Engine/Graphic/Reload/RenderPipelineReloadRequestGate.hpp"

namespace enigma::graphic
{
    RenderPipelineReloadRequestDecision RenderPipelineReloadRequestGate::TryAccept(
        const RenderPipelineReloadRequest& request)
    {
        if (m_activeRequest.has_value() || m_activeTransactionId != 0)
        {
            return ignoreRequest(request, RenderPipelineReloadIgnoredReason::TransactionActive);
        }

        if (m_bundleOnLoadBusy)
        {
            return ignoreRequest(request, RenderPipelineReloadIgnoredReason::BundleOnLoadBusy);
        }

        if (m_bundleApplyBusy)
        {
            return ignoreRequest(request, RenderPipelineReloadIgnoredReason::BundleApplyBusy);
        }

        if (m_frameSlotMutationBusy)
        {
            return ignoreRequest(request, RenderPipelineReloadIgnoredReason::FrameSlotMutationBusy);
        }

        m_activeRequest = request;
        m_lastIgnoredReason = RenderPipelineReloadIgnoredReason::None;
        return RenderPipelineReloadRequestDecision::Accepted(m_activeTransactionId);
    }

    void RenderPipelineReloadRequestGate::MarkActive(RenderPipelineReloadTransactionId transactionId) noexcept
    {
        m_activeTransactionId = transactionId;
    }

    void RenderPipelineReloadRequestGate::MarkIdle() noexcept
    {
        m_activeRequest.reset();
        m_activeTransactionId = 0;
        m_bundleOnLoadBusy = false;
        m_bundleApplyBusy = false;
        m_frameSlotMutationBusy = false;
    }

    void RenderPipelineReloadRequestGate::MarkIgnored(
        const RenderPipelineReloadRequest& request,
        RenderPipelineReloadIgnoredReason reason)
    {
        ignoreRequest(request, reason);
    }

    void RenderPipelineReloadRequestGate::SetBundleOnLoadBusy(bool isBusy) noexcept
    {
        m_bundleOnLoadBusy = isBusy;
    }

    void RenderPipelineReloadRequestGate::SetBundleApplyBusy(bool isBusy) noexcept
    {
        m_bundleApplyBusy = isBusy;
    }

    void RenderPipelineReloadRequestGate::SetFrameSlotMutationBusy(bool isBusy) noexcept
    {
        m_frameSlotMutationBusy = isBusy;
    }

    bool RenderPipelineReloadRequestGate::HasActiveRequest() const noexcept
    {
        return m_activeRequest.has_value();
    }

    bool RenderPipelineReloadRequestGate::IsBusy() const noexcept
    {
        return m_activeRequest.has_value() ||
               m_activeTransactionId != 0 ||
               m_bundleOnLoadBusy ||
               m_bundleApplyBusy ||
               m_frameSlotMutationBusy;
    }

    RenderPipelineReloadTransactionId RenderPipelineReloadRequestGate::GetActiveTransactionId() const noexcept
    {
        return m_activeTransactionId;
    }

    RenderPipelineReloadIgnoredReason RenderPipelineReloadRequestGate::GetLastIgnoredReason() const noexcept
    {
        return m_lastIgnoredReason;
    }

    const std::optional<RenderPipelineReloadRequest>& RenderPipelineReloadRequestGate::GetActiveRequest() const noexcept
    {
        return m_activeRequest;
    }

    const std::optional<RenderPipelineReloadRequest>& RenderPipelineReloadRequestGate::GetLastIgnoredRequest() const noexcept
    {
        return m_lastIgnoredRequest;
    }

    RenderPipelineReloadRequestDecision RenderPipelineReloadRequestGate::ignoreRequest(
        const RenderPipelineReloadRequest& request,
        RenderPipelineReloadIgnoredReason reason)
    {
        m_lastIgnoredRequest = request;
        m_lastIgnoredReason = reason;
        return RenderPipelineReloadRequestDecision::Ignored(reason, m_activeTransactionId);
    }
} // namespace enigma::graphic
