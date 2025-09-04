#pragma once
#include "IRegistry.hpp"
#include "IRegistrable.hpp"
#include "RegistrationKey.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <type_traits>

namespace enigma::core
{
    /**
     * @class Registry
     * @brief Type-safe registry implementation for registrable objects
     * 
     * Similar to Minecraft Neoforge's Registry class, this provides type-safe
     * registration and retrieval of objects.
     */
    template <typename T>
    class Registry : public IRegistry
    {
        static_assert(std::is_base_of_v<IRegistrable, T>, "T must inherit from IRegistrable");

    public:
        explicit Registry(const std::string& typeName, bool threadSafe = true)
            : m_typeName(typeName), m_threadSafe(threadSafe)
        {
        }

        virtual ~Registry() = default;

        // IRegistry interface
        const std::string& GetRegistryType() const override { return m_typeName; }

        size_t GetRegistrationCount() const override
        {
            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                return m_registrations.size();
            }
            else
            {
                return m_registrations.size();
            }
        }

        void Clear() override
        {
            if (m_threadSafe)
            {
                std::unique_lock<std::shared_mutex> lock(m_mutex);
                m_registrations.clear();
            }
            else
            {
                m_registrations.clear();
            }
        }

        std::vector<RegistrationKey> GetAllKeys() const override
        {
            std::vector<RegistrationKey> keys;
            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                keys.reserve(m_registrations.size());
                for (const auto& pair : m_registrations)
                {
                    keys.push_back(pair.first);
                }
            }
            else
            {
                keys.reserve(m_registrations.size());
                for (const auto& pair : m_registrations)
                {
                    keys.push_back(pair.first);
                }
            }
            return keys;
        }

        bool HasRegistration(const RegistrationKey& key) const override
        {
            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                return m_registrations.find(key) != m_registrations.end();
            }
            else
            {
                return m_registrations.find(key) != m_registrations.end();
            }
        }

        // Type-safe registration methods
        void Register(const RegistrationKey& key, std::shared_ptr<T> item)
        {
            if (!key.IsValid() || !item)
                return;

            if (m_threadSafe)
            {
                std::unique_lock<std::shared_mutex> lock(m_mutex);
                m_registrations[key] = item;
            }
            else
            {
                m_registrations[key] = item;
            }
        }

        void Register(const std::string& name, std::shared_ptr<T> item)
        {
            Register(RegistrationKey(name), item);
        }

        void Register(const std::string& namespaceName, const std::string& name, std::shared_ptr<T> item)
        {
            Register(RegistrationKey(namespaceName, name), item);
        }

        // Unregistration
        void Unregister(const RegistrationKey& key)
        {
            if (m_threadSafe)
            {
                std::unique_lock<std::shared_mutex> lock(m_mutex);
                m_registrations.erase(key);
            }
            else
            {
                m_registrations.erase(key);
            }
        }

        void Unregister(const std::string& name)
        {
            Unregister(RegistrationKey(name));
        }

        void Unregister(const std::string& namespaceName, const std::string& name)
        {
            Unregister(RegistrationKey(namespaceName, name));
        }

        // Retrieval methods
        std::shared_ptr<T> Get(const RegistrationKey& key) const
        {
            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                auto                                it = m_registrations.find(key);
                return it != m_registrations.end() ? it->second : nullptr;
            }
            else
            {
                auto it = m_registrations.find(key);
                return it != m_registrations.end() ? it->second : nullptr;
            }
        }

        std::shared_ptr<T> Get(const std::string& name) const
        {
            return Get(RegistrationKey(name));
        }

        std::shared_ptr<T> Get(const std::string& namespaceName, const std::string& name) const
        {
            return Get(RegistrationKey(namespaceName, name));
        }

        // Bulk operations
        std::vector<std::shared_ptr<T>> GetAll() const
        {
            std::vector<std::shared_ptr<T>> items;
            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                items.reserve(m_registrations.size());
                for (const auto& pair : m_registrations)
                {
                    items.push_back(pair.second);
                }
            }
            else
            {
                items.reserve(m_registrations.size());
                for (const auto& pair : m_registrations)
                {
                    items.push_back(pair.second);
                }
            }
            return items;
        }

        std::vector<std::shared_ptr<T>> GetByNamespace(const std::string& namespaceName) const
        {
            std::vector<std::shared_ptr<T>> items;
            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                for (const auto& pair : m_registrations)
                {
                    if (pair.first.namespaceName == namespaceName)
                    {
                        items.push_back(pair.second);
                    }
                }
            }
            else
            {
                for (const auto& pair : m_registrations)
                {
                    if (pair.first.namespaceName == namespaceName)
                    {
                        items.push_back(pair.second);
                    }
                }
            }
            return items;
        }

        // Iterator support
        auto begin() const
        {
            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                return m_registrations.begin();
            }
            else
            {
                return m_registrations.begin();
            }
        }

        auto end() const
        {
            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                return m_registrations.end();
            }
            else
            {
                return m_registrations.end();
            }
        }

    private:
        std::string                                             m_typeName;
        bool                                                    m_threadSafe;
        mutable std::shared_mutex                               m_mutex;
        std::unordered_map<RegistrationKey, std::shared_ptr<T>> m_registrations;
    };
}
