#pragma once
#include <string>

namespace enigma::core
{
    /**
     * @interface IRegistrable
     * @brief Interface for objects that can be registered in the RegisterSubsystem
     * 
     * Similar to Minecraft Neoforge's registrable objects, this provides a common
     * interface for any object that needs to be registered in a registry.
     */
    class IRegistrable
    {
    public:
        virtual ~IRegistrable() = default;

        /**
         * @brief Get the registry name for this object
         * @return The unique name used for registration
         */
        virtual const std::string& GetRegistryName() const = 0;

        /**
         * @brief Get the namespace for this object (optional)
         * @return The namespace string, empty if no namespace
         */
        virtual const std::string& GetNamespace() const
        {
            static std::string empty;
            return empty;
        }
    };
}
