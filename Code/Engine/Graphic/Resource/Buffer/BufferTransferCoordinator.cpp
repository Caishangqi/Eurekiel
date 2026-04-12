#include "Engine/Graphic/Resource/Buffer/BufferTransferCoordinator.hpp"

#include <cstring>
#include <limits>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"

namespace enigma::graphic
{
    namespace
    {
        const char* GetQueueTypeName(CommandQueueType queueType)
        {
            switch (queueType)
            {
            case CommandQueueType::Graphics:
                return "Graphics";
            case CommandQueueType::Compute:
                return "Compute";
            case CommandQueueType::Copy:
                return "Copy";
            }

            return "Unknown";
        }

        const char* GetTransferModeName(bool isUploadBatch)
        {
            return isUploadBatch ? "Upload" : "Copy";
        }

        bool TryComputeTotalUploadBytes(const BufferTransferBatch& batch, uint64_t& outTotalBytes)
        {
            uint64_t totalBytes = 0;
            for (const BufferUploadSpan& uploadSpan : batch.uploadSpans)
            {
                if ((std::numeric_limits<uint64_t>::max)() - totalBytes < uploadSpan.sizeInBytes)
                {
                    return false;
                }

                totalBytes += uploadSpan.sizeInBytes;
            }

            outTotalBytes = totalBytes;
            return true;
        }

        bool RecordTransitions(ID3D12GraphicsCommandList*                 commandList,
                               const std::vector<BufferTransferTransition>& transitions,
                               bool                                        useFinalState)
        {
            if (commandList == nullptr)
            {
                ERROR_RECOVERABLE("BufferTransferCoordinator: command list is null during transition recording");
                return false;
            }

            for (const BufferTransferTransition& transition : transitions)
            {
                if (!transition.IsValid())
                {
                    ERROR_RECOVERABLE("BufferTransferCoordinator: transition references a null buffer");
                    return false;
                }

                D12Buffer* const buffer = transition.buffer;
                if (buffer->GetResource() == nullptr)
                {
                    ERROR_RECOVERABLE("BufferTransferCoordinator: transition buffer resource is null");
                    return false;
                }

                const D3D12_RESOURCE_STATES targetState =
                    useFinalState ? transition.finalState : transition.transferState;
                const D3D12_RESOURCE_STATES currentState = buffer->GetCurrentState();
                if (currentState == targetState)
                {
                    continue;
                }

                const char* const debugName =
                    transition.debugName != nullptr ? transition.debugName : buffer->GetDebugName().c_str();

                D3D12RenderSystem::TransitionResource(
                    commandList,
                    buffer->GetResource(),
                    currentState,
                    targetState,
                    debugName);
                buffer->SetCurrentState(targetState);
            }

            return true;
        }

        bool RecordUploadSpans(ID3D12GraphicsCommandList*         commandList,
                               UploadContext&                     uploadContext,
                               const std::vector<BufferUploadSpan>& uploadSpans)
        {
            if (commandList == nullptr || !uploadContext.IsValid())
            {
                ERROR_RECOVERABLE("BufferTransferCoordinator: upload batch requires a valid command list and staging buffer");
                return false;
            }

            unsigned char* const mappedData = static_cast<unsigned char*>(uploadContext.GetMappedData());
            if (mappedData == nullptr)
            {
                ERROR_RECOVERABLE("BufferTransferCoordinator: upload staging buffer is not mapped");
                return false;
            }

            ID3D12Resource* const uploadBuffer = uploadContext.GetUploadBuffer();
            uint64_t              uploadOffset = 0;
            for (const BufferUploadSpan& uploadSpan : uploadSpans)
            {
                if (!uploadSpan.IsValid())
                {
                    ERROR_RECOVERABLE("BufferTransferCoordinator: upload span is invalid");
                    return false;
                }

                if (uploadSpan.destinationBuffer->GetResource() == nullptr)
                {
                    ERROR_RECOVERABLE("BufferTransferCoordinator: upload span destination resource is null");
                    return false;
                }

                std::memcpy(mappedData + uploadOffset, uploadSpan.sourceData, static_cast<size_t>(uploadSpan.sizeInBytes));
                commandList->CopyBufferRegion(
                    uploadSpan.destinationBuffer->GetResource(),
                    uploadSpan.destinationOffset,
                    uploadBuffer,
                    uploadOffset,
                    uploadSpan.sizeInBytes);

                uploadOffset += uploadSpan.sizeInBytes;
            }

            return true;
        }

        bool RecordCopySpans(ID3D12GraphicsCommandList*         commandList,
                             const std::vector<BufferCopySpan>& copySpans)
        {
            if (commandList == nullptr)
            {
                ERROR_RECOVERABLE("BufferTransferCoordinator: copy batch requires a valid command list");
                return false;
            }

            for (const BufferCopySpan& copySpan : copySpans)
            {
                if (!copySpan.IsValid())
                {
                    ERROR_RECOVERABLE("BufferTransferCoordinator: copy span is invalid");
                    return false;
                }

                if (copySpan.sourceBuffer->GetResource() == nullptr ||
                    copySpan.destinationBuffer->GetResource() == nullptr)
                {
                    ERROR_RECOVERABLE("BufferTransferCoordinator: copy span references a null resource");
                    return false;
                }

                commandList->CopyBufferRegion(
                    copySpan.destinationBuffer->GetResource(),
                    copySpan.destinationOffset,
                    copySpan.sourceBuffer->GetResource(),
                    copySpan.sourceOffset,
                    copySpan.sizeInBytes);
            }

            return true;
        }

    }

    bool BufferUploadSpan::IsValid() const noexcept
    {
        return destinationBuffer != nullptr && sourceData != nullptr && sizeInBytes > 0;
    }

    bool BufferCopySpan::IsValid() const noexcept
    {
        return sourceBuffer != nullptr &&
               destinationBuffer != nullptr &&
               sizeInBytes > 0;
    }

    uint64_t BufferTransferBatch::GetTotalUploadBytes() const noexcept
    {
        uint64_t totalBytes = 0;
        for (const BufferUploadSpan& uploadSpan : uploadSpans)
        {
            totalBytes += uploadSpan.sizeInBytes;
        }

        return totalBytes;
    }

    bool BufferTransferBatch::HasUploadWork() const noexcept
    {
        return !uploadSpans.empty();
    }

    bool BufferTransferBatch::HasCopyWork() const noexcept
    {
        return !copySpans.empty();
    }

    bool BufferTransferBatch::HasTransitions() const noexcept
    {
        return !preTransferTransitions.empty() || !postTransferTransitions.empty();
    }

    bool BufferTransferBatch::IsValidForUpload() const noexcept
    {
        if (workload == QueueWorkloadClass::Unknown ||
            preferredQueue == CommandQueueType::Compute ||
            !HasUploadWork() ||
            HasCopyWork())
        {
            return false;
        }

        for (const BufferTransferTransition& transition : preTransferTransitions)
        {
            if (!transition.IsValid())
            {
                return false;
            }
        }

        for (const BufferTransferTransition& transition : postTransferTransitions)
        {
            if (!transition.IsValid())
            {
                return false;
            }
        }

        for (const BufferUploadSpan& uploadSpan : uploadSpans)
        {
            if (!uploadSpan.IsValid())
            {
                return false;
            }
        }

        return true;
    }

    bool BufferTransferBatch::IsValidForCopy() const noexcept
    {
        if (workload == QueueWorkloadClass::Unknown ||
            preferredQueue == CommandQueueType::Compute ||
            HasUploadWork() ||
            !HasCopyWork())
        {
            return false;
        }

        for (const BufferTransferTransition& transition : preTransferTransitions)
        {
            if (!transition.IsValid())
            {
                return false;
            }
        }

        for (const BufferTransferTransition& transition : postTransferTransitions)
        {
            if (!transition.IsValid())
            {
                return false;
            }
        }

        for (const BufferCopySpan& copySpan : copySpans)
        {
            if (!copySpan.IsValid())
            {
                return false;
            }
        }

        return true;
    }

    BufferTransferExecution BufferTransferCoordinator::ExecuteUploadBatch(const BufferTransferBatch& batch) const
    {
        return ExecuteBatch(batch, TransferMode::Upload);
    }

    BufferTransferExecution BufferTransferCoordinator::ExecuteCopyBatch(const BufferTransferBatch& batch) const
    {
        return ExecuteBatch(batch, TransferMode::Copy);
    }

    bool BufferTransferCoordinator::WaitForTransferCompletion(BufferTransferExecution& execution) const
    {
        const QueueSubmissionToken completionToken =
            execution.completionToken.IsValid() ? execution.completionToken : execution.transferToken;
        if (!completionToken.IsValid())
        {
            ERROR_RECOVERABLE("BufferTransferCoordinator: transfer completion requested without a valid submission token");
            return false;
        }

        const bool waitSucceeded = D3D12RenderSystem::WaitForStandaloneSubmission(completionToken);
        if (waitSucceeded)
        {
            execution.stagingUploadContext.reset();
        }

        return waitSucceeded;
    }

    BufferTransferExecution BufferTransferCoordinator::ExecuteBatch(const BufferTransferBatch& batch, TransferMode mode) const
    {
        const bool isUploadBatch = mode == TransferMode::Upload;
        if ((isUploadBatch && !batch.IsValidForUpload()) ||
            (!isUploadBatch && !batch.IsValidForCopy()))
        {
            ERROR_RECOVERABLE(Stringf("BufferTransferCoordinator: invalid %s batch", GetTransferModeName(isUploadBatch)));
            return {};
        }

        auto* commandListManager = D3D12RenderSystem::GetCommandListManager();
        if (commandListManager == nullptr)
        {
            ERROR_RECOVERABLE("BufferTransferCoordinator: command list manager is unavailable");
            return {};
        }

        auto* device = D3D12RenderSystem::GetDevice();
        if (device == nullptr)
        {
            ERROR_RECOVERABLE("BufferTransferCoordinator: device is unavailable");
            return {};
        }

        BufferTransferExecution execution = {};
        if (isUploadBatch)
        {
            uint64_t totalUploadBytes = 0;
            if (!TryComputeTotalUploadBytes(batch, totalUploadBytes) ||
                totalUploadBytes == 0 ||
                totalUploadBytes > static_cast<uint64_t>((std::numeric_limits<size_t>::max)()))
            {
                ERROR_RECOVERABLE("BufferTransferCoordinator: upload batch staging size is invalid");
                return {};
            }

            execution.stagingUploadContext = std::make_shared<UploadContext>(device, static_cast<size_t>(totalUploadBytes));
            if (!execution.stagingUploadContext->IsValid())
            {
                ERROR_RECOVERABLE("BufferTransferCoordinator: failed to allocate upload staging buffer");
                return {};
            }
        }

        QueueRouteContext routeContext = {};
        routeContext.workload               = batch.workload;
        routeContext.isCopySafe            = batch.preferredQueue == CommandQueueType::Copy;
        routeContext.prefersAsyncExecution = batch.preferredQueue != CommandQueueType::Graphics;
        routeContext.allowGraphicsFallback = batch.allowGraphicsFallback;
        execution.routeDecision            = D3D12RenderSystem::ResolveQueueRoute(routeContext);

        auto insertQueueWaitOrBlock = [&](const QueueSubmissionToken& producerToken,
                                          CommandQueueType           waitingQueue) -> bool
        {
            if (commandListManager == nullptr)
            {
                ERROR_RECOVERABLE("BufferTransferCoordinator: command list manager is unavailable for queue handoff");
                return false;
            }

            if (commandListManager->InsertQueueWait(waitingQueue, producerToken))
            {
                D3D12RenderSystem::RecordQueueWaitInsertion(producerToken.queueType, waitingQueue);
                return true;
            }

            execution.usedCpuWaitForHandoff = true;
            LogWarn(LogRenderer,
                    "BufferTransferCoordinator: Falling back to CPU wait before %s queue consumption of %s queue work",
                    GetQueueTypeName(waitingQueue),
                    GetQueueTypeName(producerToken.queueType));

            if (!D3D12RenderSystem::WaitForStandaloneSubmission(producerToken))
            {
                return false;
            }

            commandListManager->UpdateCompletedCommandLists();
            return true;
        };

        if (execution.routeDecision.activeQueue == CommandQueueType::Compute)
        {
            ERROR_RECOVERABLE("BufferTransferCoordinator: compute queue is not supported for buffer transfer batches");
            return {};
        }

        const bool usesDedicatedTransferQueue = execution.routeDecision.activeQueue != CommandQueueType::Graphics;
        if (usesDedicatedTransferQueue && !batch.preTransferTransitions.empty())
        {
            auto* preambleCommandList = commandListManager->AcquireCommandList(
                CommandQueueType::Graphics,
                isUploadBatch ? "BufferUploadPreamble" : "BufferCopyPreamble");
            if (preambleCommandList == nullptr)
            {
                ERROR_RECOVERABLE("BufferTransferCoordinator: failed to acquire graphics preamble command list");
                return {};
            }

            if (!RecordTransitions(preambleCommandList, batch.preTransferTransitions, false))
            {
                return {};
            }

            execution.preambleToken = D3D12RenderSystem::SubmitStandaloneCommandList(preambleCommandList, batch.workload);
            if (!execution.preambleToken.IsValid())
            {
                ERROR_RECOVERABLE("BufferTransferCoordinator: failed to submit graphics preamble");
                return {};
            }

            if (!insertQueueWaitOrBlock(
                    execution.preambleToken,
                    execution.routeDecision.activeQueue))
            {
                return {};
            }
        }

        auto* transferCommandList = commandListManager->AcquireCommandList(
            execution.routeDecision.activeQueue,
            isUploadBatch ? "BufferUploadTransfer" : "BufferCopyTransfer");
        if (transferCommandList == nullptr)
        {
            ERROR_RECOVERABLE(Stringf("BufferTransferCoordinator: failed to acquire %s transfer command list",
                                      GetQueueTypeName(execution.routeDecision.activeQueue)));
            return {};
        }

        if (!usesDedicatedTransferQueue && !batch.preTransferTransitions.empty())
        {
            if (!RecordTransitions(transferCommandList, batch.preTransferTransitions, false))
            {
                return {};
            }
        }

        if (isUploadBatch)
        {
            if (!RecordUploadSpans(transferCommandList, *execution.stagingUploadContext, batch.uploadSpans))
            {
                return {};
            }
        }
        else
        {
            if (!RecordCopySpans(transferCommandList, batch.copySpans))
            {
                return {};
            }
        }

        if (!usesDedicatedTransferQueue && !batch.postTransferTransitions.empty())
        {
            if (!RecordTransitions(transferCommandList, batch.postTransferTransitions, true))
            {
                return {};
            }
        }

        execution.transferToken = D3D12RenderSystem::SubmitStandaloneCommandList(transferCommandList, batch.workload);
        if (!execution.transferToken.IsValid())
        {
            ERROR_RECOVERABLE("BufferTransferCoordinator: failed to submit transfer command list");
            return {};
        }

        if (usesDedicatedTransferQueue && !batch.postTransferTransitions.empty())
        {
            if (!insertQueueWaitOrBlock(
                    execution.transferToken,
                    CommandQueueType::Graphics))
            {
                return {};
            }

            auto* finalizeCommandList = commandListManager->AcquireCommandList(
                CommandQueueType::Graphics,
                isUploadBatch ? "BufferUploadFinalize" : "BufferCopyFinalize");
            if (finalizeCommandList == nullptr)
            {
                ERROR_RECOVERABLE("BufferTransferCoordinator: failed to acquire graphics finalize command list");
                return {};
            }

            if (!RecordTransitions(finalizeCommandList, batch.postTransferTransitions, true))
            {
                return {};
            }

            execution.finalizeToken = D3D12RenderSystem::SubmitStandaloneCommandList(finalizeCommandList, batch.workload);
            if (!execution.finalizeToken.IsValid())
            {
                ERROR_RECOVERABLE("BufferTransferCoordinator: failed to submit graphics finalize command list");
                return {};
            }

            execution.completionToken = execution.finalizeToken;
            return execution;
        }

        execution.completionToken = execution.transferToken;
        return execution;
    }
}
