#pragma once
#include <string>
#include <vector>
#include <typeinfo>
#include <memory>
#include <any>
#include <unordered_map>
#include <algorithm>
#include <functional>

namespace enigma::voxel::property
{
    /**
     * @brief Base interface for all property types
     * 
     * Properties define the possible states that blocks can have.
     * Similar to Minecraft's Property system.
     */
    class IProperty
    {
    public:
        virtual ~IProperty() = default;

        /**
         * @brief Get the property name
         */
        virtual const std::string& GetName() const = 0;

        /**
         * @brief Get the type info for this property's value type
         */
        virtual const std::type_info& GetValueType() const = 0;

        /**
         * @brief Convert a value to its string representation
         */
        virtual std::string ValueToString(const std::any& value) const = 0;

        /**
         * @brief Parse a string value into the property's type
         */
        virtual std::any StringToValue(const std::string& str) const = 0;

        /**
         * @brief Check if a value is valid for this property
         */
        virtual bool IsValidValue(const std::any& value) const = 0;

        /**
         * @brief Get all possible values as strings
         */
        virtual std::vector<std::string> GetPossibleValuesAsStrings() const = 0;

        /**
         * @brief Get the default value for this property
         */
        virtual std::any GetDefaultValue() const = 0;

        /**
         * @brief Get a hash for a given value (for fast state lookup)
         */
        virtual size_t GetValueHash(const std::any& value) const = 0;
    };

    /**
     * @brief Template base class for typed properties
     */
    template <typename T>
    class Property : public IProperty
    {
    public:
        using ValueType = T;

    protected:
        std::string    m_name;
        std::vector<T> m_possibleValues;
        T              m_defaultValue;

    public:
        explicit Property(const std::string& name, std::vector<T> possibleValues)
            : m_name(name), m_possibleValues(std::move(possibleValues))
        {
            if (!m_possibleValues.empty())
            {
                m_defaultValue = m_possibleValues[0];
            }
        }

        Property(const std::string& name, std::vector<T> possibleValues, const T& defaultValue)
            : m_name(name), m_possibleValues(std::move(possibleValues)), m_defaultValue(defaultValue)
        {
        }

        // IProperty interface
        const std::string&    GetName() const override { return m_name; }
        const std::type_info& GetValueType() const override { return typeid(T); }

        std::any GetDefaultValue() const override { return std::any(m_defaultValue); }

        bool IsValidValue(const std::any& value) const override
        {
            try
            {
                const T& typedValue = std::any_cast<const T&>(value);
                return IsValidValue(typedValue);
            }
            catch (const std::bad_any_cast&)
            {
                return false;
            }
        }

        size_t GetValueHash(const std::any& value) const override
        {
            try
            {
                const T& typedValue = std::any_cast<const T&>(value);
                return std::hash<T>{}(typedValue);
            }
            catch (const std::bad_any_cast&)
            {
                return 0;
            }
        }

        // Type-safe methods
        bool IsValidValue(const T& value) const
        {
            return std::find(m_possibleValues.begin(), m_possibleValues.end(), value) != m_possibleValues.end();
        }

        const std::vector<T>& GetPossibleValues() const { return m_possibleValues; }
        const T&              GetDefaultValueTyped() const { return m_defaultValue; }

        // Convenience methods for block state generation
        size_t   GetValueCount() const { return m_possibleValues.size(); }
        const T& GetValueAt(size_t index) const { return m_possibleValues[index]; }
    };
}
