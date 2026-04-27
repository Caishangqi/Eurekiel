#include "Engine/Graphic/Reload/RenderPipelineReloadEvents.hpp"

namespace enigma::graphic
{
    enigma::event::MulticastDelegate<RenderPipelineReloadTransactionId, RenderPipelineReloadGeneration>
        RenderPipelineReloadEvents::OnTransactionStarted;

    enigma::event::MulticastDelegate<RenderPipelineReloadTransactionId, RenderPipelineReloadGeneration>
        RenderPipelineReloadEvents::OnFrontendApplied;

    enigma::event::MulticastDelegate<RenderPipelineReloadTransactionId, RenderPipelineReloadGeneration>
        RenderPipelineReloadEvents::OnTransactionCommitted;

    enigma::event::MulticastDelegate<RenderPipelineReloadTransactionId, const RenderPipelineReloadFailure&>
        RenderPipelineReloadEvents::OnTransactionFailed;
} // namespace enigma::graphic
