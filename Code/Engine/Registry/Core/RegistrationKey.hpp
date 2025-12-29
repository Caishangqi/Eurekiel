#pragma once
#include <string>
#include <functional>

namespace enigma::core
{
    /**
     * @struct RegistrationKey
     * @brief Represents a unique key for registry entries
     * 
     * Similar to Minecraft's ResourceLocation but for registry keys.
     * Combines namespace and name to create unique identifiers.
     */
    struct RegistrationKey
    {
        std::string namespaceName;
        std::string name;

        RegistrationKey() = default;

        RegistrationKey(const std::string& ns, const std::string& n)
            : namespaceName(ns), name(n)
        {
        }

        RegistrationKey(const std::string& n)
        {
            // Check whether the namespace delimiter is included ':'
            size_t colonPos = n.find(':');
            if (colonPos != std::string::npos)
            {
                // Split namespace:name format
                namespaceName = n.substr(0, colonPos);
                name          = n.substr(colonPos + 1);
            }
            else
            {
                // No namespace, use enigma namespace
                namespaceName = "simpleminer";
                name          = n;
            }
        }

        bool operator==(const RegistrationKey& other) const
        {
            return namespaceName == other.namespaceName && name == other.name;
        }

        bool operator!=(const RegistrationKey& other) const
        {
            return !(*this == other);
        }

        bool operator<(const RegistrationKey& other) const
        {
            if (namespaceName != other.namespaceName)
                return namespaceName < other.namespaceName;
            return name < other.name;
        }

        std::string ToString() const
        {
            return namespaceName.empty() ? name : namespaceName + ":" + name;
        }

        bool IsValid() const
        {
            return !name.empty();
        }
    };
}

// Hash specialization for std::unordered_map
namespace std
{
    template <>
    struct hash<enigma::core::RegistrationKey>
    {
        size_t operator()(const enigma::core::RegistrationKey& key) const
        {
            return hash<string>()(key.namespaceName) ^ (hash<string>()(key.name) << 1);
        }
    };
}
