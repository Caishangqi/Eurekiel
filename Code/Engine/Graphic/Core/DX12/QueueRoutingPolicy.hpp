#pragma once

#include "Engine/Graphic/Resource/CommandQueueTypes.hpp"

namespace enigma::graphic
{
    class QueueRoutingPolicy final
    {
    public:
        QueueRouteDecision Resolve(const QueueRouteContext& context, QueueExecutionMode requestedMode) const
        {
            QueueRouteDecision decision = {};
            decision.workload           = context.workload;
            decision.requestedQueue     = GetDefaultQueueForWorkload(context.workload);
            decision.activeQueue        = decision.requestedQueue;

            if (context.workload == QueueWorkloadClass::Unknown)
            {
                decision.requestedQueue  = CommandQueueType::Graphics;
                decision.activeQueue     = CommandQueueType::Graphics;
                decision.fallbackReason  = QueueFallbackReason::UnsupportedWorkload;
                return decision;
            }

            if (requestedMode == QueueExecutionMode::GraphicsOnly)
            {
                if (decision.requestedQueue != CommandQueueType::Graphics)
                {
                    decision.activeQueue    = CommandQueueType::Graphics;
                    decision.fallbackReason = QueueFallbackReason::GraphicsOnlyMode;
                }
                return decision;
            }

            if (context.allowGraphicsFallback &&
                context.forcedFallbackReason != QueueFallbackReason::None)
            {
                decision.activeQueue    = CommandQueueType::Graphics;
                decision.fallbackReason = context.forcedFallbackReason;
                return decision;
            }

            switch (context.workload)
            {
            case QueueWorkloadClass::CopyReadyUpload:
                if (!context.isCopySafe)
                {
                    decision.activeQueue    = CommandQueueType::Graphics;
                    decision.fallbackReason = QueueFallbackReason::ResourceStateNotSupported;
                }
                else if (context.requiresGraphicsStateTransition)
                {
                    decision.activeQueue    = CommandQueueType::Graphics;
                    decision.fallbackReason = QueueFallbackReason::RequiresGraphicsStateTransition;
                }
                break;

            case QueueWorkloadClass::ChunkGeometryUpload:
            case QueueWorkloadClass::ChunkArenaRelocation:
                if (!context.isCopySafe)
                {
                    decision.activeQueue    = CommandQueueType::Graphics;
                    decision.fallbackReason = QueueFallbackReason::ResourceStateNotSupported;
                }
                break;

            case QueueWorkloadClass::MipmapGeneration:
                if (context.requiresGraphicsStateTransition)
                {
                    decision.activeQueue    = CommandQueueType::Graphics;
                    decision.fallbackReason = QueueFallbackReason::RequiresGraphicsStateTransition;
                }
                break;

            default:
                break;
            }

            return decision;
        }

        static CommandQueueType GetDefaultQueueForWorkload(QueueWorkloadClass workload)
        {
            switch (workload)
            {
            case QueueWorkloadClass::CopyReadyUpload:
            case QueueWorkloadClass::ChunkGeometryUpload:
            case QueueWorkloadClass::ChunkArenaRelocation:
                return CommandQueueType::Copy;

            case QueueWorkloadClass::MipmapGeneration:
                return CommandQueueType::Compute;

            case QueueWorkloadClass::Unknown:
            case QueueWorkloadClass::FrameGraphics:
            case QueueWorkloadClass::ImmediateGraphics:
            case QueueWorkloadClass::Present:
            case QueueWorkloadClass::ImGui:
            case QueueWorkloadClass::GraphicsStatefulUpload:
                return CommandQueueType::Graphics;
            }

            return CommandQueueType::Graphics;
        }
    };
}
