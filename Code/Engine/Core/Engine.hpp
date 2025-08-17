#pragma once
#include <memory>

namespace enigma::core
{
    class SubsystemManager;

    class Engine
    {
    public:
        static Engine* GetInstance();
        static void    CreateInstance();
        static void    DestroyInstance();

        // Subsystem access
        template <typename T>
        T* GetSubsystem() const;

        // Subsystem registration
        template <typename T>
        void RegisterSubsystem(std::unique_ptr<T> subsystem);

        // Life cycle
        void Startup();
        void Shutdown();

        // Game Loop
        void BeginFrame();
        void Update(float deltaTime);
        void EndFrame();

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
