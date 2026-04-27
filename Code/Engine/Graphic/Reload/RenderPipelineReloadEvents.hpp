#pragma once

#include "Engine/Core/Event/MulticastDelegate.hpp"
#include "Engine/Graphic/Reload/RenderPipelineReloadTypes.hpp"

namespace enigma::graphic
{
    struct RenderPipelineReloadEvents
    {
        static enigma::event::MulticastDelegate<RenderPipelineReloadTransactionId, RenderPipelineReloadGeneration> OnTransactionStarted;
        static enigma::event::MulticastDelegate<RenderPipelineReloadTransactionId, RenderPipelineReloadGeneration> OnFrontendApplied;
        static enigma::event::MulticastDelegate<RenderPipelineReloadTransactionId, RenderPipelineReloadGeneration> OnTransactionCommitted;
        static enigma::event::MulticastDelegate<RenderPipelineReloadTransactionId, const RenderPipelineReloadFailure&> OnTransactionFailed;
    };
} // namespace enigma::graphic
