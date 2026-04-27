#pragma once

#include "Engine/Graphic/Reload/RenderPipelineReloadTypes.hpp"

#include <optional>

namespace enigma::graphic
{
    class RenderPipelineReloadRequestGate
    {
    public:
        RenderPipelineReloadRequestDecision TryAccept(const RenderPipelineReloadRequest& request);

        void MarkActive(RenderPipelineReloadTransactionId transactionId) noexcept;
        void MarkIdle() noexcept;
        void MarkIgnored(const RenderPipelineReloadRequest& request,
                         RenderPipelineReloadIgnoredReason reason);

        void SetBundleOnLoadBusy(bool isBusy) noexcept;
        void SetBundleApplyBusy(bool isBusy) noexcept;
        void SetFrameSlotMutationBusy(bool isBusy) noexcept;

        bool HasActiveRequest() const noexcept;
        bool IsBusy() const noexcept;

        RenderPipelineReloadTransactionId GetActiveTransactionId() const noexcept;
        RenderPipelineReloadIgnoredReason GetLastIgnoredReason() const noexcept;

        const std::optional<RenderPipelineReloadRequest>& GetActiveRequest() const noexcept;
        const std::optional<RenderPipelineReloadRequest>& GetLastIgnoredRequest() const noexcept;

    private:
        RenderPipelineReloadRequestDecision ignoreRequest(const RenderPipelineReloadRequest& request,
                                                          RenderPipelineReloadIgnoredReason reason);

        std::optional<RenderPipelineReloadRequest> m_activeRequest;
        std::optional<RenderPipelineReloadRequest> m_lastIgnoredRequest;
        RenderPipelineReloadTransactionId          m_activeTransactionId = 0;
        RenderPipelineReloadIgnoredReason          m_lastIgnoredReason = RenderPipelineReloadIgnoredReason::None;
        bool                                       m_bundleOnLoadBusy = false;
        bool                                       m_bundleApplyBusy = false;
        bool                                       m_frameSlotMutationBusy = false;
    };
} // namespace enigma::graphic
