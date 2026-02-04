#pragma once

// ============================================================================
// RendererEvents.hpp - Static MulticastDelegate events for Renderer lifecycle
//
// Design Philosophy:
//   - Decouples RendererSubsystem from dependent systems (DIP compliance)
//   - Allows systems to hook into frame lifecycle without direct coupling
//   - Single-threaded execution guaranteed (Broadcast is synchronous)
//
// Usage:
//   // In dependent system (e.g., ShaderBundleSubsystem::Startup)
//   RendererEvents::OnBeginFrame.Add(this, &ShaderBundleSubsystem::OnBeginFrame);
//
//   // In RendererSubsystem::BeginFrame
//   RendererEvents::OnBeginFrame.Broadcast();
// ============================================================================

#include "Engine/Core/Event/MulticastDelegate.hpp"

namespace enigma::graphic
{
    // Centralized Renderer event definitions
    struct RendererEvents
    {
        // ====================================================================
        // Frame Lifecycle Events
        // ====================================================================

        // Fired at the very beginning of BeginFrame, BEFORE any rendering setup
        // Use this for operations that need to happen when GPU is idle:
        // - RT resource changes (format, size)
        // - ShaderBundle switching
        // - Resource cleanup
        //
        // IMPORTANT: This is synchronous - all listeners complete before
        // BeginFrame continues. Safe for RT modifications.
        static enigma::event::MulticastDelegate<> OnBeginFrame;

        // Fired at the end of EndFrame, AFTER Present
        // Use this for:
        // - Statistics collection
        // - Debug output
        // - Frame-end cleanup
        static enigma::event::MulticastDelegate<> OnEndFrame;
    };
} // namespace enigma::graphic
