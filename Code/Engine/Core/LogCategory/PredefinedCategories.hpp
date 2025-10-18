#pragma once

#include "LogCategory.hpp"

namespace enigma::core
{
    /**
     * @brief Predefined engine log categories
     *
     * These categories are used throughout the engine for consistent logging.
     * Custom modules can define additional categories using DEFINE_LOG_CATEGORY.
     */

    // Core engine systems
    DECLARE_LOG_CATEGORY_EXTERN(LogEngine);   ///< General engine messages
    DECLARE_LOG_CATEGORY_EXTERN(LogCore);     ///< Core systems (memory, threading, etc.)
    DECLARE_LOG_CATEGORY_EXTERN(LogSystem);   ///< OS and platform-specific systems

    // Rendering and graphics
    DECLARE_LOG_CATEGORY_EXTERN(LogRenderer); ///< High-level rendering system
    DECLARE_LOG_CATEGORY_EXTERN(LogRender);   ///< Render operations
    DECLARE_LOG_CATEGORY_EXTERN(LogGraphics); ///< Graphics API (D3D12, OpenGL, etc.)

    // Subsystems
    DECLARE_LOG_CATEGORY_EXTERN(LogAudio);    ///< Audio system
    DECLARE_LOG_CATEGORY_EXTERN(LogInput);    ///< Input handling

    // Temporary/debugging
    DECLARE_LOG_CATEGORY_EXTERN(LogTemp);     ///< Temporary debug messages
}
