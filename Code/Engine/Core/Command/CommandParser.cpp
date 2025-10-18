#include "CommandParser.hpp"
#include <cctype>
#include <sstream>

namespace enigma::core
{
    // ============================================================================
    // Public interface
    // ============================================================================

    std::optional<CommandArgs> CommandParser::Parse(const std::string& commandLine)
    {
        ClearError();

        // Handle empty input
        if (commandLine.empty())
        {
            SetError("Empty command line");
            return std::nullopt;
        }

        // Tokenize the input
        auto tokens = Tokenize(commandLine);

        if (tokens.empty())
        {
            SetError("No tokens found");
            return std::nullopt;
        }

        // First token should be the command name
        if (tokens[0].type != TokenType::Text)
        {
            SetError("Command name must be a text token");
            return std::nullopt;
        }

        CommandArgs args;
        args.commandName = tokens[0].value;

        // Process remaining tokens
        for (size_t i = 1; i < tokens.size(); ++i)
        {
            const auto& token = tokens[i];

            if (token.type == TokenType::NamedArg)
            {
                // Named argument
                CommandValue value = TryConvertValue(token.value);
                args.namedArgs[token.key] = value;
            }
            else if (token.type == TokenType::Text)
            {
                // Positional argument
                CommandValue value = TryConvertValue(token.value);
                args.positionalArgs.push_back(value);
            }
        }

        return args;
    }

    // ============================================================================
    // Tokenization
    // ============================================================================

    std::vector<CommandParser::Token> CommandParser::Tokenize(const std::string& input)
    {
        std::vector<Token> tokens;
        size_t pos = 0;

        while (pos < input.length())
        {
            SkipWhitespace(input, pos);

            if (pos >= input.length())
                break;

            // Check for named argument (starts with --)
            if (pos + 1 < input.length() && input[pos] == '-' && input[pos + 1] == '-')
            {
                tokens.push_back(ParseNamedArgument(input, pos));
            }
            // Check for quoted string
            else if (input[pos] == '"' || input[pos] == '\'')
            {
                Token token;
                token.type = TokenType::Text;
                token.value = ParseQuotedString(input, pos);
                tokens.push_back(token);
            }
            // Regular text token
            else
            {
                Token token;
                token.type = TokenType::Text;

                size_t start = pos;
                while (pos < input.length() && !IsWhitespace(input[pos]))
                    pos++;

                token.value = input.substr(start, pos - start);
                tokens.push_back(token);
            }
        }

        return tokens;
    }

    std::string CommandParser::ParseQuotedString(const std::string& input, size_t& pos)
    {
        char quoteChar = input[pos];
        pos++; // Skip opening quote

        std::string result;
        bool escaped = false;

        while (pos < input.length())
        {
            char c = input[pos];

            if (escaped)
            {
                // Handle escape sequences
                switch (c)
                {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"':  result += '"';  break;
                case '\'': result += '\''; break;
                default:   result += c;    break;
                }
                escaped = false;
            }
            else
            {
                if (c == '\\')
                {
                    escaped = true;
                }
                else if (c == quoteChar)
                {
                    pos++; // Skip closing quote
                    break;
                }
                else
                {
                    result += c;
                }
            }

            pos++;
        }

        return result;
    }

    CommandParser::Token CommandParser::ParseNamedArgument(const std::string& input, size_t& pos)
    {
        Token token;
        token.type = TokenType::NamedArg;

        pos += 2; // Skip "--"

        // Find the key (until '=' or whitespace)
        size_t keyStart = pos;
        while (pos < input.length() && input[pos] != '=' && !IsWhitespace(input[pos]))
            pos++;

        token.key = input.substr(keyStart, pos - keyStart);

        // Check if there's a value after '='
        if (pos < input.length() && input[pos] == '=')
        {
            pos++; // Skip '='

            // Parse the value (could be quoted or plain)
            if (pos < input.length() && (input[pos] == '"' || input[pos] == '\''))
            {
                token.value = ParseQuotedString(input, pos);
            }
            else
            {
                size_t valueStart = pos;
                while (pos < input.length() && !IsWhitespace(input[pos]))
                    pos++;
                token.value = input.substr(valueStart, pos - valueStart);
            }
        }
        else
        {
            // Flag without value (treat as true)
            token.value = "true";
        }

        return token;
    }

    // ============================================================================
    // Utility functions
    // ============================================================================

    void CommandParser::SkipWhitespace(const std::string& input, size_t& pos)
    {
        while (pos < input.length() && IsWhitespace(input[pos]))
            pos++;
    }

    bool CommandParser::IsWhitespace(char c) const
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    CommandValue CommandParser::TryConvertValue(const std::string& str)
    {
        if (str.empty())
            return str;

        // Try boolean
        if (str == "true" || str == "false")
            return str == "true";

        // Try integer
        try
        {
            size_t pos;
            int intValue = std::stoi(str, &pos);
            if (pos == str.length()) // Entire string was consumed
                return intValue;
        }
        catch (...)
        {
            // Not an integer, continue
        }

        // Try float
        try
        {
            size_t pos;
            float floatValue = std::stof(str, &pos);
            if (pos == str.length()) // Entire string was consumed
                return floatValue;
        }
        catch (...)
        {
            // Not a float, continue
        }

        // Default to string
        return str;
    }
}
