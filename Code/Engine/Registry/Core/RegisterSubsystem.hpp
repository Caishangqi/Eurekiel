#pragma once
#include "../../Core/SubsystemManager.hpp"
#include "../../Core/Event/EventSubsystem.hpp"
#include "../../Core/Event/StringEventBus.hpp"
#include "IRegistrable.hpp"
#include "RegistrationKey.hpp"
#include "IRegistry.hpp"
#include "Registry.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <shared_mutex>
#include <typeindex>

// Import EventArgs type alias for legacy compatibility
using enigma::event::EventArgs;

namespace enigma::core
{
    /**
     * @struct RegisterConfig
     * @brief Configuration for the RegisterSubsystem
     */
    struct RegisterConfig
    {
        bool enableEvents        = true;
        bool threadSafe          = true;
        bool allowDuplicateNames = false;
        bool enableNamespaces    = true;

        struct NamespaceEntry
        {
            std::string name;
            bool        autoRegister = true;

            NamespaceEntry(const std::string& n, bool autoReg = true)
                : name(n), autoRegister(autoReg)
            {
            }
        };

        std::vector<NamespaceEntry> defaultNamespaces = {
            {"engine", true},
            {"game", true}
        };

        bool IsValid() const;
    };

    /**
     * @class RegisterSubsystem
     * @brief Main registry management subsystem
     * 
     * Similar to Minecraft Neoforge's registry system, this manages multiple
     * registries for different types of objects (blocks, items, entities, etc.)
     */
    class RegisterSubsystem : public EngineSubsystem
    {
    public:
        explicit RegisterSubsystem(const RegisterConfig& config = RegisterConfig{});
        ~RegisterSubsystem() override;

        // Disable copy and move
        RegisterSubsystem(const RegisterSubsystem&)            = delete;
        RegisterSubsystem& operator=(const RegisterSubsystem&) = delete;
        RegisterSubsystem(RegisterSubsystem&&)                 = delete;
        RegisterSubsystem& operator=(RegisterSubsystem&&)      = delete;

        DECLARE_SUBSYSTEM(RegisterSubsystem, "register", 85)
        void Startup() override;
        void Shutdown() override;
        bool RequiresGameLoop() const override { return false; }

        // Configuration access
        const RegisterConfig& GetConfig() const { return m_config; }

        // Registry management
        template <typename T>
        Registry<T>* CreateRegistry(const std::string& typeName)
        {
            static_assert(std::is_base_of_v<IRegistrable, T>, "T must inherit from IRegistrable");

            std::unique_lock<std::shared_mutex> lock(m_registriesMutex);

            auto typeIndex = std::type_index(typeid(T));
            if (m_registriesByType.find(typeIndex) != m_registriesByType.end())
            {
                return static_cast<Registry<T>*>(m_registriesByType[typeIndex].get());
            }

            auto  registry    = std::make_unique<Registry<T>>(typeName, m_config.threadSafe);
            auto* registryPtr = registry.get();

            m_registriesByType[typeIndex] = std::move(registry);
            m_registriesByName[typeName]  = registryPtr;

            return registryPtr;
        }

        template <typename T>
        Registry<T>* GetRegistry()
        {
            std::shared_lock<std::shared_mutex> lock(m_registriesMutex);
            auto                                typeIndex = std::type_index(typeid(T));
            auto                                it        = m_registriesByType.find(typeIndex);
            return it != m_registriesByType.end() ? static_cast<Registry<T>*>(it->second.get()) : nullptr;
        }

        template <typename T>
        const Registry<T>* GetRegistry() const
        {
            std::shared_lock<std::shared_mutex> lock(m_registriesMutex);
            auto                                typeIndex = std::type_index(typeid(T));
            auto                                it        = m_registriesByType.find(typeIndex);
            return it != m_registriesByType.end() ? static_cast<const Registry<T>*>(it->second.get()) : nullptr;
        }

        IRegistry*       GetRegistry(const std::string& typeName);
        const IRegistry* GetRegistry(const std::string& typeName) const;

        std::vector<std::string> GetAllRegistryTypes() const;
        size_t                   GetTotalRegistrations() const;

        void ClearAllRegistries();

        // Convenience registration methods
        template <typename T>
        void RegisterItem(const RegistrationKey& key, std::shared_ptr<T> item)
        {
            static_assert(std::is_base_of_v<IRegistrable, T>, "T must inherit from IRegistrable");

            auto* registry = GetRegistry<T>();
            if (!registry)
            {
                registry = CreateRegistry<T>(typeid(T).name());
            }

            registry->Register(key, item);
            FireRegistrationEvent("RegisterItem", key, typeid(T).name());
        }

        template <typename T>
        void RegisterItem(const std::string& name, std::shared_ptr<T> item)
        {
            RegisterItem<T>(RegistrationKey(name), item);
        }

        template <typename T>
        void RegisterItem(const std::string& namespaceName, const std::string& name, std::shared_ptr<T> item)
        {
            RegisterItem<T>(RegistrationKey(namespaceName, name), item);
        }

        template <typename T>
        void UnregisterItem(const RegistrationKey& key)
        {
            auto* registry = GetRegistry<T>();
            if (registry)
            {
                registry->Unregister(key);
                FireRegistrationEvent("UnregisterItem", key, typeid(T).name());
            }
        }

        template <typename T>
        std::shared_ptr<T> GetItem(const RegistrationKey& key) const
        {
            auto* registry = GetRegistry<T>();
            return registry ? registry->Get(key) : nullptr;
        }

        template <typename T>
        std::shared_ptr<T> GetItem(const std::string& name) const
        {
            return GetItem<T>(RegistrationKey(name));
        }

        template <typename T>
        std::shared_ptr<T> GetItem(const std::string& namespaceName, const std::string& name) const
        {
            return GetItem<T>(RegistrationKey(namespaceName, name));
        }

    private:
        void        InitializeDefaultNamespaces();
        void        FireRegistrationEvent(const std::string& eventType, const RegistrationKey& key, const std::string& typeName);
        static bool Event_RegistrationChanged(EventArgs& args);

    private:
        RegisterConfig m_config;
        bool           m_initialized = false;

        mutable std::shared_mutex                                       m_registriesMutex;
        std::unordered_map<std::type_index, std::unique_ptr<IRegistry>> m_registriesByType;
        std::unordered_map<std::string, IRegistry*>                     m_registriesByName;
    };
}
