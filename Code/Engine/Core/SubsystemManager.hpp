#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include "Yaml.hpp"

namespace enigma::core
{
    class EngineSubsystem
    {
    public:
        virtual ~EngineSubsystem() = default;
        
        // Two-phase startup system
        virtual void Initialize() {} // For early initialization (e.g., register loaders)
        virtual void Startup() = 0;  // Main startup after all Initialize phases
        virtual void Shutdown() = 0;
        
        virtual const char* GetSubsystemName() const = 0;
        virtual int GetPriority() const = 0;
        virtual bool RequiresGameLoop() const { return true; }
        virtual bool RequiresInitialize() const { return false; } // Only for subsystems that need early init

        // Lifecycle methods for game loop subsystems
        virtual void BeginFrame() {}
        virtual void Update(float deltaTime) { (void)deltaTime; }
        virtual void EndFrame() {}

        // Helper macro for subsystem implementation
        #define DECLARE_SUBSYSTEM(ClassName, Name, Priority) \
            static const char* GetStaticSubsystemName() { return Name; } \
            const char* GetSubsystemName() const override { return Name; } \
            static int GetStaticPriority() { return Priority; } \
            int GetPriority() const override { return Priority; }
    };

    class SubsystemManager
    {
    public:
        SubsystemManager();
        ~SubsystemManager();

        // Configuration loading
        void LoadConfiguration(const std::string& configPath, const std::string& modulePath);

        // Subsystem registration and access
        template<typename T>
        void RegisterSubsystem(std::unique_ptr<T> subsystem);

        template<typename T>
        T* GetSubsystem() const;

        EngineSubsystem* GetSubsystem(const std::string& name) const;

        // Lifecycle management
        void InitializeAllSubsystems();  // First phase: Initialize subsystems that need early setup
        void StartupAllSubsystems();     // Second phase: Main startup after all Initialize phases
        void ShutdownAllSubsystems();

        // Game loop methods (only for subsystems that require game loop)
        void BeginFrameAllSubsystems();
        void UpdateAllSubsystems(float deltaTime);
        void EndFrameAllSubsystems();

    private:
        // Internal structures
        struct SubsystemEntry
        {
            std::unique_ptr<EngineSubsystem> subsystem;
            std::vector<std::string> dependencies;
            bool isStarted = false;
        };

        // Storage
        std::unordered_map<std::type_index, EngineSubsystem*> m_subsystemsByType;
        std::unordered_map<std::string, std::unique_ptr<SubsystemEntry>> m_subsystemsByName;
        std::vector<EngineSubsystem*> m_startupOrder;
        std::vector<EngineSubsystem*> m_gameLoopSubsystems;

        // Configuration
        YamlConfiguration m_engineConfig;
        YamlConfiguration m_moduleConfig;

        // Internal methods
        void SortSubsystemsByPriority();
        void ValidateDependencies();
        void CreateStartupOrder();
        std::vector<std::string> GetEnabledModules() const;
        std::vector<std::string> GetSubsystemDependencies(const std::string& subsystemName) const;
    };

    // Template implementations
    template<typename T>
    void SubsystemManager::RegisterSubsystem(std::unique_ptr<T> subsystem)
    {
        static_assert(std::is_base_of_v<EngineSubsystem, T>, "T must derive from EngineSubsystem");
        
        const char* name = subsystem->GetSubsystemName();
        auto entry = std::make_unique<SubsystemEntry>();
        entry->subsystem = std::move(subsystem);
        entry->dependencies = GetSubsystemDependencies(name);

        // Store both by type and by name
        m_subsystemsByType[std::type_index(typeid(T))] = entry->subsystem.get();
        m_subsystemsByName[name] = std::move(entry);
    }

    template<typename T>
    T* SubsystemManager::GetSubsystem() const
    {
        auto it = m_subsystemsByType.find(std::type_index(typeid(T)));
        if (it != m_subsystemsByType.end())
        {
            return static_cast<T*>(it->second);
        }
        return nullptr;
    }
}
