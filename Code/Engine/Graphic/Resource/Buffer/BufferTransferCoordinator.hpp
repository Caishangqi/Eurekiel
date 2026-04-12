#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <d3d12.h>

#include "Engine/Graphic/Resource/CommandQueueTypes.hpp"
#include "Engine/Graphic/Resource/UploadContext.hpp"

namespace enigma::graphic
{
    class D12Buffer;

    struct BufferTransferTransition final
    {
        D12Buffer*             buffer        = nullptr;
        D3D12_RESOURCE_STATES  transferState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES  finalState    = D3D12_RESOURCE_STATE_COMMON;
        const char*            debugName     = nullptr;

        bool IsValid() const noexcept
        {
            return buffer != nullptr;
        }
    };

    struct BufferUploadSpan final
    {
        D12Buffer* destinationBuffer = nullptr;
        uint64_t   destinationOffset = 0;
        const void* sourceData       = nullptr;
        uint64_t   sizeInBytes       = 0;

        bool IsValid() const noexcept;
    };

    struct BufferCopySpan final
    {
        D12Buffer* sourceBuffer      = nullptr;
        D12Buffer* destinationBuffer = nullptr;
        uint64_t   sourceOffset      = 0;
        uint64_t   destinationOffset = 0;
        uint64_t   sizeInBytes       = 0;

        bool IsValid() const noexcept;
    };

    struct BufferTransferBatch final
    {
        QueueWorkloadClass workload       = QueueWorkloadClass::Unknown;
        CommandQueueType   preferredQueue = CommandQueueType::Copy;
        std::vector<BufferTransferTransition> preTransferTransitions;
        std::vector<BufferUploadSpan>         uploadSpans;
        std::vector<BufferCopySpan>           copySpans;
        std::vector<BufferTransferTransition> postTransferTransitions;
        bool                                  allowGraphicsFallback = true;

        uint64_t GetTotalUploadBytes() const noexcept;
        bool     HasUploadWork() const noexcept;
        bool     HasCopyWork() const noexcept;
        bool     HasTransitions() const noexcept;
        bool     IsValidForUpload() const noexcept;
        bool     IsValidForCopy() const noexcept;
    };

    struct BufferTransferExecution final
    {
        QueueRouteDecision            routeDecision      = {};
        QueueSubmissionToken          preambleToken      = {};
        QueueSubmissionToken          transferToken      = {};
        QueueSubmissionToken          finalizeToken      = {};
        QueueSubmissionToken          completionToken    = {};
        std::shared_ptr<UploadContext> stagingUploadContext;
        bool                          usedCpuWaitForHandoff = false;

        bool IsValid() const noexcept
        {
            return completionToken.IsValid() || transferToken.IsValid();
        }
    };

    class BufferTransferCoordinator final
    {
    public:
        BufferTransferExecution ExecuteUploadBatch(const BufferTransferBatch& batch) const;
        BufferTransferExecution ExecuteCopyBatch(const BufferTransferBatch& batch) const;
        bool                    WaitForTransferCompletion(BufferTransferExecution& execution) const;

    private:
        enum class TransferMode
        {
            Upload,
            Copy
        };

        BufferTransferExecution ExecuteBatch(const BufferTransferBatch& batch, TransferMode mode) const;
    };
}
