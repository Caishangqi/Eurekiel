#pragma once

// ============================================================================
// ShaderBundleEvents.hpp - Static MulticastDelegate events for Bundle lifecycle
// ============================================================================

#include "Engine/Core/Event/MulticastDelegate.hpp"

namespace enigma::graphic
{
    class ShaderBundle;

    // Centralized ShaderBundle event definitions
    struct ShaderBundleEvents
    {
        // Fired after a Bundle is loaded - parameter is the newly loaded Bundle
        static enigma::event::MulticastDelegate<ShaderBundle*> OnBundleLoaded;

        // Fired after an unload transaction commits.
        static enigma::event::MulticastDelegate<> OnBundleUnloaded;
    };
} // namespace enigma::graphic
