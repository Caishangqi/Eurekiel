#include "Engine.hpp"
#include "SubsystemManager.hpp"
#include "ErrorWarningAssert.hpp"
#include "../Audio/AudioSubsystem.hpp"
#include "../Resource/ResourceSubsystem.hpp"
#include "Logger/LoggerSubsystem.hpp"
#include "Console/ConsoleSubsystem.hpp"
enigma::core::Engine* g_theEngine = nullptr;

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

    // Non-template subsystem access
    EngineSubsystem* Engine::GetSubsystem(const std::string& name) const
    {
        return m_subsystemManager->GetSubsystem(name);
    }

    EngineSubsystem* Engine::GetSubsystem(const std::type_index& typeId) const
    {
        return m_subsystemManager->GetSubsystem(typeId);
    }

    void Engine::RegisterSubsystem(std::unique_ptr<EngineSubsystem> subsystem)
    {
        m_subsystemManager->RegisterSubsystem(std::move(subsystem));
    }

    // Convenience accessors
    LoggerSubsystem* Engine::GetLogger() const
    {
        return GetSubsystem<LoggerSubsystem>();
    }

    ConsoleSubsystem* Engine::GetConsole() const
    {
        return GetSubsystem<ConsoleSubsystem>();
    }
}
