#pragma once
#include "RegistrationKey.hpp"
#include <vector>
#include <string>

namespace enigma::core
{
    /**
     * @interface IRegistry
     * @brief Base interface for all registry implementations
     *
     * Provides type-erased operations for registry management.
     * Supports both string-based keys (for compatibility) and numeric IDs (for performance).
     */
    class IRegistry
    {
    public:
        virtual ~IRegistry() = default;

        /**
         * @brief Get the type name of objects stored in this registry
         */
        virtual const std::string& GetRegistryType() const = 0;

        /**
         * @brief Get the number of registered objects
         */
        virtual size_t GetRegistrationCount() const = 0;

        /**
         * @brief Clear all registered objects
         */
        virtual void Clear() = 0;

        /**
         * @brief Get all registration keys
         */
        virtual std::vector<RegistrationKey> GetAllKeys() const = 0;

        /**
         * @brief Check if a key is registered
         */
        virtual bool HasRegistration(const RegistrationKey& key) const = 0;

        // Numeric ID System - Added for performance optimization

        /**
         * @brief Get the next available numeric ID
         */
        virtual int GetNextAvailableId() const = 0;

        /**
         * @brief Check if a numeric ID is currently in use
         */
        virtual bool HasId(int id) const = 0;

        /**
         * @brief Get the numeric ID for a registration key
         * @return Numeric ID, or -1 if key not found
         */
        virtual int GetId(const RegistrationKey& key) const = 0;

        /**
         * @brief Get the registration key for a numeric ID
         * @return Registration key, or invalid key if ID not found
         */
        virtual RegistrationKey GetKey(int id) const = 0;

        /**
         * @brief Set the maximum allowed ID for this registry
         */
        virtual void SetMaxId(int maxId) = 0;

        /**
         * @brief Get the maximum allowed ID for this registry
         */
        virtual int GetMaxId() const = 0;

        /**
         * @brief Get all valid numeric IDs in this registry
         */
        virtual std::vector<int> GetAllIds() const = 0;

    protected:
        /**
         * @brief Allocate a new numeric ID (internal use)
         */
        virtual int AllocateId() = 0;

        /**
         * @brief Release a numeric ID for reuse (internal use)
         */
        virtual void ReleaseId(int id) = 0;
    };
}
