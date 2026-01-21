#pragma once
#include "IRegistry.hpp"
#include "IRegistrable.hpp"
#include "RegistrationKey.hpp"
#include "../../Core/Event/EventException.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <set>
#include <shared_mutex>
#include <atomic>
#include <type_traits>
#include <limits>

DECLARE_LOG_CATEGORY_EXTERN(LogRegistry)

namespace enigma::core
{
    /**
     * @class Registry
     * @brief Type-safe registry implementation for registrable objects
     *
     * Similar to Minecraft Neoforge's Registry class, this provides type-safe
     * registration and retrieval of objects.
     *
     * Design Principles:
     * - RAII lock management via LockGuard helper
     * - Template-based lock strategy to eliminate code duplication
     * - Clear separation: public API handles locking, private methods are lock-free
     *
     * Freeze Mechanism:
     * - Call Freeze() after registration phase completes
     * - Once frozen, Register/Unregister/Clear will throw RegistryFrozenException
     */
    template <typename T>
    class Registry : public IRegistry
    {
        static_assert(std::is_base_of_v<IRegistrable, T>, "T must inherit from IRegistrable");

        // ============================================================
        // RAII Lock Guards - Eliminates manual lock management
        // ============================================================

        /// @brief RAII read lock guard (shared access)
        class ReadLock
        {
        public:
            ReadLock(std::shared_mutex& mutex, bool enabled)
                : m_mutex(mutex), m_enabled(enabled)
            {
                if (m_enabled) m_mutex.lock_shared();
            }

            ~ReadLock() { if (m_enabled) m_mutex.unlock_shared(); }
            ReadLock(const ReadLock&)            = delete;
            ReadLock& operator=(const ReadLock&) = delete;

        private:
            std::shared_mutex& m_mutex;
            bool               m_enabled;
        };

        /// @brief RAII write lock guard (exclusive access)
        class WriteLock
        {
        public:
            WriteLock(std::shared_mutex& mutex, bool enabled)
                : m_mutex(mutex), m_enabled(enabled)
            {
                if (m_enabled) m_mutex.lock();
            }

            ~WriteLock() { if (m_enabled) m_mutex.unlock(); }
            WriteLock(const WriteLock&)            = delete;
            WriteLock& operator=(const WriteLock&) = delete;

        private:
            std::shared_mutex& m_mutex;
            bool               m_enabled;
        };

    public:
        explicit Registry(const std::string& typeName, bool threadSafe = true)
            : m_typeName(typeName)
              , m_threadSafe(threadSafe)
              , m_frozen(false)
              , m_maxId((std::numeric_limits<int>::max)() - 1)
        {
            m_objectsById.reserve(1000);
        }

        virtual ~Registry() = default;

        // ============================================================
        // IRegistry Interface Implementation
        // ============================================================

        const std::string& GetRegistryType() const override { return m_typeName; }

        size_t GetRegistrationCount() const override
        {
            ReadLock lock(m_mutex, m_threadSafe);
            return m_registrations.size();
        }

        void Clear() override
        {
            WriteLock lock(m_mutex, m_threadSafe);
            ThrowIfFrozen("Clear");
            DoClear();
        }

        // ============================================================
        // Freeze Mechanism
        // ============================================================

        void Freeze() override
        {
            WriteLock lock(m_mutex, m_threadSafe);
            m_frozen = true;
        }

        bool IsFrozen() const override
        {
            ReadLock lock(m_mutex, m_threadSafe);
            return m_frozen;
        }

        void Unfreeze() override
        {
            WriteLock lock(m_mutex, m_threadSafe);
            m_frozen = false;
        }

        std::vector<RegistrationKey> GetAllKeys() const override
        {
            ReadLock                     lock(m_mutex, m_threadSafe);
            std::vector<RegistrationKey> keys;
            keys.reserve(m_registrations.size());
            for (const auto& pair : m_registrations)
            {
                keys.push_back(pair.first);
            }
            return keys;
        }

        bool HasRegistration(const RegistrationKey& key) const override
        {
            ReadLock lock(m_mutex, m_threadSafe);
            return m_registrations.find(key) != m_registrations.end();
        }

        // ============================================================
        // Type-safe Registration Methods
        // ============================================================

        void Register(const RegistrationKey& key, std::shared_ptr<T> item)
        {
            if (!key.IsValid() || !item)
                return;

            WriteLock lock(m_mutex, m_threadSafe);
            ThrowIfFrozen("Register");
            DoRegister(key, item);
        }

        void Register(const std::string& name, std::shared_ptr<T> item)
        {
            Register(RegistrationKey(name), item);
        }

        void Register(const std::string& namespaceName, const std::string& name, std::shared_ptr<T> item)
        {
            Register(RegistrationKey(namespaceName, name), item);
        }

        void Unregister(const RegistrationKey& key)
        {
            WriteLock lock(m_mutex, m_threadSafe);
            ThrowIfFrozen("Unregister");
            DoUnregister(key);
        }

        void Unregister(const std::string& name)
        {
            Unregister(RegistrationKey(name));
        }

        void Unregister(const std::string& namespaceName, const std::string& name)
        {
            Unregister(RegistrationKey(namespaceName, name));
        }

        // ============================================================
        // Retrieval Methods
        // ============================================================

        std::shared_ptr<T> Get(const RegistrationKey& key) const
        {
            ReadLock lock(m_mutex, m_threadSafe);
            auto     idIt = m_keyToId.find(key);
            if (idIt != m_keyToId.end())
            {
                int id = idIt->second;
                if (id >= 0 && id < static_cast<int>(m_objectsById.size()))
                {
                    return m_objectsById[id];
                }
            }
            return nullptr;
        }

        std::shared_ptr<T> Get(const std::string& name) const
        {
            return Get(RegistrationKey(name));
        }

        std::shared_ptr<T> Get(const std::string& namespaceName, const std::string& name) const
        {
            return Get(RegistrationKey(namespaceName, name));
        }

        std::shared_ptr<T> GetById(int id) const
        {
            if (id < 0)
                return nullptr;

            ReadLock lock(m_mutex, m_threadSafe);
            if (id >= static_cast<int>(m_objectsById.size()))
                return nullptr;
            return m_objectsById[id];
        }

        std::vector<std::shared_ptr<T>> GetByIds(const std::vector<int>& ids) const
        {
            ReadLock                        lock(m_mutex, m_threadSafe);
            std::vector<std::shared_ptr<T>> result;
            result.reserve(ids.size());
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
            return result;
        }

        std::vector<std::shared_ptr<T>> GetAll() const
        {
            ReadLock                        lock(m_mutex, m_threadSafe);
            std::vector<std::shared_ptr<T>> items;
            items.reserve(m_registrations.size());
            for (const auto& pair : m_registrations)
            {
                items.push_back(pair.second);
            }
            return items;
        }

        std::vector<std::shared_ptr<T>> GetByNamespace(const std::string& namespaceName) const
        {
            ReadLock                        lock(m_mutex, m_threadSafe);
            std::vector<std::shared_ptr<T>> items;
            for (const auto& pair : m_registrations)
            {
                if (pair.first.namespaceName == namespaceName)
                {
                    items.push_back(pair.second);
                }
            }
            return items;
        }

        // ============================================================
        // Numeric ID Interface (IRegistry)
        // ============================================================

        int GetId(const RegistrationKey& key) const override
        {
            ReadLock lock(m_mutex, m_threadSafe);
            auto     it = m_keyToId.find(key);
            return it != m_keyToId.end() ? it->second : -1;
        }

        RegistrationKey GetKey(int id) const override
        {
            ReadLock lock(m_mutex, m_threadSafe);
            auto     it = m_idToKey.find(id);
            return it != m_idToKey.end() ? it->second : RegistrationKey();
        }

        int GetNextAvailableId() const override
        {
            ReadLock lock(m_mutex, m_threadSafe);
            if (!m_freeIds.empty())
            {
                return *m_freeIds.begin();
            }
            return m_nextId.load();
        }

        bool HasId(int id) const override
        {
            if (id < 0)
                return false;

            ReadLock lock(m_mutex, m_threadSafe);
            if (id >= static_cast<int>(m_objectsById.size()))
                return false;
            return m_objectsById[id] != nullptr;
        }

        std::vector<int> GetAllIds() const override
        {
            ReadLock         lock(m_mutex, m_threadSafe);
            std::vector<int> result;
            for (int i = 0; i < static_cast<int>(m_objectsById.size()); ++i)
            {
                if (m_objectsById[i] != nullptr)
                {
                    result.push_back(i);
                }
            }
            return result;
        }

        void SetMaxId(int maxId) override
        {
            WriteLock lock(m_mutex, m_threadSafe);
            m_maxId = maxId;
        }

        int GetMaxId() const override
        {
            ReadLock lock(m_mutex, m_threadSafe);
            return m_maxId;
        }

        // ============================================================
        // Iterator Support (Snapshot-based for thread safety)
        // ============================================================

        auto begin() const
        {
            ReadLock lock(m_mutex, m_threadSafe);
            return m_registrations.begin();
        }

        auto end() const
        {
            ReadLock lock(m_mutex, m_threadSafe);
            return m_registrations.end();
        }

    protected:
        // IRegistry protected interface
        int AllocateId() override
        {
            WriteLock lock(m_mutex, m_threadSafe);
            return DoAllocateId();
        }

        void ReleaseId(int id) override
        {
            WriteLock lock(m_mutex, m_threadSafe);
            DoReleaseId(id);
        }

    private:
        // ============================================================
        // Member Variables
        // ============================================================

        std::string               m_typeName;
        bool                      m_threadSafe;
        bool                      m_frozen;
        mutable std::shared_mutex m_mutex;

        // String-based registration
        std::unordered_map<RegistrationKey, std::shared_ptr<T>> m_registrations;

        // Numeric ID system (O(1) access)
        std::vector<std::shared_ptr<T>>          m_objectsById;
        std::unordered_map<RegistrationKey, int> m_keyToId;
        std::unordered_map<int, RegistrationKey> m_idToKey;

        // ID allocation
        std::atomic<int> m_nextId{0};
        std::set<int>    m_freeIds;
        int              m_maxId;

        // ============================================================
        // Private Implementation (Lock-Free - caller must hold lock)
        // ============================================================

        /// @brief Check frozen state and throw if frozen
        /// @note Caller MUST hold lock before calling
        void ThrowIfFrozen(const std::string& operation)
        {
            if (m_frozen)
            {
                throw enigma::event::RegistryFrozenException(m_typeName, operation);
            }
        }

        /// @brief Perform registration
        /// @note Caller MUST hold write lock before calling
        void DoRegister(const RegistrationKey& key, std::shared_ptr<T> item)
        {
            if (m_registrations.find(key) != m_registrations.end())
            {
                return; // Already registered
            }

            int id = DoAllocateId();
            if (id < 0)
            {
                return; // ID allocation failed
            }

            m_registrations[key] = item;
            m_keyToId[key]       = id;
            m_idToKey[id]        = key;

            if (id >= static_cast<int>(m_objectsById.size()))
            {
                m_objectsById.resize(id + 1);
            }
            m_objectsById[id] = item;

            if constexpr (std::is_base_of_v<IRegistrable, T>)
            {
                item->SetNumericId(id);
            }
        }

        /// @brief Perform unregistration
        /// @note Caller MUST hold write lock before calling
        void DoUnregister(const RegistrationKey& key)
        {
            auto regIt = m_registrations.find(key);
            if (regIt == m_registrations.end())
            {
                return;
            }

            auto keyToIdIt = m_keyToId.find(key);
            if (keyToIdIt != m_keyToId.end())
            {
                int id = keyToIdIt->second;

                if (id >= 0 && id < static_cast<int>(m_objectsById.size()))
                {
                    m_objectsById[id] = nullptr;
                }

                m_keyToId.erase(keyToIdIt);
                m_idToKey.erase(id);
                DoReleaseId(id);
            }

            m_registrations.erase(regIt);
        }

        /// @brief Clear all data
        /// @note Caller MUST hold write lock before calling
        void DoClear()
        {
            m_registrations.clear();
            m_objectsById.clear();
            m_keyToId.clear();
            m_idToKey.clear();
            m_freeIds.clear();
            m_nextId.store(0);
        }

        /// @brief Allocate a new ID
        /// @note Caller MUST hold write lock before calling
        int DoAllocateId()
        {
            if (!m_freeIds.empty())
            {
                int id = *m_freeIds.begin();
                m_freeIds.erase(m_freeIds.begin());
                return id;
            }

            int id = m_nextId.fetch_add(1);
            if (id > m_maxId)
            {
                return -1;
            }
            return id;
        }

        /// @brief Release an ID for reuse
        /// @note Caller MUST hold write lock before calling
        void DoReleaseId(int id)
        {
            if (id >= 0)
            {
                m_freeIds.insert(id);
            }
        }
    };
}
