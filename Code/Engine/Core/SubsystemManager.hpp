#pragma once
#pragma once
#include <string>

namespace enigma::core
{
    class EngineSubsystem
    {
    public:
        virtual             ~EngineSubsystem() = default;
        virtual void        Startup() = 0;
        virtual void        Shutdown() = 0;
        virtual std::string GetSubsystemName() const = 0;

        // Lifecycle methods
        virtual void BeginFrame()
        {
        }

        virtual void Update(float deltaTime)
        {
            (void)deltaTime;
        }

        virtual void EndFrame()
        {
        }
    };

    class SubsystemManager
    {
    public:
    };
}
