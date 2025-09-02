#pragma once
#include "Engine/Core/SubsystemManager.hpp"

namespace enigma::graphic
{
    struct RendererConfig
    {
    };

    class RendererSubsystem : public core::EngineSubsystem
    {
    public:
        explicit RendererSubsystem(RendererConfig& config);
        ~RendererSubsystem() override;

        // Disable copy and move
        RendererSubsystem(const RendererSubsystem&)            = delete;
        RendererSubsystem& operator=(const RendererSubsystem&) = delete;
        RendererSubsystem(RendererSubsystem&&)                 = delete;
        RendererSubsystem& operator=(RendererSubsystem&&)      = delete;

        // EngineSubsystem interface
        DECLARE_SUBSYSTEM(RendererSubsystem, "renderer", 200)
        void Startup() override;
        void Shutdown() override;
        bool RequiresGameLoop() const override { return true; }

        void Update(float deltaTime) override;

    private:
        // Configuration (reference, not owned)
        RendererConfig m_config;
    };
}
