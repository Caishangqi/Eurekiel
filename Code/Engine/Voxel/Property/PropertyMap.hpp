#pragma once
#include "Property.hpp"
#include <unordered_map>
#include <memory>
#include <functional>

namespace enigma::voxel
{
    /**
     * @brief Container for property-value pairs with fast hash-based lookup
     * 
     * Used by BlockState to store the current values of all properties.
     * Optimized for fast comparison and hashing.
     */
    class PropertyMap
    {
    private:
        std::unordered_map<std::shared_ptr<IProperty>, std::any> m_values;
        mutable size_t                                           m_cachedHash = 0;
        mutable bool                                             m_hashValid  = false;

    public:
        PropertyMap() = default;

        /**
         * @brief Set a property value using std::any (less type-safe but more flexible)
         */
        void SetAny(std::shared_ptr<IProperty> property, const std::any& value)
        {
            if (property && property->IsValidValue(value))
            {
                m_values[property] = value;
                m_hashValid        = false;
            }
        }

        /**
         * @brief Set a property value (type-safe)
         */
        template <typename T>
        void Set(std::shared_ptr<Property<T>> property, const T& value)
        {
            if (property && property->IsValidValue(value))
            {
                m_values[property] = std::any(value);
                m_hashValid        = false;
            }
        }

        /**
         * @brief Get a property value (type-safe)
         */
        template <typename T>
        T Get(std::shared_ptr<Property<T>> property) const
        {
            auto it = m_values.find(property);
            if (it != m_values.end())
            {
                try
                {
                    return std::any_cast<T>(it->second);
                }
                catch (const std::bad_any_cast&)
                {
                    // Fall through to default
                }
            }

            // Return default value if not found or cast failed
            return property ? property->GetDefaultValueTyped() : T{};
        }

        /**
         * @brief Get a property value as std::any (less type-safe but more flexible)
         */
        std::any GetAny(std::shared_ptr<IProperty> property) const
        {
            auto it = m_values.find(property);
            if (it != m_values.end())
            {
                return it->second;
            }
            return std::any{};
        }

        /**
         * @brief Create a new PropertyMap with one value changed
         */
        template <typename T>
        PropertyMap With(std::shared_ptr<Property<T>> property, const T& value) const
        {
            PropertyMap newMap = *this;
            newMap.Set(property, value);
            return newMap;
        }

        /**
         * @brief Check if this map contains a property
         */
        bool HasProperty(std::shared_ptr<IProperty> property) const
        {
            return m_values.find(property) != m_values.end();
        }

        /**
         * @brief Get all properties in this map
         */
        std::vector<std::shared_ptr<IProperty>> GetProperties() const
        {
            std::vector<std::shared_ptr<IProperty>> properties;
            properties.reserve(m_values.size());
            for (const auto& pair : m_values)
            {
                properties.push_back(pair.first);
            }
            return properties;
        }

        /**
         * @brief Get hash for fast BlockState comparison
         */
        size_t GetHash() const
        {
            if (!m_hashValid)
            {
                m_cachedHash = ComputeHash();
                m_hashValid  = true;
            }
            return m_cachedHash;
        }

        /**
         * @brief Check equality with another PropertyMap
         */
        bool operator==(const PropertyMap& other) const
        {
            if (m_values.size() != other.m_values.size())
                return false;

            for (const auto& pair : m_values)
            {
                auto it = other.m_values.find(pair.first);
                if (it == other.m_values.end())
                    return false;

                // Compare values using property's hash
                if (pair.first->GetValueHash(pair.second) != pair.first->GetValueHash(it->second))
                    return false;
            }
            return true;
        }

        bool operator!=(const PropertyMap& other) const
        {
            return !(*this == other);
        }

        /**
         * @brief Convert to string representation (for debugging)
         */
        std::string ToString() const
        {
            if (m_values.empty())
                return "{}";

            std::string result = "{";
            bool        first  = true;
            for (const auto& pair : m_values)
            {
                if (!first) result += ",";
                first = false;

                result += pair.first->GetName() + "=" + pair.first->ValueToString(pair.second);
            }
            result += "}";
            return result;
        }

        /**
         * @brief Get number of properties
         */
        size_t Size() const { return m_values.size(); }

        /**
         * @brief Check if empty
         */
        bool Empty() const { return m_values.empty(); }

        /**
         * @brief Clear all properties
         */
        void Clear()
        {
            m_values.clear();
            m_hashValid = false;
        }

    private:
        size_t ComputeHash() const
        {
            size_t hash = 0;
            for (const auto& pair : m_values)
            {
                // Combine property pointer hash with value hash
                size_t propertyHash = std::hash<IProperty*>{}(pair.first.get());
                size_t valueHash    = pair.first->GetValueHash(pair.second);
                hash ^= propertyHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                hash ^= valueHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };
}

// Hash specialization for PropertyMap
namespace std
{
    template <>
    struct hash<enigma::voxel::PropertyMap>
    {
        size_t operator()(const enigma::voxel::PropertyMap& map) const
        {
            return map.GetHash();
        }
    };
}
