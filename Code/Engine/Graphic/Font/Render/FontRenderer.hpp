#pragma once

namespace enigma::graphic
{
    // Future facade for font draw submission. M1 does not own renderer state,
    // upload GPU resources, or issue draw calls.
    class FontRenderer final
    {
    public:
        FontRenderer()  = default;
        ~FontRenderer() = default;

        FontRenderer(const FontRenderer&)                = delete;
        FontRenderer& operator=(const FontRenderer&)     = delete;
        FontRenderer(FontRenderer&&) noexcept            = default;
        FontRenderer& operator=(FontRenderer&&) noexcept = default;
    };
} // namespace enigma::graphic
