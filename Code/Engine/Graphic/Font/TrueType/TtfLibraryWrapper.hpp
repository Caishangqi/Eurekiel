#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

#include "Engine/Graphic/Font/Atlas/GlyphBitmap.hpp"
#include "Engine/Graphic/Font/Atlas/GlyphMetrics.hpp"

namespace enigma::graphic
{
    class TtfLibraryWrapper final
    {
    public:
        TtfLibraryWrapper();
        ~TtfLibraryWrapper();

        TtfLibraryWrapper(const TtfLibraryWrapper&)                = delete;
        TtfLibraryWrapper& operator=(const TtfLibraryWrapper&)     = delete;
        TtfLibraryWrapper(TtfLibraryWrapper&&) noexcept;
        TtfLibraryWrapper& operator=(TtfLibraryWrapper&&) noexcept;

        static TtfLibraryWrapper LoadFromFile(const std::filesystem::path& filePath, int fontIndex = 0);

        bool IsLoaded() const noexcept;
        const std::filesystem::path& GetFilePath() const;

        FontVerticalMetrics GetVerticalMetrics(float pixelHeight) const;
        GlyphLookupResult   FindGlyph(uint32_t codepoint) const;
        GlyphMetrics        GetGlyphMetrics(uint32_t codepoint, float pixelHeight) const;
        GlyphBitmap         RasterizeGlyph(uint32_t codepoint, float pixelHeight) const;

    private:
        class Impl;

        explicit TtfLibraryWrapper(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> m_impl;
    };
} // namespace enigma::graphic
