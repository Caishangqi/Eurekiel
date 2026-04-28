#include "Engine/Graphic/Reload/RenderPipelineReloadCoordinator.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleReloadAdapter.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Bundle/ShaderBundleEvents.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Integration/RendererEvents.hpp"
#include "Engine/Graphic/Integration/RendererFrontendMutationGate.hpp"
#include "Engine/Graphic/Reload/RendererFrontendReloadService.hpp"
#include "Engine/Graphic/Reload/RenderPipelineReloadEvents.hpp"
#include "Engine/Voxel/World/World.hpp"

#include <utility>

namespace enigma::graphic
{
    RenderPipelineReloadCoordinator::~RenderPipelineReloadCoordinator()
    {
        UnsubscribeRendererEvents();
        releaseMutationGateIfOwned();
    }

    bool RenderPipelineReloadCoordinator::RequestLoadShaderBundle(const ShaderBundleMeta& meta)
    {
        RenderPipelineReloadRequest request;
        request.type = RenderPipelineReloadRequestType::LoadShaderBundle;
        request.shaderBundleMeta = meta;
        request.requestId = allocateRequestId();
        request.debugName = meta.name.empty() ? "LoadShaderBundle" : meta.name;

        if (!m_requestGate.IsBusy() && isCurrentStableBundle(meta))
        {
            ignoreRequestBeforeGate(
                request,
                RenderPipelineReloadIgnoredReason::CurrentBundleAlreadyLoaded);
            return false;
        }

        return acceptRequest(std::move(request));
    }

    bool RenderPipelineReloadCoordinator::RequestUnloadShaderBundle()
    {
        RenderPipelineReloadRequest request;
        request.type = RenderPipelineReloadRequestType::UnloadShaderBundle;
        request.requestId = allocateRequestId();
        request.debugName = "UnloadShaderBundle";

        return acceptRequest(std::move(request));
    }

    void RenderPipelineReloadCoordinator::SetServices(
        ShaderBundleReloadAdapter* bundleAdapter,
        RendererFrontendReloadService* frontendReloadService) noexcept
    {
        m_bundleAdapter = bundleAdapter;
        m_frontendReloadService = frontendReloadService;
    }

    void RenderPipelineReloadCoordinator::SetWorldChunkReloadParticipant(enigma::voxel::World* world) noexcept
    {
        m_worldChunkParticipant = world;
    }

    void RenderPipelineReloadCoordinator::OnRendererBeginFrame()
    {
        if (!m_activeTransaction.has_value())
        {
            return;
        }

        if (m_activeTransaction->phase == RenderPipelineReloadPhase::RequestAccepted)
        {
            try
            {
                executeQueueSynchronization();
            }
            catch (const RenderPipelineReloadException& exception)
            {
                failActiveTransaction(exception);
            }
            catch (const std::exception& exception)
            {
                failActiveTransaction(ReloadFrontendApplyException(Stringf(
                    "Render pipeline reload queue synchronization failed: %s",
                    exception.what())));
            }
            return;
        }

        if (m_activeTransaction->phase == RenderPipelineReloadPhase::WaitingForChunkGate)
        {
            try
            {
                TickCompletionGates();
            }
            catch (const RenderPipelineReloadException& exception)
            {
                failActiveTransaction(exception);
            }
            catch (const std::exception& exception)
            {
                failActiveTransaction(ReloadFrontendApplyException(Stringf(
                    "Render pipeline reload chunk gate tick failed: %s",
                    exception.what())));
            }
            return;
        }

        m_diagnostics.SetActivePhase(m_activeTransaction->phase);
    }

    void RenderPipelineReloadCoordinator::OnRendererFrameSlotAcquired()
    {
        if (!m_activeTransaction.has_value())
        {
            return;
        }

        if (m_activeTransaction->phase != RenderPipelineReloadPhase::QueueSynchronizing)
        {
            m_diagnostics.SetActivePhase(m_activeTransaction->phase);
            return;
        }

        try
        {
            executeFrameSlotMutation();
            TickCompletionGates();
        }
        catch (const RenderPipelineReloadException& exception)
        {
            failActiveTransaction(exception);
        }
        catch (const std::exception& exception)
        {
            failActiveTransaction(ReloadFrontendApplyException(Stringf(
                "Render pipeline reload frame-slot mutation failed: %s",
                exception.what())));
        }
    }

    void RenderPipelineReloadCoordinator::TickCompletionGates()
    {
        if (!m_activeTransaction.has_value())
        {
            return;
        }

        if (m_activeTransaction->phase != RenderPipelineReloadPhase::WaitingForChunkGate)
        {
            m_diagnostics.SetActivePhase(m_activeTransaction->phase);
            return;
        }

        RenderPipelineReloadChunkGateSnapshot snapshot;
        if (m_worldChunkParticipant != nullptr)
        {
            snapshot = m_worldChunkParticipant->GetReloadChunkGateSnapshot(m_activeTransaction->generation);
        }
        else
        {
            snapshot.generation = m_activeTransaction->generation;
            snapshot.completionSatisfied = true;
        }

        m_activeTransaction->chunkGateSnapshot = snapshot;
        m_diagnostics.RecordChunkGateSnapshot(snapshot);

        if (!snapshot.completionSatisfied)
        {
            return;
        }

        commitActiveTransaction();
    }

    void RenderPipelineReloadCoordinator::SubscribeRendererEvents()
    {
        if (m_beginFrameHandle == 0)
        {
            m_beginFrameHandle = RendererEvents::OnBeginFrame.Add(
                this,
                &RenderPipelineReloadCoordinator::OnRendererBeginFrame);
        }

        if (m_frameSlotAcquiredHandle == 0)
        {
            m_frameSlotAcquiredHandle = RendererEvents::OnFrameSlotAcquired.Add(
                this,
                &RenderPipelineReloadCoordinator::OnRendererFrameSlotAcquired);
        }
    }

    void RenderPipelineReloadCoordinator::UnsubscribeRendererEvents()
    {
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

    void RenderPipelineReloadCoordinator::SetBundleOnLoadBusy(bool isBusy) noexcept
    {
        m_requestGate.SetBundleOnLoadBusy(isBusy);
    }

    void RenderPipelineReloadCoordinator::SetBundleApplyBusy(bool isBusy) noexcept
    {
        m_requestGate.SetBundleApplyBusy(isBusy);
    }

    void RenderPipelineReloadCoordinator::SetFrameSlotMutationBusy(bool isBusy) noexcept
    {
        m_requestGate.SetFrameSlotMutationBusy(isBusy);
    }

    bool RenderPipelineReloadCoordinator::HasActiveTransaction() const noexcept
    {
        return m_activeTransaction.has_value() && m_activeTransaction->IsActive();
    }

    bool RenderPipelineReloadCoordinator::ShouldSuppressWorldRendering() const noexcept
    {
        return m_activeTransaction.has_value() &&
               (m_activeTransaction->phase == RenderPipelineReloadPhase::WaitingForChunkGate ||
                m_activeTransaction->phase == RenderPipelineReloadPhase::Committing);
    }

    bool RenderPipelineReloadCoordinator::IsSubscribedToRendererEvents() const noexcept
    {
        return m_beginFrameHandle != 0 || m_frameSlotAcquiredHandle != 0;
    }

    RenderPipelineReloadGeneration RenderPipelineReloadCoordinator::GetCurrentGeneration() const noexcept
    {
        return m_currentGeneration;
    }

    const RenderPipelineReloadDiagnostics& RenderPipelineReloadCoordinator::GetDiagnostics() const noexcept
    {
        return m_diagnostics;
    }

    RenderPipelineReloadDiagnosticsSnapshot RenderPipelineReloadCoordinator::GetDiagnosticsSnapshot() const
    {
        return m_diagnostics.BuildSnapshot();
    }

    const RenderPipelineReloadRequestGate& RenderPipelineReloadCoordinator::GetRequestGate() const noexcept
    {
        return m_requestGate;
    }

    const std::optional<RenderPipelineReloadTransaction>& RenderPipelineReloadCoordinator::GetActiveTransaction() const noexcept
    {
        return m_activeTransaction;
    }

    bool RenderPipelineReloadCoordinator::acceptRequest(RenderPipelineReloadRequest request)
    {
        const RenderPipelineReloadRequestDecision decision = m_requestGate.TryAccept(request);
        if (!decision.accepted)
        {
            m_diagnostics.RecordIgnoredRequest(request, decision.ignoredReason);
            return false;
        }

        if (!tryAcquireMutationGate(request))
        {
            m_requestGate.MarkIgnored(request, RenderPipelineReloadIgnoredReason::RendererFrontendMutationGateBusy);
            m_requestGate.MarkIdle();
            m_diagnostics.RecordIgnoredRequest(
                request,
                RenderPipelineReloadIgnoredReason::RendererFrontendMutationGateBusy);
            return false;
        }

        RenderPipelineReloadTransaction transaction;
        transaction.id = allocateTransactionId();
        transaction.generation = m_currentGeneration;
        transaction.phase = RenderPipelineReloadPhase::RequestAccepted;
        transaction.request = std::move(request);

        m_requestGate.MarkActive(transaction.id);
        m_diagnostics.RecordActiveRequest(
            transaction.id,
            transaction.request,
            transaction.generation,
            transaction.phase);
        m_diagnostics.RecordPhaseTransition(
            transaction.id,
            transaction.generation,
            RenderPipelineReloadPhase::Idle,
            RenderPipelineReloadPhase::RequestAccepted,
            "Request accepted by render pipeline reload coordinator");

        m_activeTransaction = transaction;
        RenderPipelineReloadEvents::OnTransactionStarted.Broadcast(transaction.id, transaction.generation);
        return true;
    }

    void RenderPipelineReloadCoordinator::transitionActiveTransaction(
        RenderPipelineReloadPhase nextPhase,
        const char* reason)
    {
        if (!m_activeTransaction.has_value())
        {
            return;
        }

        const RenderPipelineReloadPhase previousPhase = m_activeTransaction->phase;
        if (previousPhase == nextPhase)
        {
            m_diagnostics.SetActivePhase(nextPhase);
            return;
        }

        m_activeTransaction->phase = nextPhase;
        m_diagnostics.RecordPhaseTransition(
            m_activeTransaction->id,
            m_activeTransaction->generation,
            previousPhase,
            nextPhase,
            reason ? reason : "");
    }

    void RenderPipelineReloadCoordinator::executeQueueSynchronization()
    {
        if (!m_activeTransaction.has_value())
        {
            return;
        }

        transitionActiveTransaction(
            RenderPipelineReloadPhase::QueueSynchronizing,
            "Synchronizing active queues before render pipeline reload");

        const QueueSubmittedFenceSnapshot submittedBeforeSync =
            D3D12RenderSystem::GetLastSubmittedFenceSnapshot();
        const QueueFenceSnapshot completedBeforeSync =
            D3D12RenderSystem::GetCompletedFenceSnapshot();

        if (!D3D12RenderSystem::SynchronizeActiveQueues("RenderPipelineReload"))
        {
            throw ReloadQueueSyncException(
                "Render pipeline reload failed while synchronizing active queues",
                RenderPipelineReloadFailureSeverity::Fatal);
        }

        const QueueFenceSnapshot completedAfterSync =
            D3D12RenderSystem::GetCompletedFenceSnapshot();

        m_activeTransaction->submittedBeforeSync = submittedBeforeSync;
        m_activeTransaction->completedBeforeSync = completedBeforeSync;
        m_activeTransaction->completedAfterSync = completedAfterSync;
        m_diagnostics.RecordQueueSyncSnapshots(
            submittedBeforeSync,
            completedBeforeSync,
            completedAfterSync);
    }

    void RenderPipelineReloadCoordinator::executeFrameSlotMutation()
    {
        if (!m_activeTransaction.has_value())
        {
            return;
        }

        if (m_bundleAdapter == nullptr)
        {
            throw ReloadFrontendApplyException("Render pipeline reload has no shader bundle reload adapter");
        }

        if (m_frontendReloadService == nullptr)
        {
            throw ReloadFrontendApplyException("Render pipeline reload has no renderer frontend reload service");
        }

        transitionActiveTransaction(
            RenderPipelineReloadPhase::FrameSlotMutation,
            "Applying prepared shader bundle at frame-slot boundary");

        m_activeTransaction->previousStableBundle = m_bundleAdapter->CaptureStableSnapshot();

        ShaderBundleReloadAdapter::PreparedShaderBundle prepared =
            m_activeTransaction->request.IsLoadRequest()
                ? m_bundleAdapter->PrepareLoad(m_activeTransaction->request.shaderBundleMeta.value())
                : m_bundleAdapter->PrepareUnloadToEngine();

        RendererFrontendReloadScope scope = m_bundleAdapter->BuildFrontendReloadScope(prepared);
        m_frontendReloadService->ApplyReloadScope(scope);

        transitionActiveTransaction(
            RenderPipelineReloadPhase::FrontendApplied,
            "Renderer frontend reload scope applied");
        RenderPipelineReloadEvents::OnFrontendApplied.Broadcast(
            m_activeTransaction->id,
            m_activeTransaction->generation);

        m_bundleAdapter->ApplyPreparedBundle(std::move(prepared));

        ++m_currentGeneration.value;
        m_activeTransaction->generation = m_currentGeneration;
        m_diagnostics.SetActiveGeneration(m_currentGeneration);
        beginWorldReloadGeneration();

        transitionActiveTransaction(
            RenderPipelineReloadPhase::WaitingForChunkGate,
            "Waiting for reload chunk generation gate");
    }

    void RenderPipelineReloadCoordinator::commitActiveTransaction()
    {
        if (!m_activeTransaction.has_value())
        {
            return;
        }

        if (m_bundleAdapter == nullptr)
        {
            throw ReloadFrontendApplyException("Render pipeline reload cannot commit without shader bundle reload adapter");
        }

        transitionActiveTransaction(
            RenderPipelineReloadPhase::Committing,
            "Committing render pipeline reload transaction");

        m_bundleAdapter->SaveCommittedRequest(m_activeTransaction->request);
        endWorldReloadGeneration();

        if (m_activeTransaction->request.IsUnloadRequest())
        {
            ShaderBundleEvents::OnBundleUnloaded.Broadcast();
        }

        StableShaderBundleSnapshot committedSnapshot = m_bundleAdapter->CaptureStableSnapshot();
        if (committedSnapshot.bundle != nullptr)
        {
            ShaderBundleEvents::OnBundleLoaded.Broadcast(committedSnapshot.bundle.get());
        }

        transitionActiveTransaction(
            RenderPipelineReloadPhase::Completed,
            "Render pipeline reload transaction committed");
        RenderPipelineReloadEvents::OnTransactionCommitted.Broadcast(
            m_activeTransaction->id,
            m_activeTransaction->generation);

        completeActiveTransaction();
    }

    void RenderPipelineReloadCoordinator::completeActiveTransaction()
    {
        releaseMutationGateIfOwned();
        m_requestGate.MarkIdle();
        m_diagnostics.ClearActiveRequest();
        m_activeTransaction.reset();
    }

    void RenderPipelineReloadCoordinator::failActiveTransaction(const RenderPipelineReloadException& exception)
    {
        if (!m_activeTransaction.has_value())
        {
            return;
        }

        RenderPipelineReloadFailure failure;
        failure.severity = exception.GetSeverity();
        failure.exceptionType = "RenderPipelineReloadException";
        failure.message = exception.what();
        failure.failedPhase = ToString(exception.GetPhase());
        failure.recoveryAction = exception.GetRecoveryAction();

        m_activeTransaction->failure = failure;
        endWorldReloadGeneration();
        transitionActiveTransaction(
            RenderPipelineReloadPhase::Failed,
            "Render pipeline reload transaction failed");
        m_diagnostics.RecordFailure(failure);

        if (failure.severity == RenderPipelineReloadFailureSeverity::Recoverable)
        {
            try
            {
                rollbackToPreviousStableBundle();
            }
            catch (const RenderPipelineReloadException& rollbackException)
            {
                RenderPipelineReloadFailure rollbackFailure;
                rollbackFailure.severity = rollbackException.GetSeverity();
                rollbackFailure.exceptionType = "ReloadRollbackException";
                rollbackFailure.message = rollbackException.what();
                rollbackFailure.failedPhase = ToString(rollbackException.GetPhase());
                rollbackFailure.recoveryAction = rollbackException.GetRecoveryAction();

                m_activeTransaction->failure = rollbackFailure;
                m_diagnostics.RecordFailure(rollbackFailure);
                RenderPipelineReloadEvents::OnTransactionFailed.Broadcast(
                    m_activeTransaction->id,
                    rollbackFailure);
                releaseMutationGateIfOwned();
                m_requestGate.MarkIdle();
                ERROR_AND_DIE(rollbackFailure.message)
                return;
            }

            RenderPipelineReloadEvents::OnTransactionFailed.Broadcast(
                m_activeTransaction->id,
                failure);
            releaseMutationGateIfOwned();
            m_requestGate.MarkIdle();
            ERROR_RECOVERABLE(failure.message);
            return;
        }

        if (failure.severity == RenderPipelineReloadFailureSeverity::Fatal)
        {
            RenderPipelineReloadEvents::OnTransactionFailed.Broadcast(
                m_activeTransaction->id,
                failure);
            releaseMutationGateIfOwned();
            m_requestGate.MarkIdle();
            ERROR_AND_DIE(failure.message)
            return;
        }
    }

    bool RenderPipelineReloadCoordinator::tryAcquireMutationGate(const RenderPipelineReloadRequest& request)
    {
        RendererFrontendMutationGate& gate = GetRendererFrontendMutationGate();
        if (gate.TryBegin(RendererFrontendMutationOwner::ShaderBundleReload, request.debugName.c_str()))
        {
            m_hasMutationGate = true;
            return true;
        }

        const RendererFrontendMutationGateSnapshot snapshot = gate.GetDiagnosticsSnapshot();
        m_diagnostics.SetBlockingReason(Stringf(
            "Renderer frontend mutation gate busy: owner=%s reason=%s",
            ToString(snapshot.owner),
            snapshot.reason.c_str()));
        return false;
    }

    void RenderPipelineReloadCoordinator::releaseMutationGateIfOwned()
    {
        if (!m_hasMutationGate)
        {
            return;
        }

        GetRendererFrontendMutationGate().End(RendererFrontendMutationOwner::ShaderBundleReload);
        m_hasMutationGate = false;
    }

    void RenderPipelineReloadCoordinator::rollbackToPreviousStableBundle()
    {
        if (!m_activeTransaction.has_value() ||
            m_bundleAdapter == nullptr ||
            m_frontendReloadService == nullptr ||
            !m_activeTransaction->previousStableBundle.IsValid())
        {
            return;
        }

        try
        {
            ShaderBundleReloadAdapter::PreparedShaderBundle prepared;
            prepared.action = m_activeTransaction->previousStableBundle.wasEngineBundle
                                  ? ShaderBundleReloadAdapter::PreparedShaderBundleAction::UnloadToEngine
                                  : ShaderBundleReloadAdapter::PreparedShaderBundleAction::Load;
            prepared.bundle = m_activeTransaction->previousStableBundle.bundle;
            prepared.meta = m_activeTransaction->previousStableBundle.meta;
            prepared.debugName = m_activeTransaction->previousStableBundle.debugName;

            RendererFrontendReloadScope rollbackScope = m_bundleAdapter->BuildFrontendReloadScope(prepared);
            m_frontendReloadService->ApplyReloadScope(rollbackScope);
            m_bundleAdapter->RollbackToStableBundle(m_activeTransaction->previousStableBundle);
        }
        catch (const std::exception& e)
        {
            throw ReloadRollbackException(Stringf(
                "Render pipeline reload rollback failed: %s",
                e.what()));
        }
    }

    void RenderPipelineReloadCoordinator::beginWorldReloadGeneration()
    {
        if (!m_activeTransaction.has_value() || m_worldChunkParticipant == nullptr)
        {
            return;
        }

        m_worldChunkParticipant->BeginReloadGeneration(m_activeTransaction->generation);
        m_worldChunkParticipant->MarkLoadedChunksForReload(m_activeTransaction->generation);
    }

    void RenderPipelineReloadCoordinator::endWorldReloadGeneration()
    {
        if (!m_activeTransaction.has_value() || m_worldChunkParticipant == nullptr)
        {
            return;
        }

        m_worldChunkParticipant->EndReloadGeneration(m_activeTransaction->generation);
    }

    bool RenderPipelineReloadCoordinator::isCurrentStableBundle(const ShaderBundleMeta& meta) const
    {
        if (m_bundleAdapter == nullptr)
        {
            return false;
        }

        const StableShaderBundleSnapshot snapshot = m_bundleAdapter->CaptureStableSnapshot();
        if (!snapshot.bundle)
        {
            return false;
        }

        return IsSameShaderBundleMeta(snapshot.bundle->GetMeta(), meta);
    }

    bool RenderPipelineReloadCoordinator::ignoreRequestBeforeGate(
        const RenderPipelineReloadRequest& request,
        RenderPipelineReloadIgnoredReason reason)
    {
        m_requestGate.MarkIgnored(request, reason);
        m_diagnostics.RecordIgnoredRequest(request, reason);
        return true;
    }

    RenderPipelineReloadTransactionId RenderPipelineReloadCoordinator::allocateTransactionId() noexcept
    {
        return m_nextTransactionId++;
    }

    uint64_t RenderPipelineReloadCoordinator::allocateRequestId() noexcept
    {
        return m_nextRequestId++;
    }
} // namespace enigma::graphic
