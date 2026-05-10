#include "Engine/Graphic/Font/TrueType/TtfLibraryWrapper.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>

#include "Engine/Core/Buffer/BufferExceptions.hpp"
#include "Engine/Core/FileSystemHelper.hpp"
#include "Engine/Graphic/Font/Exception/FontException.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4505)
#endif
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "ThirdParty/stb/stb_truetype.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace enigma::graphic
{
    namespace
    {
        constexpr uint32_t kMaximumUnicodeCodepoint = 0x10FFFFu;
        constexpr size_t   kMinimumFontHeaderBytes  = 4u;

        std::string BuildFontMessage(const std::filesystem::path& filePath, const char* reason)
        {
            return "Font file [" + filePath.string() + "]: " + reason;
        }

        std::string BuildGlyphMessage(
            const std::filesystem::path& filePath,
            uint32_t                     codepoint,
            uint32_t                     glyphIndex,
            float                        pixelHeight,
            const char*                  reason)
        {
            return "Font file [" + filePath.string() + "], codepoint " + std::to_string(codepoint)
                 + ", glyph " + std::to_string(glyphIndex)
                 + ", pixel height " + std::to_string(pixelHeight)
                 + ": " + reason;
        }

        void ValidatePixelHeight(float pixelHeight)
        {
            if (!std::isfinite(pixelHeight) || pixelHeight <= 0.0f)
            {
                throw FontConfigurationException(
                    "Font pixel height must be finite and greater than zero: " + std::to_string(pixelHeight));
            }
        }
    } // namespace

    class TtfLibraryWrapper::Impl final
    {
    public:
        static std::unique_ptr<Impl> LoadFromFile(const std::filesystem::path& filePath, int fontIndex)
        {
            if (fontIndex < 0)
            {
                throw FontConfigurationException("Font index must be non-negative: " + std::to_string(fontIndex));
            }

            enigma::core::ByteArray fontBytes;
            try
            {
                fontBytes = FileSystemHelper::ReadFileToBuffer(filePath);
            }
            catch (const enigma::core::FileIOException& exception)
            {
                throw FontResourceException(exception.what());
            }

            if (fontBytes.empty())
            {
                throw FontResourceException(BuildFontMessage(filePath, "file is empty"));
            }

            if (fontBytes.size() < kMinimumFontHeaderBytes)
            {
                throw FontResourceException(BuildFontMessage(filePath, "file is too small to contain a font header"));
            }

            int fontOffset = stbtt_GetFontOffsetForIndex(fontBytes.data(), fontIndex);
            if (fontOffset < 0)
            {
                throw FontResourceException(BuildFontMessage(filePath, "invalid or unsupported font index"));
            }

            stbtt_fontinfo fontInfo = {};
            if (stbtt_InitFont(&fontInfo, fontBytes.data(), fontOffset) == 0)
            {
                throw FontResourceException(BuildFontMessage(filePath, "stbtt_InitFont failed"));
            }

            return std::unique_ptr<Impl>(new Impl(filePath, std::move(fontBytes), fontInfo));
        }

        explicit Impl(std::filesystem::path filePath, enigma::core::ByteArray fontBytes, const stbtt_fontinfo& fontInfo)
            : m_filePath(std::move(filePath))
            , m_fontBytes(std::move(fontBytes))
            , m_fontInfo(fontInfo)
        {
        }

        const std::filesystem::path& GetFilePath() const
        {
            return m_filePath;
        }

        FontVerticalMetrics GetVerticalMetrics(float pixelHeight) const
        {
            ValidatePixelHeight(pixelHeight);

            int ascent  = 0;
            int descent = 0;
            int lineGap = 0;
            stbtt_GetFontVMetrics(&m_fontInfo, &ascent, &descent, &lineGap);

            float scale = stbtt_ScaleForPixelHeight(&m_fontInfo, pixelHeight);

            FontVerticalMetrics metrics;
            metrics.pixelHeight = pixelHeight;
            metrics.ascent      = static_cast<float>(ascent) * scale;
            metrics.descent     = static_cast<float>(descent) * scale;
            metrics.lineGap     = static_cast<float>(lineGap) * scale;
            metrics.lineHeight  = metrics.ascent - metrics.descent + metrics.lineGap;
            return metrics;
        }

        GlyphLookupResult FindGlyph(uint32_t codepoint) const
        {
            GlyphLookupResult result;
            result.codepoint = codepoint;

            if (codepoint > kMaximumUnicodeCodepoint || codepoint > static_cast<uint32_t>(std::numeric_limits<int>::max()))
            {
                result.glyphIndex      = 0;
                result.found           = false;
                result.isFallbackGlyph = true;
                return result;
            }

            int glyphIndex = stbtt_FindGlyphIndex(&m_fontInfo, static_cast<int>(codepoint));

            result.glyphIndex      = static_cast<uint32_t>(glyphIndex);
            result.found           = glyphIndex != 0;
            result.isFallbackGlyph = glyphIndex == 0;
            return result;
        }

        GlyphMetrics GetGlyphMetrics(uint32_t codepoint, float pixelHeight) const
        {
            ValidatePixelHeight(pixelHeight);

            GlyphLookupResult lookup = FindGlyph(codepoint);
            int               glyph  = static_cast<int>(lookup.glyphIndex);
            float             scale  = stbtt_ScaleForPixelHeight(&m_fontInfo, pixelHeight);

            int advanceWidth    = 0;
            int leftSideBearing = 0;
            stbtt_GetGlyphHMetrics(&m_fontInfo, glyph, &advanceWidth, &leftSideBearing);

            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            int hasBox = stbtt_GetGlyphBox(&m_fontInfo, glyph, &x0, &y0, &x1, &y1);

            GlyphMetrics metrics;
            metrics.codepoint  = codepoint;
            metrics.glyphIndex = lookup.glyphIndex;
            metrics.found      = lookup.found;
            metrics.advanceX   = static_cast<float>(advanceWidth) * scale;
            metrics.bearingX   = static_cast<float>(leftSideBearing) * scale;
            metrics.bearingY   = hasBox != 0 ? static_cast<float>(y1) * scale : 0.0f;
            metrics.width      = hasBox != 0 ? static_cast<float>(x1 - x0) * scale : 0.0f;
            metrics.height     = hasBox != 0 ? static_cast<float>(y1 - y0) * scale : 0.0f;
            return metrics;
        }

        GlyphBitmap RasterizeGlyph(uint32_t codepoint, float pixelHeight) const
        {
            ValidatePixelHeight(pixelHeight);

            GlyphLookupResult lookup = FindGlyph(codepoint);
            int               glyph  = static_cast<int>(lookup.glyphIndex);
            float             scale  = stbtt_ScaleForPixelHeight(&m_fontInfo, pixelHeight);

            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            stbtt_GetGlyphBitmapBox(&m_fontInfo, glyph, scale, scale, &x0, &y0, &x1, &y1);

            int width  = x1 - x0;
            int height = y1 - y0;

            if (width < 0 || height < 0)
            {
                throw FontResourceException(
                    BuildGlyphMessage(m_filePath, codepoint, lookup.glyphIndex, pixelHeight, "invalid bitmap dimensions"));
            }

            GlyphBitmap bitmap;
            bitmap.codepoint   = codepoint;
            bitmap.glyphIndex  = lookup.glyphIndex;
            bitmap.pixelHeight = pixelHeight;
            bitmap.width       = width;
            bitmap.height      = height;
            bitmap.offsetX     = x0;
            bitmap.offsetY     = y0;
            bitmap.stride      = width;

            if (width == 0 || height == 0)
            {
                return bitmap;
            }

            bitmap.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
            stbtt_MakeGlyphBitmap(
                &m_fontInfo,
                bitmap.pixels.data(),
                width,
                height,
                bitmap.stride,
                scale,
                scale,
                glyph);

            return bitmap;
        }

    private:
        std::filesystem::path     m_filePath;
        enigma::core::ByteArray   m_fontBytes;
        stbtt_fontinfo            m_fontInfo = {};
    };

    TtfLibraryWrapper::TtfLibraryWrapper() = default;

    TtfLibraryWrapper::~TtfLibraryWrapper() = default;

    TtfLibraryWrapper::TtfLibraryWrapper(TtfLibraryWrapper&&) noexcept = default;

    TtfLibraryWrapper& TtfLibraryWrapper::operator=(TtfLibraryWrapper&&) noexcept = default;

    TtfLibraryWrapper::TtfLibraryWrapper(std::unique_ptr<Impl> impl)
        : m_impl(std::move(impl))
    {
    }

    TtfLibraryWrapper TtfLibraryWrapper::LoadFromFile(const std::filesystem::path& filePath, int fontIndex)
    {
        return TtfLibraryWrapper(Impl::LoadFromFile(filePath, fontIndex));
    }

    bool TtfLibraryWrapper::IsLoaded() const noexcept
    {
        return m_impl != nullptr;
    }

    const std::filesystem::path& TtfLibraryWrapper::GetFilePath() const
    {
        if (!m_impl)
        {
            throw FontConfigurationException("Cannot query file path from an unloaded TTF wrapper");
        }

        return m_impl->GetFilePath();
    }

    FontVerticalMetrics TtfLibraryWrapper::GetVerticalMetrics(float pixelHeight) const
    {
        if (!m_impl)
        {
            throw FontConfigurationException("Cannot query vertical metrics from an unloaded TTF wrapper");
        }

        return m_impl->GetVerticalMetrics(pixelHeight);
    }

    GlyphLookupResult TtfLibraryWrapper::FindGlyph(uint32_t codepoint) const
    {
        if (!m_impl)
        {
            throw FontConfigurationException("Cannot query glyph lookup from an unloaded TTF wrapper");
        }

        return m_impl->FindGlyph(codepoint);
    }

    GlyphMetrics TtfLibraryWrapper::GetGlyphMetrics(uint32_t codepoint, float pixelHeight) const
    {
        if (!m_impl)
        {
            throw FontConfigurationException("Cannot query glyph metrics from an unloaded TTF wrapper");
        }

        return m_impl->GetGlyphMetrics(codepoint, pixelHeight);
    }

    GlyphBitmap TtfLibraryWrapper::RasterizeGlyph(uint32_t codepoint, float pixelHeight) const
    {
        if (!m_impl)
        {
            throw FontConfigurationException("Cannot rasterize glyph from an unloaded TTF wrapper");
        }

        return m_impl->RasterizeGlyph(codepoint, pixelHeight);
    }
} // namespace enigma::graphic
