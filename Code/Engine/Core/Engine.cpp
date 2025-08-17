#include "Engine.hpp"
#include "SubsystemManager.hpp"
#include "ErrorWarningAssert.hpp"
#include "../Audio/AudioSubsystem.hpp"
#include "../Resource/ResourceSubsystem.hpp"

namespace enigma::core
{
    Engine* Engine::s_instance = nullptr;

    Engine::Engine()
        : m_subsystemManager(std::make_unique<SubsystemManager>())
    {
    }

    Engine::~Engine() = default;

    Engine* Engine::GetInstance()
    {
        ASSERT_OR_DIE(s_instance != nullptr, "Engine instance not created. Call CreateInstance() first.")
        return s_instance;
    }

    void Engine::CreateInstance()
    {
        if (s_instance == nullptr)
        {
            s_instance = new Engine();
        }
    }

    void Engine::DestroyInstance()
    {
        if (s_instance != nullptr)
        {
            delete s_instance;
            s_instance = nullptr;
        }
    }

    void Engine::Startup()
    {
        // Load configuration files
        m_subsystemManager->LoadConfiguration(
            ".enigma/config/engine/config.yml",
            ".enigma/config/engine/module.yml"
        );

        // Two-phase startup
        m_subsystemManager->InitializeAllSubsystems(); // Phase 1: Early initialization
        m_subsystemManager->StartupAllSubsystems(); // Phase 2: Main startup
    }

    void Engine::Shutdown()
    {
        m_subsystemManager->ShutdownAllSubsystems();
    }

    void Engine::BeginFrame()
    {
        m_subsystemManager->BeginFrameAllSubsystems();
    }

    void Engine::Update(float deltaTime)
    {
        m_subsystemManager->UpdateAllSubsystems(deltaTime);
    }

    void Engine::EndFrame()
    {
        m_subsystemManager->EndFrameAllSubsystems();
    }

    // Template method implementations
    template <typename T>
    T* Engine::GetSubsystem() const
    {
        return m_subsystemManager->GetSubsystem<T>();
    }

    template <typename T>
    void Engine::RegisterSubsystem(std::unique_ptr<T> subsystem)
    {
        m_subsystemManager->RegisterSubsystem(std::move(subsystem));
    }

    // Explicit instantiations for commonly used types
    template AudioSubsystem*                      Engine::GetSubsystem<AudioSubsystem>() const;
    template enigma::resource::ResourceSubsystem* Engine::GetSubsystem<enigma::resource::ResourceSubsystem>() const;
    template void                                 Engine::RegisterSubsystem<AudioSubsystem>(std::unique_ptr<AudioSubsystem>);
    template void                                 Engine::RegisterSubsystem<enigma::resource::ResourceSubsystem>(std::unique_ptr<enigma::resource::ResourceSubsystem>);
}
