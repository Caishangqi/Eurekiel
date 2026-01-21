#pragma once

// ============================================================================
// DeferredHolder.hpp - Deferred object holder for lazy registration
// Part of Registry System
// Provides safe access to objects that are registered later
// ============================================================================

#include "Engine/Core/Event/EventException.hpp"
#include "Engine/Core/Event/EventCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include <string>
#include <memory>

namespace enigma::registry
{
    //-----------------------------------------------------------------------------------------------
    // Deferred Holder
    //
    // DESIGN PHILOSOPHY:
    // - Holds a reference to an object that will be registered later
    // - Throws exception if accessed before registration completes
    // - Similar to NeoForge's DeferredHolder/Supplier pattern
    //
    // LIFECYCLE:
    // 1. Created during static initialization (before main)
    // 2. Resolved during RegisterEvent handling
    // 3. Safe to access after GameData.FreezeData()
    //
    // USAGE:
    //   static auto MY_BLOCK = BLOCKS.Register("my_block", []() { return new Block(); });
    //   // Later, after registration:
    //   Block* block = MY_BLOCK->Get();
    //-----------------------------------------------------------------------------------------------
    template <typename T>
    class DeferredHolder
    {
    public:
        explicit DeferredHolder(const std::string& id)
            : m_id(id)
        {
        }

        ~DeferredHolder() = default;

        // Disable copy (holder identity matters)
        DeferredHolder(const DeferredHolder&)            = delete;
        DeferredHolder& operator=(const DeferredHolder&) = delete;

        // Move is allowed
        DeferredHolder(DeferredHolder&&) noexcept            = default;
        DeferredHolder& operator=(DeferredHolder&&) noexcept = default;

        //-------------------------------------------------------------------------------------------
        // Accessors
        //-------------------------------------------------------------------------------------------

        /// Get the held object (throws if not resolved)
        T* Get()
        {
            if (!m_resolved)
            {
                using namespace enigma::event;
                LogError(LogEvent, "DeferredHolder::Get '%s' accessed before resolution", m_id.c_str());
                throw HolderNotResolvedException(m_id);
            }
            return m_value;
        }

        const T* Get() const
        {
            if (!m_resolved)
            {
                using namespace enigma::event;
                LogError(LogEvent, "DeferredHolder::Get '%s' accessed before resolution", m_id.c_str());
                throw HolderNotResolvedException(m_id);
            }
            return m_value;
        }

        /// Pointer dereference operator
        T*       operator->() { return Get(); }
        const T* operator->() const { return Get(); }

        /// Reference dereference operator
        T&       operator*() { return *Get(); }
        const T& operator*() const { return *Get(); }

        //-------------------------------------------------------------------------------------------
        // Query Methods
        //-------------------------------------------------------------------------------------------

        /// Get the registration ID
        const std::string& GetId() const { return m_id; }

        /// Check if holder has been resolved
        bool IsResolved() const { return m_resolved; }

        /// Implicit bool conversion (true if resolved)
        explicit operator bool() const { return m_resolved; }

        //-------------------------------------------------------------------------------------------
        // Internal Methods (called by DeferredRegister)
        //-------------------------------------------------------------------------------------------

        /// Resolve the holder with the actual object pointer
        /// @note This should only be called by DeferredRegister during registration
        void Resolve(T* value)
        {
            using namespace enigma::event;
            if (m_resolved)
            {
                LogWarn(LogEvent, "DeferredHolder::Resolve '%s' already resolved, ignoring", m_id.c_str());
                return;
            }

            m_value    = value;
            m_resolved = true;
            LogDebug(LogEvent, "DeferredHolder::Resolve '%s' resolved successfully", m_id.c_str());
        }

    private:
        std::string m_id;
        T*          m_value    = nullptr;
        bool        m_resolved = false;
    };
} // namespace enigma::registry
