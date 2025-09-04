#pragma once
#include <memory>
#include <string>
#include <typeindex>

namespace enigma::core
{
    class SubsystemManager;
    class EngineSubsystem;
    class LoggerSubsystem;
    class ConsoleSubsystem;

    class Engine
    {
    public:
        static Engine* GetInstance();
        static void    CreateInstance();
        static void    DestroyInstance();

        // Subsystem access (non-template version)
        EngineSubsystem* GetSubsystem(const std::string& name) const;
        EngineSubsystem* GetSubsystem(const std::type_index& typeId) const;

        // Template convenience methods (inline)
        template <typename T>
        T* GetSubsystem() const
        {
            auto* subsystem = GetSubsystem(std::type_index(typeid(T)));
            return static_cast<T*>(subsystem);
        }

        // Subsystem registration (non-template version)
        void RegisterSubsystem(std::unique_ptr<EngineSubsystem> subsystem);

        // Life cycle
        void Startup();
        void Shutdown();

        // Game Loop
        void BeginFrame();
        void Update(float deltaTime);
        void EndFrame();

        // Convenience accessors for commonly used subsystems
        LoggerSubsystem*  GetLogger() const;
        ConsoleSubsystem* GetConsole() const;

    private:
        Engine();
        ~Engine();

        // Singleton
        static Engine*                    s_instance;
        std::unique_ptr<SubsystemManager> m_subsystemManager;
    };
}

// Convenient macro definition
#define GEngine enigma::core::Engine::GetInstance()
