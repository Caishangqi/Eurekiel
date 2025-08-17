#include "SubsystemManager.hpp"
#include "ErrorWarningAssert.hpp"
#include <algorithm>
#include <unordered_set>

namespace enigma::core
{
    SubsystemManager::SubsystemManager() = default;

    SubsystemManager::~SubsystemManager()
    {
        ShutdownAllSubsystems();
    }

    void SubsystemManager::LoadConfiguration(const std::string& configPath, const std::string& modulePath)
    {
        m_engineConfig = YamlConfiguration::LoadFromFile(configPath);
        m_moduleConfig = YamlConfiguration::LoadFromFile(modulePath);
    }

    EngineSubsystem* SubsystemManager::GetSubsystem(const std::string& name) const
    {
        auto it = m_subsystemsByName.find(name);
        if (it != m_subsystemsByName.end())
        {
            return it->second->subsystem.get();
        }
        return nullptr;
    }

    EngineSubsystem* SubsystemManager::GetSubsystem(const std::type_index& typeId) const
    {
        auto it = m_subsystemsByType.find(typeId);
        if (it != m_subsystemsByType.end())
        {
            return it->second;
        }
        return nullptr;
    }

    void SubsystemManager::RegisterSubsystem(std::unique_ptr<EngineSubsystem> subsystem)
    {
        const char* name = subsystem->GetSubsystemName();
        auto entry = std::make_unique<SubsystemEntry>();
        
        // Store type info for type-based lookup
        std::type_index typeId = std::type_index(typeid(*subsystem));
        m_subsystemsByType[typeId] = subsystem.get();
        
        entry->subsystem = std::move(subsystem);
        entry->dependencies = GetSubsystemDependencies(name);
        m_subsystemsByName[name] = std::move(entry);
    }

    void SubsystemManager::InitializeAllSubsystems()
    {
        CreateStartupOrder();
        
        // Phase 1: Initialize subsystems that require early setup (e.g., AudioSubsystem needs to register loaders)
        for (auto* subsystem : m_startupOrder)
        {
            if (subsystem->RequiresInitialize())
            {
                subsystem->Initialize();
            }
        }
    }

    void SubsystemManager::StartupAllSubsystems()
    {
        // Phase 2: Main startup after all Initialize phases
        for (auto* subsystem : m_startupOrder)
        {
            const char* name = subsystem->GetSubsystemName();
            auto& entry = m_subsystemsByName[name];
            
            if (!entry->isStarted)
            {
                subsystem->Startup();
                entry->isStarted = true;
            }
        }

        // Collect subsystems that require game loop
        m_gameLoopSubsystems.clear();
        for (auto* subsystem : m_startupOrder)
        {
            if (subsystem->RequiresGameLoop())
            {
                m_gameLoopSubsystems.push_back(subsystem);
            }
        }
    }

    void SubsystemManager::ShutdownAllSubsystems()
    {
        // Shutdown in reverse order
        for (auto it = m_startupOrder.rbegin(); it != m_startupOrder.rend(); ++it)
        {
            auto* subsystem = *it;
            const char* name = subsystem->GetSubsystemName();
            auto& entry = m_subsystemsByName[name];
            
            if (entry->isStarted)
            {
                subsystem->Shutdown();
                entry->isStarted = false;
            }
        }
    }

    void SubsystemManager::BeginFrameAllSubsystems()
    {
        for (auto* subsystem : m_gameLoopSubsystems)
        {
            subsystem->BeginFrame();
        }
    }

    void SubsystemManager::UpdateAllSubsystems(float deltaTime)
    {
        for (auto* subsystem : m_gameLoopSubsystems)
        {
            subsystem->Update(deltaTime);
        }
    }

    void SubsystemManager::EndFrameAllSubsystems()
    {
        for (auto* subsystem : m_gameLoopSubsystems)
        {
            subsystem->EndFrame();
        }
    }

    void SubsystemManager::SortSubsystemsByPriority()
    {
        std::vector<EngineSubsystem*> subsystems;
        for (const auto& pair : m_subsystemsByName)
        {
            subsystems.push_back(pair.second->subsystem.get());
        }

        std::sort(subsystems.begin(), subsystems.end(),
            [](const EngineSubsystem* a, const EngineSubsystem* b)
            {
                return a->GetPriority() < b->GetPriority();
            });

        m_startupOrder = std::move(subsystems);
    }

    void SubsystemManager::ValidateDependencies()
    {
        for (const auto& pair : m_subsystemsByName)
        {
            const std::string& subsystemName = pair.first;
            const auto& dependencies = pair.second->dependencies;

            for (const std::string& dependency : dependencies)
            {
                if (m_subsystemsByName.find(dependency) == m_subsystemsByName.end())
                {
                    ERROR_RECOVERABLE(("Subsystem '" + subsystemName + "' depends on '" + dependency + "' which is not registered").c_str());
                }
            }
        }
    }

    void SubsystemManager::CreateStartupOrder()
    {
        ValidateDependencies();
        
        // Simple priority-based ordering for now
        // Future enhancement: Implement proper topological sort for dependencies
        SortSubsystemsByPriority();
    }

    std::vector<std::string> SubsystemManager::GetEnabledModules() const
    {
        std::vector<std::string> modules;
        if (m_engineConfig.Contains("engine.modules"))
        {
            modules = m_engineConfig.GetStringList("engine.modules");
        }
        return modules;
    }

    std::vector<std::string> SubsystemManager::GetSubsystemDependencies(const std::string& subsystemName) const
    {
        std::vector<std::string> dependencies;
        std::string path = "moduleConfig." + subsystemName + ".dependencies";
        
        if (m_moduleConfig.Contains(path))
        {
            dependencies = m_moduleConfig.GetStringList(path);
        }
        return dependencies;
    }
}
