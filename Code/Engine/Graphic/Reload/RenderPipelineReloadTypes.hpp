#pragma once

#include "Engine/Graphic/Bundle/ShaderBundleCommon.hpp"
#include "Engine/Graphic/Resource/CommandQueueTypes.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace enigma::graphic
{
    class ShaderBundle;

    enum class RenderPipelineReloadPhase
    {
        Idle,
        RequestAccepted,
        QueueSynchronizing,
        FrameSlotMutation,
        FrontendApplied,
        WaitingForChunkGate,
        Committing,
        Completed,
        Failed
    };

    enum class RenderPipelineReloadRequestType
    {
        LoadShaderBundle,
        UnloadShaderBundle
    };

    enum class RenderPipelineReloadIgnoredReason
    {
        None,
        TransactionActive,
        BundleOnLoadBusy,
        BundleApplyBusy,
        FrameSlotMutationBusy,
        CurrentBundleAlreadyLoaded
    };

    enum class RenderPipelineReloadFailureSeverity
    {
        None,
        Recoverable,
        Fatal
    };

    using RenderPipelineReloadTransactionId = uint64_t;

    struct RenderPipelineReloadGeneration
    {
        uint64_t value = 0;

        constexpr bool IsValid() const noexcept
        {
            return value != 0;
        }

        constexpr void Reset() noexcept
        {
            value = 0;
        }

        constexpr bool operator==(const RenderPipelineReloadGeneration& other) const noexcept
        {
            return value == other.value;
        }

        constexpr bool operator!=(const RenderPipelineReloadGeneration& other) const noexcept
        {
            return !(*this == other);
        }

        constexpr bool operator<(const RenderPipelineReloadGeneration& other) const noexcept
        {
            return value < other.value;
        }
    };

    struct RenderPipelineReloadRequest
    {
        RenderPipelineReloadRequestType  type = RenderPipelineReloadRequestType::LoadShaderBundle;
        std::optional<ShaderBundleMeta>  shaderBundleMeta;
        uint64_t                         requestId = 0;
        std::string                      debugName;

        bool IsLoadRequest() const noexcept
        {
            return type == RenderPipelineReloadRequestType::LoadShaderBundle;
        }

        bool IsUnloadRequest() const noexcept
        {
            return type == RenderPipelineReloadRequestType::UnloadShaderBundle;
        }
    };

    struct RenderPipelineReloadRequestDecision
    {
        bool                              accepted = false;
        RenderPipelineReloadIgnoredReason ignoredReason = RenderPipelineReloadIgnoredReason::None;
        RenderPipelineReloadTransactionId activeTransactionId = 0;

        static RenderPipelineReloadRequestDecision Accepted(RenderPipelineReloadTransactionId transactionId) noexcept
        {
            RenderPipelineReloadRequestDecision decision;
            decision.accepted = true;
            decision.activeTransactionId = transactionId;
            return decision;
        }

        static RenderPipelineReloadRequestDecision Ignored(RenderPipelineReloadIgnoredReason reason,
                                                           RenderPipelineReloadTransactionId activeTransactionId = 0) noexcept
        {
            RenderPipelineReloadRequestDecision decision;
            decision.accepted = false;
            decision.ignoredReason = reason;
            decision.activeTransactionId = activeTransactionId;
            return decision;
        }
    };

    struct RenderPipelineReloadFailure
    {
        RenderPipelineReloadFailureSeverity severity = RenderPipelineReloadFailureSeverity::None;
        std::string                         exceptionType;
        std::string                         message;
        std::string                         failedPhase;
        std::string                         recoveryAction;

        bool HasFailure() const noexcept
        {
            return severity != RenderPipelineReloadFailureSeverity::None;
        }
    };

    struct RenderPipelineReloadChunkGateSnapshot
    {
        RenderPipelineReloadGeneration generation;
        uint32_t                       affectedVisibleChunks = 0;
        uint32_t                       rebuiltAndPublishedChunks = 0;
        uint32_t                       pendingDispatchCount = 0;
        uint32_t                       activeWorkerCount = 0;
        uint32_t                       waitingForNeighborsCount = 0;
        uint32_t                       partialMeshPublishedCount = 0;
        uint32_t                       refinementPendingCount = 0;
        bool                           completionSatisfied = false;
        std::string                    blockingReason;
    };

    struct StableShaderBundleSnapshot
    {
        std::shared_ptr<ShaderBundle>      bundle;
        std::optional<ShaderBundleMeta>    meta;
        RenderPipelineReloadGeneration     generation;
        bool                               wasEngineBundle = false;
        std::string                        debugName;

        bool IsValid() const noexcept
        {
            return bundle != nullptr || meta.has_value();
        }
    };

    struct RenderPipelineReloadTransaction
    {
        RenderPipelineReloadTransactionId id = 0;
        RenderPipelineReloadGeneration    generation;
        RenderPipelineReloadPhase         phase = RenderPipelineReloadPhase::Idle;
        RenderPipelineReloadRequest       request;
        StableShaderBundleSnapshot        previousStableBundle;
        QueueSubmittedFenceSnapshot       submittedBeforeSync;
        QueueFenceSnapshot                completedBeforeSync;
        QueueFenceSnapshot                completedAfterSync;
        RenderPipelineReloadChunkGateSnapshot chunkGateSnapshot;
        RenderPipelineReloadFailure       failure;

        bool IsActive() const noexcept
        {
            return phase != RenderPipelineReloadPhase::Idle &&
                   phase != RenderPipelineReloadPhase::Completed &&
                   phase != RenderPipelineReloadPhase::Failed;
        }
    };

    inline const char* ToString(RenderPipelineReloadPhase phase) noexcept
    {
        switch (phase)
        {
        case RenderPipelineReloadPhase::Idle:
            return "Idle";
        case RenderPipelineReloadPhase::RequestAccepted:
            return "RequestAccepted";
        case RenderPipelineReloadPhase::QueueSynchronizing:
            return "QueueSynchronizing";
        case RenderPipelineReloadPhase::FrameSlotMutation:
            return "FrameSlotMutation";
        case RenderPipelineReloadPhase::FrontendApplied:
            return "FrontendApplied";
        case RenderPipelineReloadPhase::WaitingForChunkGate:
            return "WaitingForChunkGate";
        case RenderPipelineReloadPhase::Committing:
            return "Committing";
        case RenderPipelineReloadPhase::Completed:
            return "Completed";
        case RenderPipelineReloadPhase::Failed:
            return "Failed";
        }

        return "Unknown";
    }

    inline const char* ToString(RenderPipelineReloadRequestType type) noexcept
    {
        switch (type)
        {
        case RenderPipelineReloadRequestType::LoadShaderBundle:
            return "LoadShaderBundle";
        case RenderPipelineReloadRequestType::UnloadShaderBundle:
            return "UnloadShaderBundle";
        }

        return "Unknown";
    }

    inline const char* ToString(RenderPipelineReloadIgnoredReason reason) noexcept
    {
        switch (reason)
        {
        case RenderPipelineReloadIgnoredReason::None:
            return "None";
        case RenderPipelineReloadIgnoredReason::TransactionActive:
            return "TransactionActive";
        case RenderPipelineReloadIgnoredReason::BundleOnLoadBusy:
            return "BundleOnLoadBusy";
        case RenderPipelineReloadIgnoredReason::BundleApplyBusy:
            return "BundleApplyBusy";
        case RenderPipelineReloadIgnoredReason::FrameSlotMutationBusy:
            return "FrameSlotMutationBusy";
        case RenderPipelineReloadIgnoredReason::CurrentBundleAlreadyLoaded:
            return "CurrentBundleAlreadyLoaded";
        }

        return "Unknown";
    }

    inline const char* ToString(RenderPipelineReloadFailureSeverity severity) noexcept
    {
        switch (severity)
        {
        case RenderPipelineReloadFailureSeverity::None:
            return "None";
        case RenderPipelineReloadFailureSeverity::Recoverable:
            return "Recoverable";
        case RenderPipelineReloadFailureSeverity::Fatal:
            return "Fatal";
        }

        return "Unknown";
    }
} // namespace enigma::graphic
