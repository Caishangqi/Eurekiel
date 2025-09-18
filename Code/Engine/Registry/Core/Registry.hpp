#pragma once
#include "IRegistry.hpp"
#include "IRegistrable.hpp"
#include "RegistrationKey.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <set>
#include <shared_mutex>
#include <atomic>
#include <type_traits>
#include <limits>

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
            : m_typeName(typeName), m_threadSafe(threadSafe), m_maxId((std::numeric_limits<int>::max)() - 1)
        {
            // Pre-allocate some space for performance
            m_objectsById.reserve(1000);
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
                ClearInternal();
            }
            else
            {
                ClearInternal();
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
                RegisterInternal(key, item);
            }
            else
            {
                RegisterInternal(key, item);
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

        // High-performance numeric ID access
        std::shared_ptr<T> GetById(int id) const
        {
            if (id < 0 || id >= static_cast<int>(m_objectsById.size()))
                return nullptr;

            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                return m_objectsById[id]; // O(1) access
            }
            else
            {
                return m_objectsById[id];
            }
        }

        // Batch high-performance access
        std::vector<std::shared_ptr<T>> GetByIds(const std::vector<int>& ids) const
        {
            std::vector<std::shared_ptr<T>> result;
            result.reserve(ids.size());

            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                for (int id : ids)
                {
                    if (id >= 0 && id < static_cast<int>(m_objectsById.size()))
                    {
                        result.push_back(m_objectsById[id]);
                    }
                    else
                    {
                        result.push_back(nullptr);
                    }
                }
            }
            else
            {
                for (int id : ids)
                {
                    if (id >= 0 && id < static_cast<int>(m_objectsById.size()))
                    {
                        result.push_back(m_objectsById[id]);
                    }
                    else
                    {
                        result.push_back(nullptr);
                    }
                }
            }
            return result;
        }

        // IRegistry numeric ID interface implementation
        int GetId(const RegistrationKey& key) const override
        {
            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                auto                                it = m_keyToId.find(key);
                return it != m_keyToId.end() ? it->second : -1;
            }
            else
            {
                auto it = m_keyToId.find(key);
                return it != m_keyToId.end() ? it->second : -1;
            }
        }

        RegistrationKey GetKey(int id) const override
        {
            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                auto                                it = m_idToKey.find(id);
                return it != m_idToKey.end() ? it->second : RegistrationKey();
            }
            else
            {
                auto it = m_idToKey.find(id);
                return it != m_idToKey.end() ? it->second : RegistrationKey();
            }
        }

        int GetNextAvailableId() const override
        {
            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                return GetNextAvailableIdInternal();
            }
            else
            {
                return GetNextAvailableIdInternal();
            }
        }

        bool HasId(int id) const override
        {
            if (id < 0 || id >= static_cast<int>(m_objectsById.size()))
                return false;

            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                return m_objectsById[id] != nullptr;
            }
            else
            {
                return m_objectsById[id] != nullptr;
            }
        }

        std::vector<int> GetAllIds() const override
        {
            std::vector<int> result;

            if (m_threadSafe)
            {
                std::shared_lock<std::shared_mutex> lock(m_mutex);
                for (int i = 0; i < static_cast<int>(m_objectsById.size()); ++i)
                {
                    if (m_objectsById[i] != nullptr)
                    {
                        result.push_back(i);
                    }
                }
            }
            else
            {
                for (int i = 0; i < static_cast<int>(m_objectsById.size()); ++i)
                {
                    if (m_objectsById[i] != nullptr)
                    {
                        result.push_back(i);
                    }
                }
            }
            return result;
        }

        void SetMaxId(int maxId) override
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_maxId = maxId;
        }

        int GetMaxId() const override
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_maxId;
        }

        // Unregistration
        void Unregister(const RegistrationKey& key)
        {
            if (m_threadSafe)
            {
                std::unique_lock<std::shared_mutex> lock(m_mutex);
                UnregisterInternal(key);
            }
            else
            {
                UnregisterInternal(key);
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

                // Optimized: use ID mapping for O(1) access
                auto idIt = m_keyToId.find(key);
                if (idIt != m_keyToId.end())
                {
                    int id = idIt->second;
                    if (id >= 0 && id < static_cast<int>(m_objectsById.size()))
                    {
                        return m_objectsById[id]; // O(1) array access
                    }
                }
                return nullptr;
            }
            else
            {
                // Optimized: use ID mapping for O(1) access
                auto idIt = m_keyToId.find(key);
                if (idIt != m_keyToId.end())
                {
                    int id = idIt->second;
                    if (id >= 0 && id < static_cast<int>(m_objectsById.size()))
                    {
                        return m_objectsById[id]; // O(1) array access
                    }
                }
                return nullptr;
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
        std::string               m_typeName;
        bool                      m_threadSafe;
        mutable std::shared_mutex m_mutex;

        // String-based registration (compatibility)
        std::unordered_map<RegistrationKey, std::shared_ptr<T>> m_registrations;

        // Numeric ID system (performance)
        std::vector<std::shared_ptr<T>>          m_objectsById; // ID -> Object mapping
        std::unordered_map<RegistrationKey, int> m_keyToId; // Key -> ID mapping
        std::unordered_map<int, RegistrationKey> m_idToKey; // ID -> Key mapping

        // ID allocation management
        std::atomic<int> m_nextId{0};
        std::set<int>    m_freeIds; // Released IDs for reuse
        int              m_maxId;

        // Internal helper methods
        void RegisterInternal(const RegistrationKey& key, std::shared_ptr<T> item)
        {
            // Check if already registered
            if (m_registrations.find(key) != m_registrations.end())
            {
                // Log warning but don't fail - allow re-registration
                return;
            }

            // Allocate numeric ID
            int id = AllocateIdInternal();
            if (id < 0)
            {
                // ID allocation failed
                return;
            }

            // Update all mappings
            m_registrations[key] = item;
            m_keyToId[key]       = id;
            m_idToKey[id]        = key;

            // Ensure array size
            if (id >= static_cast<int>(m_objectsById.size()))
            {
                m_objectsById.resize(id + 1);
            }
            m_objectsById[id] = item;

            // Set numeric ID in the object if it's an IRegistrable
            if constexpr (std::is_base_of_v<IRegistrable, T>)
            {
                item->SetNumericId(id);
            }
        }

        int AllocateIdInternal()
        {
            // Prioritize reusing released IDs
            if (!m_freeIds.empty())
            {
                int id = *m_freeIds.begin();
                m_freeIds.erase(m_freeIds.begin());
                return id;
            }

            // Allocate new ID
            int id = m_nextId.fetch_add(1);
            if (id > m_maxId)
            {
                // Log error - ID overflow
                return -1;
            }
            return id;
        }

        void ReleaseIdInternal(int id)
        {
            if (id >= 0)
            {
                m_freeIds.insert(id);
            }
        }

        int GetNextAvailableIdInternal() const
        {
            if (!m_freeIds.empty())
            {
                return *m_freeIds.begin();
            }
            return m_nextId.load();
        }

        void ClearInternal()
        {
            m_registrations.clear();
            m_objectsById.clear();
            m_keyToId.clear();
            m_idToKey.clear();
            m_freeIds.clear();
            m_nextId.store(0);
        }

        void UnregisterInternal(const RegistrationKey& key)
        {
            auto regIt = m_registrations.find(key);
            if (regIt != m_registrations.end())
            {
                // Find and release the numeric ID
                auto keyToIdIt = m_keyToId.find(key);
                if (keyToIdIt != m_keyToId.end())
                {
                    int id = keyToIdIt->second;

                    // Clear object by ID
                    if (id >= 0 && id < static_cast<int>(m_objectsById.size()))
                    {
                        m_objectsById[id] = nullptr;
                    }

                    // Remove mappings
                    m_keyToId.erase(keyToIdIt);
                    m_idToKey.erase(id);

                    // Release ID for reuse
                    ReleaseIdInternal(id);
                }

                // Remove registration
                m_registrations.erase(regIt);
            }
        }

    protected:
        int  AllocateId() override { return AllocateIdInternal(); }
        void ReleaseId(int id) override { ReleaseIdInternal(id); }
    };
}
