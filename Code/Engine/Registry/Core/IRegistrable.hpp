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
     * Supports both string-based registration keys and numeric IDs for performance.
     */
    class IRegistrable
    {
    private:
        int m_numericId = -1; // Registry-assigned numeric ID (-1 = unassigned)

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

        // Numeric ID System - Added for performance optimization

        /**
         * @brief Get the numeric ID assigned by the registry
         * @return Numeric ID, or -1 if not assigned
         */
        int GetNumericId() const { return m_numericId; }

        /**
         * @brief Check if this object has a valid numeric ID
         * @return True if ID is assigned (>= 0)
         */
        bool HasValidId() const { return m_numericId >= 0; }

        /**
         * @brief Get the full registry key (namespace:name)
         * @return Combined registry key string
         */
        std::string GetRegistryKey() const
        {
            const std::string& ns = GetNamespace();
            if (ns.empty())
            {
                return GetRegistryName();
            }
            return ns + ":" + GetRegistryName();
        }

    protected:
        /**
         * @brief Set the numeric ID (called by Registry during registration)
         * @param id The numeric ID assigned by the registry
         * @note This should only be called by Registry implementations
         */
        void SetNumericId(int id) { m_numericId = id; }

        // Allow Registry template to access SetNumericId
        template <typename T>
        friend class Registry;
    };
}
