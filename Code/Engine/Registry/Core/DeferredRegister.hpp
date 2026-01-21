#pragma once

// ============================================================================
// DeferredRegister.hpp - Deferred registration helper
// Part of Registry System
// Collects registrations and executes them when RegisterEvent is posted
// ============================================================================

#include "DeferredHolder.hpp"
#include "Engine/Core/Event/EventBus.hpp"
#include "Engine/Core/Event/RegisterEvent.hpp"
#include "Engine/Core/Event/EventException.hpp"
#include "Engine/Core/Event/EventCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace enigma::registry
{
    //-----------------------------------------------------------------------------------------------
    // Deferred Register
    //
    // DESIGN PHILOSOPHY:
    // - Collects registration entries during static initialization
    // - Executes actual registration when RegisterEvent is posted
    // - Prevents registration after freeze (throws exception)
    // - Similar to NeoForge's DeferredRegister pattern
    //
    // LIFECYCLE:
    // 1. Static initialization: Create DeferredRegister, call Register()
    // 2. Mod initialization: Call Register(eventBus) to bind to ModBus
    // 3. Registration phase: RegisterEvent triggers OnRegisterEvent()
    // 4. Freeze: No more registrations allowed
    //
    // USAGE:
    //   // In header (static initialization)
    //   static DeferredRegister<Block, BlockRegistry> BLOCKS("mymod");
    //   static auto STONE = BLOCKS.Register("stone", []() { return std::make_unique<Block>(); });
    //
    //   // In initialization
    //   BLOCKS.Register(modBus);
    //-----------------------------------------------------------------------------------------------
    template <typename T, typename TRegistry>
    class DeferredRegister
    {
    public:
        using SupplierFunc = std::function<std::unique_ptr<T>()>;
        using HolderPtr    = std::shared_ptr<DeferredHolder<T>>;

        explicit DeferredRegister(const std::string& namespaceId)
            : m_namespace(namespaceId)
        {
            using namespace enigma::event;
            LogDebug(LogEvent, "DeferredRegister::Create Created for namespace '%s'", namespaceId.c_str());
        }

        ~DeferredRegister() = default;

        // Disable copy
        DeferredRegister(const DeferredRegister&)            = delete;
        DeferredRegister& operator=(const DeferredRegister&) = delete;

        //-------------------------------------------------------------------------------------------
        // Registration Methods
        //-------------------------------------------------------------------------------------------

        /// Register an object with a supplier function
        /// @param name Object name (without namespace)
        /// @param supplier Function that creates the object
        /// @return Shared pointer to DeferredHolder for later access
        template <typename TDerived = T>
        std::shared_ptr<DeferredHolder<TDerived>> Register(
            const std::string&                         name,
            std::function<std::unique_ptr<TDerived>()> supplier)
        {
            using namespace enigma::event;

            if (m_frozen)
            {
                const std::string fullId = m_namespace + ":" + name;
                LogError(LogEvent, "DeferredRegister::Register Cannot register '%s' after freeze", fullId.c_str());
                throw RegistryFrozenException("DeferredRegister<" + m_namespace + ">", fullId);
            }

            const std::string fullId = m_namespace + ":" + name;
            auto              holder = std::make_shared<DeferredHolder<TDerived>>(fullId);

            // Store entry for later registration
            Entry entry;
            entry.id       = fullId;
            entry.supplier = [supplier]() -> std::unique_ptr<T>
            {
                return supplier();
            };
            entry.resolver = [holder](T* ptr)
            {
                holder->Resolve(static_cast<TDerived*>(ptr));
            };

            m_entries.push_back(std::move(entry));

            LogDebug(LogEvent, "DeferredRegister::Register Queued '%s' for registration", fullId.c_str());
            return holder;
        }

        /// Bind to event bus to receive RegisterEvent
        void Register(enigma::event::EventBus& bus)
        {
            using namespace enigma::event;

            bus.AddListener<RegisterEvent<TRegistry>>(
                [this](RegisterEvent<TRegistry>& event)
                {
                    this->OnRegisterEvent(event);
                },
                EventPriority::Normal,
                false
            );

            LogInfo(LogEvent, "DeferredRegister::Register Bound to EventBus for namespace '%s'", m_namespace.c_str());
        }

        //-------------------------------------------------------------------------------------------
        // Query Methods
        //-------------------------------------------------------------------------------------------

        /// Get the namespace
        const std::string& GetNamespace() const { return m_namespace; }

        /// Check if frozen (no more registrations allowed)
        bool IsFrozen() const { return m_frozen; }

        /// Get number of pending entries
        size_t GetEntryCount() const { return m_entries.size(); }

    private:
        struct Entry
        {
            std::string             id;
            SupplierFunc            supplier;
            std::function<void(T*)> resolver;
        };

        void OnRegisterEvent(enigma::event::RegisterEvent<TRegistry>& event)
        {
            using namespace enigma::event;

            LogInfo(LogEvent, "DeferredRegister::OnRegisterEvent Processing %zu entries for '%s'",
                    m_entries.size(), m_namespace.c_str());

            auto& registry = event.GetRegistry();

            for (auto& entry : m_entries)
            {
                try
                {
                    // Create object using supplier
                    auto object = entry.supplier();
                    T*   rawPtr = object.get();

                    // Register to registry (registry takes ownership)
                    registry.Register(entry.id, std::move(object));

                    // Resolve the holder
                    entry.resolver(rawPtr);

                    LogDebug(LogEvent, "DeferredRegister::OnRegisterEvent Registered '%s'", entry.id.c_str());
                }
                catch (const std::exception& e)
                {
                    LogError(LogEvent, "DeferredRegister::OnRegisterEvent Failed to register '%s': %s",
                             entry.id.c_str(), e.what());
                    throw;
                }
            }

            // Mark as frozen after registration
            m_frozen = true;
            LogInfo(LogEvent, "DeferredRegister::OnRegisterEvent Completed, namespace '%s' is now frozen",
                    m_namespace.c_str());
        }

        std::string        m_namespace;
        std::vector<Entry> m_entries;
        bool               m_frozen = false;
    };
} // namespace enigma::registry
