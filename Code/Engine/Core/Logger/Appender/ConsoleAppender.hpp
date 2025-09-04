#pragma once
#include "ILogAppender.hpp"
#include <mutex>

namespace enigma::core
{
    class ConsoleAppender : public ILogAppender
    {
    public:
        ConsoleAppender(bool enableColors = true);

        void Write(const LogMessage& message) override;
        void Flush() override;

        void SetColorMode(bool enabled) { m_enableColors = enabled; }
        bool GetColorMode() const { return m_enableColors; }

    private:
        std::string FormatLogMessage(const LogMessage& message) const;
        std::string GetAnsiColorCode(LogLevel level) const;
        std::string GetAnsiResetCode() const;
        bool        SupportsAnsiColors() const;

        bool               m_enableColors;
        mutable std::mutex m_writeMutex; // Protect cout output
    };
}
