#pragma once

// ============================================================================
// RendererEvents.hpp - Static renderer lifecycle notifications
//
// Design notes:
//   - Decouples RendererSubsystem from dependent systems
//   - Broadcast is synchronous and notification-only unless stated otherwise
//   - Ownership-sensitive listeners must bind to the explicit post-retirement hook
// ============================================================================

#include "Engine/Core/Event/MulticastDelegate.hpp"

namespace enigma::graphic
{
    // Centralized renderer lifecycle notifications.
    struct RendererEvents
    {
        // Fired once at the end of RendererSubsystem::Startup().
        // Core renderer services are ready for CPU-side registration work.
        static enigma::event::MulticastDelegate<> OnPipelineReady;

        // Fired synchronously near the start of RendererSubsystem::BeginFrame().
        // This is the pre-retirement CPU notification for work that does not
        // require frame-slot ownership or frame-local resource mutation.
        static enigma::event::MulticastDelegate<> OnBeginFrame;

        // Fired after the active frame slot has been retired and is safe to
        // reuse for frame-local reset or upload work.
        static enigma::event::MulticastDelegate<> OnFrameSlotAcquired;

        // Fired at the end of EndFrame after present submission work.
        // This is also notification-only and does not imply queue completion.
        static enigma::event::MulticastDelegate<> OnEndFrame;
    };
} // namespace enigma::graphic
