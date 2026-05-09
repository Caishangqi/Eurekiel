#pragma once

namespace enigma::graphic
{
    // Future owner of glyph atlas CPU/GPU resources. M1 intentionally keeps this
    // as a shell and does not allocate atlas storage or upload textures.
    class GlyphAtlas final
    {
    public:
        GlyphAtlas()  = default;
        ~GlyphAtlas() = default;

        GlyphAtlas(const GlyphAtlas&)                = delete;
        GlyphAtlas& operator=(const GlyphAtlas&)     = delete;
        GlyphAtlas(GlyphAtlas&&) noexcept            = default;
        GlyphAtlas& operator=(GlyphAtlas&&) noexcept = default;
    };
} // namespace enigma::graphic
