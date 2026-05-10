#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

#include "Engine/Graphic/Font/Atlas/GlyphBitmap.hpp"
#include "Engine/Graphic/Font/Atlas/GlyphMetrics.hpp"

namespace enigma::graphic
{
    class TtfLibraryWrapper;

    // Public TrueType font face boundary. Third-party TTF details stay behind
    // the implementation and are not required by App-side consumers.
    class TrueTypeFont final
    {
    public:
        TrueTypeFont();
        ~TrueTypeFont();

        TrueTypeFont(const TrueTypeFont&)                = delete;
        TrueTypeFont& operator=(const TrueTypeFont&)     = delete;
        TrueTypeFont(TrueTypeFont&&) noexcept;
        TrueTypeFont& operator=(TrueTypeFont&&) noexcept;

        static TrueTypeFont LoadFromFile(const std::filesystem::path& filePath, int fontIndex = 0);

        bool IsLoaded() const noexcept;
        const std::filesystem::path& GetFilePath() const;

        FontVerticalMetrics GetVerticalMetrics(float pixelHeight) const;
        GlyphLookupResult   FindGlyph(uint32_t codepoint) const;
        GlyphMetrics        GetGlyphMetrics(uint32_t codepoint, float pixelHeight) const;
        GlyphBitmap         RasterizeGlyph(uint32_t codepoint, float pixelHeight) const;

    private:
        explicit TrueTypeFont(std::unique_ptr<TtfLibraryWrapper> wrapper);

        std::unique_ptr<TtfLibraryWrapper> m_wrapper;
    };
} // namespace enigma::graphic
