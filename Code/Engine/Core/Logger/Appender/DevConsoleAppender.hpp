#pragma once
#include "ILogAppender.hpp"

// Forward declaration to avoid circular dependencies
class DevConsole;

namespace enigma::core
{
    class DevConsoleAppender : public ILogAppender
    {
    public:
        DevConsoleAppender();

        void Write(const LogMessage& message) override;
        bool IsEnabled() const override;

    private:
        std::string FormatLogMessage(const LogMessage& message) const;
        Rgba8       GetColorForLevel(LogLevel level) const;

        // Will access g_theDevConsole global variable
        DevConsole* GetDevConsole() const;
    };
}
