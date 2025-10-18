#pragma once
#include "LoggerSubsystem.hpp"
#include "../Engine.hpp"
#include "../StringUtils.hpp"
#include "../LogCategory/LogCategory.hpp"

namespace enigma::core
{
    // Forward declarations for convenience
    inline LoggerSubsystem* GetGlobalLogger()
    {
        return GEngine ? GEngine->GetLogger() : nullptr;
    }

    // Non-formatted logging functions - simple string versions
    inline void LogTrace(const char* category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogTrace(category, message);
    }

    inline void LogDebug(const char* category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogDebug(category, message);
    }

    inline void LogInfo(const char* category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogInfo(category, message);
    }

    inline void LogWarn(const char* category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogWarn(category, message);
    }

    inline void LogError(const char* category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogError(category, message);
    }

    inline void LogFatal(const char* category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger) logger->LogFatal(category, message);
    }

    // Non-formatted logging functions - LogCategoryBase overloads with default level filtering
    inline void LogTrace(const LogCategoryBase& category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::TRACE, category))
            logger->LogTrace(category.GetName(), message);
    }

    inline void LogDebug(const LogCategoryBase& category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::DEBUG, category))
            logger->LogDebug(category.GetName(), message);
    }

    inline void LogInfo(const LogCategoryBase& category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::INFO, category))
            logger->LogInfo(category.GetName(), message);
    }

    inline void LogWarn(const LogCategoryBase& category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::WARNING, category))
            logger->LogWarn(category.GetName(), message);
    }

    inline void LogError(const LogCategoryBase& category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::ERROR_, category))
            logger->LogError(category.GetName(), message);
    }

    inline void LogFatal(const LogCategoryBase& category, const char* message)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::FATAL, category))
            logger->LogFatal(category.GetName(), message);
    }

    // Formatted logging functions - using Stringf for formatting
    template <typename... Args>
    inline void LogTrace(const char* category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogTrace(category, formatted.c_str());
        }
    }

    template <typename... Args>
    inline void LogDebug(const char* category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogDebug(category, formatted.c_str());
        }
    }

    template <typename... Args>
    inline void LogInfo(const char* category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogInfo(category, formatted.c_str());
        }
    }

    template <typename... Args>
    inline void LogWarn(const char* category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogWarn(category, formatted.c_str());
        }
    }

    template <typename... Args>
    inline void LogError(const char* category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogError(category, formatted.c_str());
        }
    }

    template <typename... Args>
    inline void LogFatal(const char* category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger)
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogFatal(category, formatted.c_str());
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

    // Formatted logging functions - LogCategoryBase overloads with default level filtering
    template <typename... Args>
    inline void LogTrace(const LogCategoryBase& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::TRACE, category))
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogTrace(category.GetName(), formatted.c_str());
        }
    }

    template <typename... Args>
    inline void LogDebug(const LogCategoryBase& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::DEBUG, category))
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogDebug(category.GetName(), formatted.c_str());
        }
    }

    template <typename... Args>
    inline void LogInfo(const LogCategoryBase& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::INFO, category))
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogInfo(category.GetName(), formatted.c_str());
        }
    }

    template <typename... Args>
    inline void LogWarn(const LogCategoryBase& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::WARNING, category))
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogWarn(category.GetName(), formatted.c_str());
        }
    }

    template <typename... Args>
    inline void LogError(const LogCategoryBase& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::ERROR_, category))
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogError(category.GetName(), formatted.c_str());
        }
    }

    template <typename... Args>
    inline void LogFatal(const LogCategoryBase& category, const char* format, Args&&... args)
    {
        auto* logger = GetGlobalLogger();
        if (logger && logger->ShouldLogMessage(LogLevel::FATAL, category))
        {
            std::string formatted = Stringf(format, std::forward<Args>(args)...);
            logger->LogFatal(category.GetName(), formatted.c_str());
        }
    }
}