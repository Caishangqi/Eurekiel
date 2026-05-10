#include "Engine/Graphic/Font/TrueType/TrueTypeFont.hpp"

#include <utility>

#include "Engine/Graphic/Font/Exception/FontException.hpp"
#include "Engine/Graphic/Font/TrueType/TtfLibraryWrapper.hpp"

namespace enigma::graphic
{
    TrueTypeFont::TrueTypeFont() = default;

    TrueTypeFont::~TrueTypeFont() = default;

    TrueTypeFont::TrueTypeFont(TrueTypeFont&&) noexcept = default;

    TrueTypeFont& TrueTypeFont::operator=(TrueTypeFont&&) noexcept = default;

    TrueTypeFont::TrueTypeFont(std::unique_ptr<TtfLibraryWrapper> wrapper)
        : m_wrapper(std::move(wrapper))
    {
    }

    TrueTypeFont TrueTypeFont::LoadFromFile(const std::filesystem::path& filePath, int fontIndex)
    {
        auto wrapper = std::make_unique<TtfLibraryWrapper>(TtfLibraryWrapper::LoadFromFile(filePath, fontIndex));
        return TrueTypeFont(std::move(wrapper));
    }

    bool TrueTypeFont::IsLoaded() const noexcept
    {
        return m_wrapper && m_wrapper->IsLoaded();
    }

    const std::filesystem::path& TrueTypeFont::GetFilePath() const
    {
        if (!m_wrapper)
        {
            throw FontConfigurationException("Cannot query file path from an unloaded TrueType font");
        }

        return m_wrapper->GetFilePath();
    }

    FontVerticalMetrics TrueTypeFont::GetVerticalMetrics(float pixelHeight) const
    {
        if (!m_wrapper)
        {
            throw FontConfigurationException("Cannot query vertical metrics from an unloaded TrueType font");
        }

        return m_wrapper->GetVerticalMetrics(pixelHeight);
    }

    GlyphLookupResult TrueTypeFont::FindGlyph(uint32_t codepoint) const
    {
        if (!m_wrapper)
        {
            throw FontConfigurationException("Cannot query glyph lookup from an unloaded TrueType font");
        }

        return m_wrapper->FindGlyph(codepoint);
    }

    GlyphMetrics TrueTypeFont::GetGlyphMetrics(uint32_t codepoint, float pixelHeight) const
    {
        if (!m_wrapper)
        {
            throw FontConfigurationException("Cannot query glyph metrics from an unloaded TrueType font");
        }

        return m_wrapper->GetGlyphMetrics(codepoint, pixelHeight);
    }

    GlyphBitmap TrueTypeFont::RasterizeGlyph(uint32_t codepoint, float pixelHeight) const
    {
        if (!m_wrapper)
        {
            throw FontConfigurationException("Cannot rasterize glyph from an unloaded TrueType font");
        }

        return m_wrapper->RasterizeGlyph(codepoint, pixelHeight);
    }
} // namespace enigma::graphic
