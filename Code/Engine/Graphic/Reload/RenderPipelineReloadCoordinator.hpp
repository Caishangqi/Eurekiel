#pragma once

#include "Engine/Core/Event/MulticastDelegate.hpp"
#include "Engine/Graphic/Reload/RenderPipelineReloadDiagnostics.hpp"
#include "Engine/Graphic/Reload/RenderPipelineReloadException.hpp"
#include "Engine/Graphic/Reload/RenderPipelineReloadRequestGate.hpp"

#include <optional>

namespace enigma::graphic
{
    class RendererFrontendReloadService;
    class ShaderBundleReloadAdapter;
}

namespace enigma::voxel
{
    class World;
}

namespace enigma::graphic
{
    class RenderPipelineReloadCoordinator
    {
    public:
        RenderPipelineReloadCoordinator() = default;
        ~RenderPipelineReloadCoordinator();

        RenderPipelineReloadCoordinator(const RenderPipelineReloadCoordinator&) = delete;
        RenderPipelineReloadCoordinator& operator=(const RenderPipelineReloadCoordinator&) = delete;

        bool RequestLoadShaderBundle(const ShaderBundleMeta& meta);
        bool RequestUnloadShaderBundle();

        void SetServices(ShaderBundleReloadAdapter* bundleAdapter,
                         RendererFrontendReloadService* frontendReloadService) noexcept;
        void SetWorldChunkReloadParticipant(enigma::voxel::World* world) noexcept;

        void OnRendererBeginFrame();
        void OnRendererFrameSlotAcquired();
        void TickCompletionGates();

        void SubscribeRendererEvents();
        void UnsubscribeRendererEvents();

        void SetBundleOnLoadBusy(bool isBusy) noexcept;
        void SetBundleApplyBusy(bool isBusy) noexcept;
        void SetFrameSlotMutationBusy(bool isBusy) noexcept;

        bool HasActiveTransaction() const noexcept;
        bool ShouldSuppressWorldRendering() const noexcept;
        bool IsSubscribedToRendererEvents() const noexcept;
        RenderPipelineReloadGeneration GetCurrentGeneration() const noexcept;

        const RenderPipelineReloadDiagnostics& GetDiagnostics() const noexcept;
        RenderPipelineReloadDiagnosticsSnapshot GetDiagnosticsSnapshot() const;
        const RenderPipelineReloadRequestGate& GetRequestGate() const noexcept;
        const std::optional<RenderPipelineReloadTransaction>& GetActiveTransaction() const noexcept;

    private:
        bool acceptRequest(RenderPipelineReloadRequest request);
        void transitionActiveTransaction(RenderPipelineReloadPhase nextPhase, const char* reason);
        void executeQueueSynchronization();
        void executeFrameSlotMutation();
        void commitActiveTransaction();
        void completeActiveTransaction();
        void failActiveTransaction(const RenderPipelineReloadException& exception);
        bool tryAcquireMutationGate(const RenderPipelineReloadRequest& request);
        void releaseMutationGateIfOwned();
        void rollbackToPreviousStableBundle();
        void beginWorldReloadGeneration();
        void endWorldReloadGeneration();
        bool isCurrentStableBundle(const ShaderBundleMeta& meta) const;
        bool ignoreRequestBeforeGate(const RenderPipelineReloadRequest& request,
                                     RenderPipelineReloadIgnoredReason reason);
        RenderPipelineReloadTransactionId allocateTransactionId() noexcept;
        uint64_t allocateRequestId() noexcept;

        ShaderBundleReloadAdapter*                      m_bundleAdapter = nullptr;
        RendererFrontendReloadService*                  m_frontendReloadService = nullptr;
        enigma::voxel::World*                           m_worldChunkParticipant = nullptr;
        RenderPipelineReloadRequestGate                 m_requestGate;
        RenderPipelineReloadDiagnostics                 m_diagnostics;
        std::optional<RenderPipelineReloadTransaction>  m_activeTransaction;
        enigma::event::DelegateHandle                   m_beginFrameHandle = 0;
        enigma::event::DelegateHandle                   m_frameSlotAcquiredHandle = 0;
        bool                                            m_hasMutationGate = false;
        uint64_t                                        m_nextRequestId = 1;
        RenderPipelineReloadTransactionId               m_nextTransactionId = 1;
        RenderPipelineReloadGeneration                  m_currentGeneration;
    };
} // namespace enigma::graphic
