#pragma once
#include "CommandTypes.hpp"
#include <string>
#include <optional>

namespace enigma::core
{
    // ============================================================================
    // Command line parser
    // ============================================================================
    class CommandParser
    {
    public:
        CommandParser() = default;

        // Parse a command line string into CommandArgs
        // Returns nullopt if parsing fails
        std::optional<CommandArgs> Parse(const std::string& commandLine);

        // Get last parse error message
        const std::string& GetLastError() const { return m_lastError; }

    private:
        // Token types for lexical analysis
        enum class TokenType
        {
            Text,           // Plain text or quoted string
            NamedArg,       // --key=value or --flag
            EndOfInput
        };

        struct Token
        {
            TokenType   type;
            std::string value;
            std::string key;   // For named arguments
        };

        // Tokenization
        std::vector<Token> Tokenize(const std::string& input);

        // Parse a quoted string
        std::string ParseQuotedString(const std::string& input, size_t& pos);

        // Parse a named argument (--key=value or --flag)
        Token ParseNamedArgument(const std::string& input, size_t& pos);

        // Skip whitespace
        void SkipWhitespace(const std::string& input, size_t& pos);

        // Check if character is whitespace
        bool IsWhitespace(char c) const;

        // Try to convert string to specific types
        CommandValue TryConvertValue(const std::string& str);

        // Error handling
        std::string m_lastError;

        void SetError(const std::string& error)
        {
            m_lastError = error;
        }

        void ClearError()
        {
            m_lastError.clear();
        }
    };
}
