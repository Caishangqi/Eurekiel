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
     * Provides type-erased operations for registry management
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
    };
}