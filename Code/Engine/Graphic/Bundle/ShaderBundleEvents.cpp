// ============================================================================
// ShaderBundleEvents.cpp - [NEW] Static event instance definitions
// ============================================================================

#include "ShaderBundleEvents.hpp"

namespace enigma::graphic
{
    // [NEW] Static event instances
    enigma::event::MulticastDelegate<ShaderBundle*> ShaderBundleEvents::OnBundleLoaded;
    enigma::event::MulticastDelegate<>              ShaderBundleEvents::OnBundleUnloaded;
} // namespace enigma::graphic
