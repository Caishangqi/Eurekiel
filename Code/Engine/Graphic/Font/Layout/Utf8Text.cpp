#include "Engine/Graphic/Font/Layout/Utf8Text.hpp"

#include <algorithm>
#include <string>

#include "Engine/Graphic/Font/Exception/FontException.hpp"

namespace enigma::graphic
{
    namespace
    {
        bool IsContinuationByte(unsigned char value)
        {
            return (value & 0xC0u) == 0x80u;
        }

        FontConfigurationException BuildUtf8Exception(size_t byteOffset, const char* reason)
        {
            return FontConfigurationException(
                "Invalid UTF-8 text at byte offset " + std::to_string(byteOffset) + ": " + reason);
        }

        unsigned char ReadByte(std::string_view text, size_t index)
        {
            return static_cast<unsigned char>(text[index]);
        }

        void RequireRemainingBytes(std::string_view text, size_t byteOffset, size_t byteCount)
        {
            if (byteOffset + byteCount > text.size())
            {
                throw BuildUtf8Exception(byteOffset, "truncated sequence");
            }
        }

        void RequireContinuation(std::string_view text, size_t byteOffset, size_t index)
        {
            if (!IsContinuationByte(ReadByte(text, index)))
            {
                throw BuildUtf8Exception(byteOffset, "expected continuation byte");
            }
        }
    } // namespace

    std::vector<DecodedCodepoint> DecodeUtf8Text(std::string_view text)
    {
        std::vector<DecodedCodepoint> result;
        result.reserve(text.size());

        for (size_t offset = 0; offset < text.size();)
        {
            const unsigned char first = ReadByte(text, offset);

            if (first <= 0x7Fu)
            {
                result.push_back({static_cast<uint32_t>(first), offset});
                ++offset;
                continue;
            }

            uint32_t codepoint = 0;
            size_t   length    = 0;

            if (first >= 0xC2u && first <= 0xDFu)
            {
                RequireRemainingBytes(text, offset, 2);
                RequireContinuation(text, offset, offset + 1);

                codepoint = ((first & 0x1Fu) << 6u) | (ReadByte(text, offset + 1) & 0x3Fu);
                length    = 2;
            }
            else if (first >= 0xE0u && first <= 0xEFu)
            {
                RequireRemainingBytes(text, offset, 3);
                RequireContinuation(text, offset, offset + 1);
                RequireContinuation(text, offset, offset + 2);

                const unsigned char second = ReadByte(text, offset + 1);
                if (first == 0xE0u && second < 0xA0u)
                {
                    throw BuildUtf8Exception(offset, "overlong three-byte sequence");
                }
                if (first == 0xEDu && second > 0x9Fu)
                {
                    throw BuildUtf8Exception(offset, "surrogate codepoint");
                }

                codepoint = ((first & 0x0Fu) << 12u) |
                            ((second & 0x3Fu) << 6u) |
                            (ReadByte(text, offset + 2) & 0x3Fu);
                length = 3;
            }
            else if (first >= 0xF0u && first <= 0xF4u)
            {
                RequireRemainingBytes(text, offset, 4);
                RequireContinuation(text, offset, offset + 1);
                RequireContinuation(text, offset, offset + 2);
                RequireContinuation(text, offset, offset + 3);

                const unsigned char second = ReadByte(text, offset + 1);
                if (first == 0xF0u && second < 0x90u)
                {
                    throw BuildUtf8Exception(offset, "overlong four-byte sequence");
                }
                if (first == 0xF4u && second > 0x8Fu)
                {
                    throw BuildUtf8Exception(offset, "codepoint above U+10FFFF");
                }

                codepoint = ((first & 0x07u) << 18u) |
                            ((second & 0x3Fu) << 12u) |
                            ((ReadByte(text, offset + 2) & 0x3Fu) << 6u) |
                            (ReadByte(text, offset + 3) & 0x3Fu);
                length = 4;
            }
            else if (IsContinuationByte(first))
            {
                throw BuildUtf8Exception(offset, "unexpected continuation byte");
            }
            else
            {
                throw BuildUtf8Exception(offset, "invalid leading byte");
            }

            result.push_back({codepoint, offset});
            offset += length;
        }

        return result;
    }

    std::vector<uint32_t> DecodeUniqueCodepoints(std::string_view text)
    {
        std::vector<uint32_t> result;
        for (const DecodedCodepoint decoded : DecodeUtf8Text(text))
        {
            if (std::find(result.begin(), result.end(), decoded.codepoint) == result.end())
            {
                result.push_back(decoded.codepoint);
            }
        }

        return result;
    }
} // namespace enigma::graphic
