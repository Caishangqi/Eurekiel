#pragma once

namespace enigma::graphic
{
    // CPU text layout boundary. Later milestones add glyph metrics input and
    // quad generation here without depending on renderer state.
    class TextLayout final
    {
    public:
        TextLayout()  = default;
        ~TextLayout() = default;

        TextLayout(const TextLayout&)                = delete;
        TextLayout& operator=(const TextLayout&)     = delete;
        TextLayout(TextLayout&&) noexcept            = default;
        TextLayout& operator=(TextLayout&&) noexcept = default;
    };
} // namespace enigma::graphic
