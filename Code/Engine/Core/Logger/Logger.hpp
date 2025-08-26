#pragma once

// Include all logger components
#include "LoggerSubsystem.hpp"
#include "LogLevel.hpp"
#include "LogMessage.hpp"
#include "Appender/ILogAppender.hpp"
#include "Appender/ConsoleAppender.hpp"
#include "Appender/DevConsoleAppender.hpp"
#include "Appender/FileAppender.hpp"

// Include the new elegant API
#include "LoggerAPI.hpp"

// Include Engine for convenient access
#include "../Engine.hpp"

// Note: GetGlobalLogger is now defined in LoggerAPI.hpp

// Convenient global macros for logging
#define GLogger enigma::core::GetGlobalLogger()

// Main logging macros - check if logger is available before calling
#define LOG_TRACE(category, message) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogTrace(category, message); \
    } while(0)

#define LOG_DEBUG(category, message) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogDebug(category, message); \
    } while(0)

#define LOG_INFO(category, message) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogInfo(category, message); \
    } while(0)

#define LOG_WARN(category, message) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogWarn(category, message); \
    } while(0)

#define LOG_ERROR(category, message) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogError(category, message); \
    } while(0)

#define LOG_FATAL(category, message) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogFatal(category, message); \
    } while(0)

// Formatted logging macros
#define LOG_TRACE_F(category, format, ...) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogFormatted(enigma::core::LogLevel::TRACE, category, format, __VA_ARGS__); \
    } while(0)

#define LOG_DEBUG_F(category, format, ...) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogFormatted(enigma::core::LogLevel::DEBUG, category, format, __VA_ARGS__); \
    } while(0)

#define LOG_INFO_F(category, format, ...) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogFormatted(enigma::core::LogLevel::INFO, category, format, __VA_ARGS__); \
    } while(0)

#define LOG_WARN_F(category, format, ...) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogFormatted(enigma::core::LogLevel::WARNING, category, format, __VA_ARGS__); \
    } while(0)

#define LOG_ERROR_F(category, format, ...) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogFormatted(enigma::core::LogLevel::ERROR_, category, format, __VA_ARGS__); \
    } while(0)

#define LOG_FATAL_F(category, format, ...) \
    do { \
        auto* logger = GLogger; \
        if (logger) logger->LogFormatted(enigma::core::LogLevel::FATAL, category, format, __VA_ARGS__); \
    } while(0)

// Convenience macros for common categories
#define LOG_ENGINE_INFO(message) LOG_INFO("Engine", message)
#define LOG_ENGINE_WARN(message) LOG_WARN("Engine", message)
#define LOG_ENGINE_ERROR(message) LOG_ERROR("Engine", message)

#define LOG_RENDERER_INFO(message) LOG_INFO("Renderer", message)
#define LOG_RENDERER_WARN(message) LOG_WARN("Renderer", message)
#define LOG_RENDERER_ERROR(message) LOG_ERROR("Renderer", message)

#define LOG_AUDIO_INFO(message) LOG_INFO("Audio", message)
#define LOG_AUDIO_WARN(message) LOG_WARN("Audio", message)
#define LOG_AUDIO_ERROR(message) LOG_ERROR("Audio", message)

#define LOG_GAME_INFO(message) LOG_INFO("Game", message)
#define LOG_GAME_WARN(message) LOG_WARN("Game", message)
#define LOG_GAME_ERROR(message) LOG_ERROR("Game", message)