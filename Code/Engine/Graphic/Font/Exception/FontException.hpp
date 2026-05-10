#pragma once

#include <exception>
#include <string>

namespace enigma::graphic
{
    // Base exception for Engine Graphic Font module failures.
    //
    // Boundary mapping guidance:
    // - Required engine font missing or unusable: ERROR_AND_DIE at startup/resource boundary.
    // - Optional App/User font missing or unusable: ERROR_RECOVERABLE at caller boundary.
    // - Invalid font module configuration: boundary decides fatal versus recoverable severity.
    class FontException : public std::exception
    {
    public:
        explicit FontException(const std::string& message)
            : m_message(message)
        {
        }

        const char* what() const noexcept override
        {
            return m_message.c_str();
        }

    private:
        std::string m_message;
    };

    class FontResourceException : public FontException
    {
    public:
        using FontException::FontException;
    };

    class FontConfigurationException : public FontException
    {
    public:
        using FontException::FontException;
    };

    class AtlasBuildException : public FontException
    {
    public:
        using FontException::FontException;
    };
} // namespace enigma::graphic
