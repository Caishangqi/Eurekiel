#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace enigma::graphic
{
    struct DecodedCodepoint
    {
        uint32_t codepoint  = 0;
        size_t   byteOffset = 0;
    };

    std::vector<DecodedCodepoint> DecodeUtf8Text(std::string_view text);
    std::vector<uint32_t>         DecodeUniqueCodepoints(std::string_view text);
} // namespace enigma::graphic
