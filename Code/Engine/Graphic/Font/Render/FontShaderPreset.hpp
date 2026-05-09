#pragma once

namespace enigma::graphic
{
    enum class FontShaderPresetOwner
    {
        Engine,
        User
    };

    // Future shader preset boundary. Font shader assets live under
    // .../assets/<namespace>/shaders/font/ and remain separate from .ttf assets.
    class FontShaderPreset final
    {
    public:
        explicit FontShaderPreset(FontShaderPresetOwner owner = FontShaderPresetOwner::Engine) noexcept
            : m_owner(owner)
        {
        }

        [[nodiscard]] FontShaderPresetOwner GetOwner() const noexcept
        {
            return m_owner;
        }

    private:
        FontShaderPresetOwner m_owner = FontShaderPresetOwner::Engine;
    };
} // namespace enigma::graphic
