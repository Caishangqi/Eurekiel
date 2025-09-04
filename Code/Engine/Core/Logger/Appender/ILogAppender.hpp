#pragma once
#include "../LogMessage.hpp"

namespace enigma::core
{
    class ILogAppender
    {
    public:
        virtual      ~ILogAppender() = default;
        virtual void Write(const LogMessage& message) = 0;

        virtual void Flush()
        {
        } // Optional: Force flush buffers
        virtual bool IsEnabled() const { return m_enabled; }
        virtual void SetEnabled(bool enabled) { m_enabled = enabled; }

    protected:
        bool m_enabled = true;
    };
}
