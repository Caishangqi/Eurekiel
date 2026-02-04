// ============================================================================
// RendererEvents.cpp - Static member definitions for RendererEvents
// ============================================================================

#include "RendererEvents.hpp"

namespace enigma::graphic
{
    // Static member definitions
    enigma::event::MulticastDelegate<> RendererEvents::OnBeginFrame;
    enigma::event::MulticastDelegate<> RendererEvents::OnEndFrame;
} // namespace enigma::graphic
