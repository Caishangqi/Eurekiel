// ============================================================================
// [NEW] DXGIFormatParser.cpp - DXGI format string to enum parser implementation
// Part of RenderTarget Format Refactor Feature
// ============================================================================

#include "DXGIFormatParser.hpp"
#include <algorithm>
#include <unordered_map>
#include <cctype>

namespace enigma::graphic
{
    namespace
    {
        // Convert string to uppercase for case-insensitive comparison
        std::string ToUpper(const std::string& str)
        {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return result;
        }

        // Format name to DXGI_FORMAT mapping (uppercase keys)
        const std::unordered_map<std::string, DXGI_FORMAT>& GetFormatMap()
        {
            static const std::unordered_map<std::string, DXGI_FORMAT> formatMap = {
                // 8-bit normalized
                {"R8_UNORM", DXGI_FORMAT_R8_UNORM},
                {"R8G8_UNORM", DXGI_FORMAT_R8G8_UNORM},
                {"R8G8B8A8_UNORM", DXGI_FORMAT_R8G8B8A8_UNORM},
                {"B8G8R8A8_UNORM", DXGI_FORMAT_B8G8R8A8_UNORM},

                // 8-bit signed normalized
                {"R8_SNORM", DXGI_FORMAT_R8_SNORM},
                {"R8G8_SNORM", DXGI_FORMAT_R8G8_SNORM},
                {"R8G8B8A8_SNORM", DXGI_FORMAT_R8G8B8A8_SNORM},

                // 16-bit normalized
                {"R16_UNORM", DXGI_FORMAT_R16_UNORM},
                {"R16G16_UNORM", DXGI_FORMAT_R16G16_UNORM},
                {"R16G16B16A16_UNORM", DXGI_FORMAT_R16G16B16A16_UNORM},

                // 16-bit signed normalized
                {"R16_SNORM", DXGI_FORMAT_R16_SNORM},
                {"R16G16_SNORM", DXGI_FORMAT_R16G16_SNORM},
                {"R16G16B16A16_SNORM", DXGI_FORMAT_R16G16B16A16_SNORM},

                // 16-bit float
                {"R16_FLOAT", DXGI_FORMAT_R16_FLOAT},
                {"R16G16_FLOAT", DXGI_FORMAT_R16G16_FLOAT},
                {"R16G16B16A16_FLOAT", DXGI_FORMAT_R16G16B16A16_FLOAT},

                // 32-bit float
                {"R32_FLOAT", DXGI_FORMAT_R32_FLOAT},
                {"R32G32_FLOAT", DXGI_FORMAT_R32G32_FLOAT},
                {"R32G32B32_FLOAT", DXGI_FORMAT_R32G32B32_FLOAT},
                {"R32G32B32A32_FLOAT", DXGI_FORMAT_R32G32B32A32_FLOAT},

                // 8-bit unsigned integer
                {"R8_UINT", DXGI_FORMAT_R8_UINT},
                {"R8G8_UINT", DXGI_FORMAT_R8G8_UINT},
                {"R8G8B8A8_UINT", DXGI_FORMAT_R8G8B8A8_UINT},

                // 8-bit signed integer
                {"R8_SINT", DXGI_FORMAT_R8_SINT},
                {"R8G8_SINT", DXGI_FORMAT_R8G8_SINT},
                {"R8G8B8A8_SINT", DXGI_FORMAT_R8G8B8A8_SINT},

                // 16-bit unsigned integer
                {"R16_UINT", DXGI_FORMAT_R16_UINT},
                {"R16G16_UINT", DXGI_FORMAT_R16G16_UINT},
                {"R16G16B16A16_UINT", DXGI_FORMAT_R16G16B16A16_UINT},

                // 16-bit signed integer
                {"R16_SINT", DXGI_FORMAT_R16_SINT},
                {"R16G16_SINT", DXGI_FORMAT_R16G16_SINT},
                {"R16G16B16A16_SINT", DXGI_FORMAT_R16G16B16A16_SINT},

                // 32-bit unsigned integer
                {"R32_UINT", DXGI_FORMAT_R32_UINT},
                {"R32G32_UINT", DXGI_FORMAT_R32G32_UINT},
                {"R32G32B32_UINT", DXGI_FORMAT_R32G32B32_UINT},
                {"R32G32B32A32_UINT", DXGI_FORMAT_R32G32B32A32_UINT},

                // 32-bit signed integer
                {"R32_SINT", DXGI_FORMAT_R32_SINT},
                {"R32G32_SINT", DXGI_FORMAT_R32G32_SINT},
                {"R32G32B32_SINT", DXGI_FORMAT_R32G32B32_SINT},
                {"R32G32B32A32_SINT", DXGI_FORMAT_R32G32B32A32_SINT},

                // Packed formats
                {"R10G10B10A2_UNORM", DXGI_FORMAT_R10G10B10A2_UNORM},
                {"R10G10B10A2_UINT", DXGI_FORMAT_R10G10B10A2_UINT},
                {"R11G11B10_FLOAT", DXGI_FORMAT_R11G11B10_FLOAT},
                {"R9G9B9E5_SHAREDEXP", DXGI_FORMAT_R9G9B9E5_SHAREDEXP},

                // Depth formats
                {"D16_UNORM", DXGI_FORMAT_D16_UNORM},
                {"D24_UNORM_S8_UINT", DXGI_FORMAT_D24_UNORM_S8_UINT},
                {"D32_FLOAT", DXGI_FORMAT_D32_FLOAT},
                {"D32_FLOAT_S8X24_UINT", DXGI_FORMAT_D32_FLOAT_S8X24_UINT},

                // SRGB variants
                {"R8G8B8A8_UNORM_SRGB", DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},
                {"B8G8R8A8_UNORM_SRGB", DXGI_FORMAT_B8G8R8A8_UNORM_SRGB},
            };
            return formatMap;
        }

        // DXGI_FORMAT to string mapping
        const std::unordered_map<DXGI_FORMAT, std::string>& GetReverseFormatMap()
        {
            static std::unordered_map<DXGI_FORMAT, std::string> reverseMap;
            static bool                                         initialized = false;

            if (!initialized)
            {
                for (const auto& [name, format] : GetFormatMap())
                {
                    reverseMap[format] = name;
                }
                initialized = true;
            }
            return reverseMap;
        }
    } // anonymous namespace

    std::optional<DXGI_FORMAT> DXGIFormatParser::Parse(const std::string& formatName)
    {
        if (formatName.empty())
        {
            return std::nullopt;
        }

        std::string upperName = ToUpper(formatName);

        const auto& formatMap = GetFormatMap();
        auto        it        = formatMap.find(upperName);

        if (it != formatMap.end())
        {
            return it->second;
        }

        return std::nullopt;
    }

    int DXGIFormatParser::GetChannelCount(DXGI_FORMAT format)
    {
        switch (format)
        {
        // 1-channel formats
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_D32_FLOAT:
            return 1;

        // 2-channel formats
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return 2;

        // 3-channel formats
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
            return 3;

        // 4-channel formats
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
            return 4;

        default:
            return 0;
        }
    }

    std::string DXGIFormatParser::ToString(DXGI_FORMAT format)
    {
        const auto& reverseMap = GetReverseFormatMap();
        auto        it         = reverseMap.find(format);

        if (it != reverseMap.end())
        {
            return it->second;
        }

        return "UNKNOWN";
    }

    bool DXGIFormatParser::IsDepthFormat(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return true;
        default:
            return false;
        }
    }

    bool DXGIFormatParser::IsColorFormat(DXGI_FORMAT format)
    {
        return !IsDepthFormat(format) && format != DXGI_FORMAT_UNKNOWN;
    }
} // namespace enigma::graphic
