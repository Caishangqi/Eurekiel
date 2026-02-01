#pragma once

// ============================================================================
// [NEW] DXGIFormatParser.hpp - DXGI format string to enum parser
// Part of RenderTarget Format Refactor Feature
// ============================================================================

#include <dxgi.h>
#include <optional>
#include <string>

namespace enigma::graphic
{
    /**
     * @class DXGIFormatParser
     * @brief Parses DXGI format name strings to DXGI_FORMAT enum values
     *
     * Supports common RT formats used in deferred rendering:
     * - 8/16/32-bit normalized, float, and integer formats
     * - Depth-stencil formats
     *
     * Usage:
     * @code
     * auto format = DXGIFormatParser::Parse("R16G16B16A16_FLOAT");
     * if (format.has_value()) {
     *     // Use format.value()
     * }
     * @endcode
     */
    class DXGIFormatParser
    {
    public:
        DXGIFormatParser() = delete; // Static-only class

        /**
         * @brief Parse format name string to DXGI_FORMAT
         * @param formatName Format name (e.g., "R16G16B16A16_FLOAT", "R8G8B8A8_UNORM")
         * @return DXGI_FORMAT if valid, std::nullopt if invalid
         *
         * Supported format names (case-insensitive):
         * - R8_UNORM, R8G8_UNORM, R8G8B8A8_UNORM
         * - R8_SNORM, R8G8_SNORM, R8G8B8A8_SNORM
         * - R16_UNORM, R16G16_UNORM, R16G16B16A16_UNORM
         * - R16_FLOAT, R16G16_FLOAT, R16G16B16A16_FLOAT
         * - R32_FLOAT, R32G32_FLOAT, R32G32B32A32_FLOAT
         * - R8_SINT/UINT, R16_SINT/UINT, R32_SINT/UINT (and multi-channel variants)
         * - R10G10B10A2_UNORM, R11G11B10_FLOAT
         * - D16_UNORM, D24_UNORM_S8_UINT, D32_FLOAT, D32_FLOAT_S8X24_UINT
         */
        static std::optional<DXGI_FORMAT> Parse(const std::string& formatName);

        /**
         * @brief Get channel count for a DXGI format
         * @param format DXGI_FORMAT enum value
         * @return Number of channels (1-4), or 0 if unknown
         */
        static int GetChannelCount(DXGI_FORMAT format);

        /**
         * @brief Convert DXGI_FORMAT to string representation
         * @param format DXGI_FORMAT enum value
         * @return Format name string, or "UNKNOWN" if not recognized
         */
        static std::string ToString(DXGI_FORMAT format);

        /**
         * @brief Check if format is a depth-stencil format
         * @param format DXGI_FORMAT enum value
         * @return true if depth or depth-stencil format
         */
        static bool IsDepthFormat(DXGI_FORMAT format);

        /**
         * @brief Check if format is a color format (non-depth)
         * @param format DXGI_FORMAT enum value
         * @return true if color format
         */
        static bool IsColorFormat(DXGI_FORMAT format);
    };
} // namespace enigma::graphic
