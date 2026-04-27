#pragma once

namespace enigma::graphic
{
    class RendererSubsystem;
    struct RendererFrontendReloadScope;

    class RendererFrontendReloadService final
    {
    public:
        explicit RendererFrontendReloadService(RendererSubsystem& renderer) noexcept;

        void ApplyReloadScope(const RendererFrontendReloadScope& scope);

    private:
        RendererSubsystem* m_renderer = nullptr;
    };
} // namespace enigma::graphic
