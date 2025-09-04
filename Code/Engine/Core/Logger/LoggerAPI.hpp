#pragma once
#include "LoggerSubsystem.hpp"
#include "../Engine.hpp"
#include "../StringUtils.hpp"

namespace enigma::core
{
    // Forward declarations for convenience
    inline LoggerSubsystem* GetGlobalLogger()
    {
        return GEngine ? GEngine->GetLogger() : nullptr;
    }

    // Non-formatted logging functions - simple string versions
    inline void LogTrace(const std::string& category, const std::string& message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogTrace(category, message);
    }

    inline void LogDebug(const std::string& category, const std::string& message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogDebug(category, message);
    }

    inline void LogInfo(const std::string& category, const std::string& message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogInfo(category, message);
    }

    inline void LogWarn(const std::string& category, const std::string& message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogWarn(category, message);
    }

    inline void LogError(const std::string& category, const std::string& message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogError(category, message);
    }

    inline void LogFatal(const std::string& category, const std::string& message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogFatal(category, message);
    }

    // Formatted logging functions - using Stringf for formatting
    template <typename... Args>
    inline void LogTrace(const std::string& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogTrace(category, formatted);
        }
    }

    template <typename... Args>
    inline void LogDebug(const std::string& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogDebug(category, formatted);
        }
    }

    template <typename... Args>
    inline void LogInfo(const std::string& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogInfo(category, formatted);
        }
    }

    template <typename... Args>
    inline void LogWarn(const std::string& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogWarn(category, formatted);
        }
    }

    template <typename... Args>
    inline void LogError(const std::string& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogError(category, formatted);
        }
    }

    template <typename... Args>
    inline void LogFatal(const std::string& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogFatal(category, formatted);
        }
    }

    // Category-specific convenience functions (using the overloaded functions above)
    template <typename... Args>
    inline void LogEngineInfo(Args&&... args)
    {
        LogInfo("Engine", std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogEngineWarn(Args&&... args)
    {
        LogWarn("Engine", std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogEngineError(Args&&... args)
    {
        LogError("Engine", std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogRendererInfo(Args&&... args)
    {
        LogInfo("Renderer", std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogRendererWarn(Args&&... args)
    {
        LogWarn("Renderer", std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogRendererError(Args&&... args)
    {
        LogError("Renderer", std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogAudioInfo(Args&&... args)
    {
        LogInfo("Audio", std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogAudioWarn(Args&&... args)
    {
        LogWarn("Audio", std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogAudioError(Args&&... args)
    {
        LogError("Audio", std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogGameInfo(Args&&... args)
    {
        LogInfo("Game", std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogGameWarn(Args&&... args)
    {
        LogWarn("Game", std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogGameError(Args&&... args)
    {
        LogError("Game", std::forward<Args>(args)...);
    }
}
