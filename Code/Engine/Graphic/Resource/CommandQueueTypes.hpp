#pragma once

#include <array>
#include <cstdint>

namespace enigma::graphic
{
    enum class CommandQueueType
    {
        Graphics,
        Compute,
        Copy
    };

    enum class QueueExecutionMode
    {
        GraphicsOnly,
        MixedQueueExperimental
    };

    enum class QueueWorkloadClass
    {
        Unknown,
        FrameGraphics,
        ImmediateGraphics,
        Present,
        ImGui,
        GraphicsStatefulUpload,
        CopyReadyUpload,
        MipmapGeneration
    };

    enum class QueueFallbackReason
    {
        None,
        GraphicsOnlyMode,
        RouteNotValidated,
        UnsupportedWorkload,
        RequiresGraphicsStateTransition,
        DedicatedQueueUnavailable,
        QueueTypeUnavailable,
        ResourceStateNotSupported
    };

    enum class FrameLifecyclePhase
    {
        Idle,
        PreFrameBegin,
        RetiringFrameSlot,
        FrameSlotAcquired,
        RecordingFrame,
        SubmittingFrame
    };

    static constexpr size_t kCommandQueueTypeCount = 3;

    constexpr size_t GetCommandQueueTypeIndex(CommandQueueType queueType) noexcept
    {
        switch (queueType)
        {
        case CommandQueueType::Graphics:
            return 0;
        case CommandQueueType::Compute:
            return 1;
        case CommandQueueType::Copy:
            return 2;
        }

        return 0;
    }

    struct QueueSubmissionToken
    {
        CommandQueueType queueType  = CommandQueueType::Graphics;
        uint64_t         fenceValue = 0;

        constexpr bool IsValid() const noexcept
        {
            return fenceValue != 0;
        }

        constexpr void Reset() noexcept
        {
            queueType  = CommandQueueType::Graphics;
            fenceValue = 0;
        }

        constexpr bool operator==(const QueueSubmissionToken& other) const noexcept
        {
            return queueType == other.queueType && fenceValue == other.fenceValue;
        }

        constexpr bool operator!=(const QueueSubmissionToken& other) const noexcept
        {
            return !(*this == other);
        }
    };

    struct QueueFenceSnapshot
    {
        uint64_t graphicsCompleted = 0;
        uint64_t computeCompleted  = 0;
        uint64_t copyCompleted     = 0;

        constexpr uint64_t GetCompletedFenceValue(CommandQueueType queueType) const noexcept
        {
            switch (queueType)
            {
            case CommandQueueType::Graphics:
                return graphicsCompleted;
            case CommandQueueType::Compute:
                return computeCompleted;
            case CommandQueueType::Copy:
                return copyCompleted;
            }

            return 0;
        }
    };

    struct QueueSubmittedFenceSnapshot
    {
        uint64_t graphicsSubmitted = 0;
        uint64_t computeSubmitted  = 0;
        uint64_t copySubmitted     = 0;

        constexpr uint64_t GetSubmittedFenceValue(CommandQueueType queueType) const noexcept
        {
            switch (queueType)
            {
            case CommandQueueType::Graphics:
                return graphicsSubmitted;
            case CommandQueueType::Compute:
                return computeSubmitted;
            case CommandQueueType::Copy:
                return copySubmitted;
            }

            return 0;
        }

        constexpr bool IsSatisfiedBy(const QueueFenceSnapshot& completedSnapshot) const noexcept
        {
            return completedSnapshot.graphicsCompleted >= graphicsSubmitted &&
                   completedSnapshot.computeCompleted >= computeSubmitted &&
                   completedSnapshot.copyCompleted >= copySubmitted;
        }

        constexpr bool HasTrackedWork() const noexcept
        {
            return graphicsSubmitted != 0 || computeSubmitted != 0 || copySubmitted != 0;
        }
    };

    struct FrameQueueRetirementState
    {
        QueueSubmissionToken graphicsToken = {};
        QueueSubmissionToken computeToken  = {};
        QueueSubmissionToken copyToken     = {};
        std::array<bool, kCommandQueueTypeCount> expectedQueues = {};

        void ExpectQueue(CommandQueueType queueType) noexcept
        {
            expectedQueues[GetCommandQueueTypeIndex(queueType)] = true;
        }

        void RegisterSubmission(const QueueSubmissionToken& token) noexcept
        {
            switch (token.queueType)
            {
            case CommandQueueType::Graphics:
                graphicsToken = token;
                break;
            case CommandQueueType::Compute:
                computeToken = token;
                break;
            case CommandQueueType::Copy:
                copyToken = token;
                break;
            }
        }

        void Clear() noexcept
        {
            graphicsToken.Reset();
            computeToken.Reset();
            copyToken.Reset();
            expectedQueues = {};
        }

        std::array<QueueSubmissionToken, 3> GetTrackedTokens() const noexcept
        {
            return { graphicsToken, computeToken, copyToken };
        }

        std::array<bool, kCommandQueueTypeCount> GetTrackedQueues() const noexcept
        {
            return {
                graphicsToken.IsValid(),
                computeToken.IsValid(),
                copyToken.IsValid()
            };
        }

        std::array<bool, kCommandQueueTypeCount> GetMissingQueues() const noexcept
        {
            const std::array<bool, kCommandQueueTypeCount> trackedQueues = GetTrackedQueues();
            return {
                expectedQueues[0] && !trackedQueues[0],
                expectedQueues[1] && !trackedQueues[1],
                expectedQueues[2] && !trackedQueues[2]
            };
        }

        bool HasTrackedSubmissions() const noexcept
        {
            return graphicsToken.IsValid() || computeToken.IsValid() || copyToken.IsValid();
        }

        bool HasMissingRegistrations() const noexcept
        {
            const std::array<bool, kCommandQueueTypeCount> missingQueues = GetMissingQueues();
            return missingQueues[0] || missingQueues[1] || missingQueues[2];
        }
    };

    struct QueueRouteContext
    {
        QueueWorkloadClass workload                         = QueueWorkloadClass::Unknown;
        bool               isCopySafe                      = false;
        bool               requiresGraphicsStateTransition = false;
        bool               prefersAsyncExecution           = false;
        bool               allowGraphicsFallback           = true;
        QueueFallbackReason forcedFallbackReason           = QueueFallbackReason::None;
    };

    struct QueueRouteDecision
    {
        QueueWorkloadClass workload        = QueueWorkloadClass::Unknown;
        CommandQueueType   requestedQueue  = CommandQueueType::Graphics;
        CommandQueueType   activeQueue     = CommandQueueType::Graphics;
        QueueFallbackReason fallbackReason = QueueFallbackReason::None;

        bool UsesFallback() const noexcept
        {
            return requestedQueue != activeQueue || fallbackReason != QueueFallbackReason::None;
        }
    };

    struct QueueExecutionDiagnostics
    {
        QueueExecutionMode requestedMode      = QueueExecutionMode::GraphicsOnly;
        QueueExecutionMode activeMode         = QueueExecutionMode::GraphicsOnly;
        FrameLifecyclePhase lifecyclePhase    = FrameLifecyclePhase::Idle;
        uint32_t           requestedFramesInFlightDepth = 0;
        uint32_t           activeFramesInFlightDepth    = 0;
        uint64_t           graphicsSubmissions = 0;
        uint64_t           computeSubmissions  = 0;
        uint64_t           copySubmissions     = 0;
        uint64_t           queueWaitInsertions = 0;
        bool               lastBeginFrameHadTrackedRetirement = false;
        bool               lastBeginFrameWaitedOnRetirement   = false;
        uint32_t           lastWaitedFrameSlot = 0;
        QueueFenceSnapshot lastCompletedFenceSnapshotBeforeWait = {};
        QueueSubmissionToken lastRetirementGraphicsToken = {};
        QueueSubmissionToken lastRetirementComputeToken  = {};
        QueueSubmissionToken lastRetirementCopyToken     = {};
        std::array<bool, kCommandQueueTypeCount> lastRetirementParticipatingQueues = {};
        std::array<bool, kCommandQueueTypeCount> lastRetirementExpectedQueues = {};
        std::array<bool, kCommandQueueTypeCount> lastRetirementMissingQueues = {};
        std::array<bool, kCommandQueueTypeCount> lastRetirementWaitedQueues = {};
        std::array<std::array<uint64_t, 3>, 8> workloadSubmissionsByQueue = {};
        std::array<std::array<uint64_t, 3>, 3> queueWaitsByProducerConsumer = {};
        QueueFallbackReason lastFallbackReason = QueueFallbackReason::None;

        void RecordSubmission(QueueWorkloadClass workload, CommandQueueType queueType) noexcept
        {
            switch (queueType)
            {
            case CommandQueueType::Graphics:
                ++graphicsSubmissions;
                break;
            case CommandQueueType::Compute:
                ++computeSubmissions;
                break;
            case CommandQueueType::Copy:
                ++copySubmissions;
                break;
            }

            const size_t workloadIndex = static_cast<size_t>(workload);
            const size_t queueIndex = static_cast<size_t>(queueType);
            if (workloadIndex < workloadSubmissionsByQueue.size() &&
                queueIndex < workloadSubmissionsByQueue[workloadIndex].size())
            {
                ++workloadSubmissionsByQueue[workloadIndex][queueIndex];
            }
        }

        void RecordQueueWait(CommandQueueType producerQueue, CommandQueueType consumerQueue) noexcept
        {
            ++queueWaitInsertions;

            const size_t producerIndex = static_cast<size_t>(producerQueue);
            const size_t consumerIndex = static_cast<size_t>(consumerQueue);
            if (producerIndex < queueWaitsByProducerConsumer.size() &&
                consumerIndex < queueWaitsByProducerConsumer[producerIndex].size())
            {
                ++queueWaitsByProducerConsumer[producerIndex][consumerIndex];
            }
        }

        uint64_t GetWorkloadSubmissionCount(QueueWorkloadClass workload, CommandQueueType queueType) const noexcept
        {
            const size_t workloadIndex = static_cast<size_t>(workload);
            const size_t queueIndex = static_cast<size_t>(queueType);
            if (workloadIndex >= workloadSubmissionsByQueue.size() ||
                queueIndex >= workloadSubmissionsByQueue[workloadIndex].size())
            {
                return 0;
            }

            return workloadSubmissionsByQueue[workloadIndex][queueIndex];
        }

        uint64_t GetWorkloadSubmissionTotal(QueueWorkloadClass workload) const noexcept
        {
            const size_t workloadIndex = static_cast<size_t>(workload);
            if (workloadIndex >= workloadSubmissionsByQueue.size())
            {
                return 0;
            }

            uint64_t total = 0;
            for (uint64_t count : workloadSubmissionsByQueue[workloadIndex])
            {
                total += count;
            }
            return total;
        }

        uint64_t GetQueueWaitCount(CommandQueueType producerQueue, CommandQueueType consumerQueue) const noexcept
        {
            const size_t producerIndex = static_cast<size_t>(producerQueue);
            const size_t consumerIndex = static_cast<size_t>(consumerQueue);
            if (producerIndex >= queueWaitsByProducerConsumer.size() ||
                consumerIndex >= queueWaitsByProducerConsumer[producerIndex].size())
            {
                return 0;
            }

            return queueWaitsByProducerConsumer[producerIndex][consumerIndex];
        }

        void ClearCounters() noexcept
        {
            lifecyclePhase = FrameLifecyclePhase::Idle;
            requestedFramesInFlightDepth = 0;
            activeFramesInFlightDepth    = 0;
            graphicsSubmissions = 0;
            computeSubmissions  = 0;
            copySubmissions     = 0;
            queueWaitInsertions = 0;
            lastBeginFrameHadTrackedRetirement = false;
            lastBeginFrameWaitedOnRetirement   = false;
            lastWaitedFrameSlot = 0;
            lastCompletedFenceSnapshotBeforeWait = {};
            lastRetirementGraphicsToken = {};
            lastRetirementComputeToken  = {};
            lastRetirementCopyToken     = {};
            lastRetirementParticipatingQueues = {};
            lastRetirementExpectedQueues = {};
            lastRetirementMissingQueues = {};
            lastRetirementWaitedQueues = {};
            workloadSubmissionsByQueue = {};
            queueWaitsByProducerConsumer = {};
            lastFallbackReason  = QueueFallbackReason::None;
        }
    };
}
