#pragma once

namespace enigma::graphic
{
    // Public TrueType font face boundary. Real TTF loading and library ownership
    // are added by later milestones behind this API.
    class TrueTypeFont final
    {
    public:
        TrueTypeFont()  = default;
        ~TrueTypeFont() = default;

        TrueTypeFont(const TrueTypeFont&)                = delete;
        TrueTypeFont& operator=(const TrueTypeFont&)     = delete;
        TrueTypeFont(TrueTypeFont&&) noexcept            = default;
        TrueTypeFont& operator=(TrueTypeFont&&) noexcept = default;
    };
} // namespace enigma::graphic
