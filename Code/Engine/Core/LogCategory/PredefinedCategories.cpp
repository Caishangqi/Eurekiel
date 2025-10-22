#include "PredefinedCategories.hpp"

namespace enigma::core
{
    // Define predefined engine log categories with appropriate default levels

    // Core engine components - use INFO level (production environments don't show DEBUG/TRACE)
    DEFINE_LOG_CATEGORY(LogEngine, LogLevel::INFO);
    DEFINE_LOG_CATEGORY(LogCore, LogLevel::INFO);
    DEFINE_LOG_CATEGORY(LogSystem, LogLevel::INFO);

    // Rendering system - use INFO level (high-frequency calls, avoid performance impact)
    DEFINE_LOG_CATEGORY(LogRenderer, LogLevel::INFO);
    DEFINE_LOG_CATEGORY(LogRender, LogLevel::INFO);
    DEFINE_LOG_CATEGORY(LogGraphics, LogLevel::INFO);

    // Resource relates systems
    DEFINE_LOG_CATEGORY(LogResource)

    // Audio and input - use WARNING level (usually only care about errors)
    DEFINE_LOG_CATEGORY(LogAudio, LogLevel::WARNING);
    DEFINE_LOG_CATEGORY(LogInput, LogLevel::WARNING);

    // Game related items
    DEFINE_LOG_CATEGORY(LogGame)
    DEFINE_LOG_CATEGORY(LogPlayer)

    // Temporary debugging - use TRACE level (allow all log levels)
    DEFINE_LOG_CATEGORY(LogTemp); // Defaults to TRACE
}
