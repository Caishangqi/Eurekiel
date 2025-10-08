#pragma once
#include "Property.hpp"
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace enigma::voxel
{
    /**
     * @brief Direction enumeration for DirectionProperty
     */
    enum class Direction
    {
        NORTH = 0,
        SOUTH = 1,
        EAST = 2,
        WEST = 3,
        UP = 4,
        DOWN = 5
    };

    /**
     * @brief Convert Direction enum to string
     */
    std::string DirectionToString(Direction dir);

    /**
     * @brief Convert string to Direction enum
     */
    Direction StringToDirection(const std::string& str);

    /**
     * @brief Boolean property implementation
     */
    class BooleanProperty : public Property<bool>
    {
    public:
        explicit BooleanProperty(const std::string& name);

        BooleanProperty(const std::string& name, bool defaultValue);

        // IProperty interface
        std::string ValueToString(const std::any& value) const override;

        std::any StringToValue(const std::string& str) const override;

        std::vector<std::string> GetPossibleValuesAsStrings() const override;
    };

    /**
     * @brief Integer range property implementation
     */
    class IntProperty : public Property<int>
    {
    private:
        int m_min;
        int m_max;

    public:
        IntProperty(const std::string& name, int min, int max);

        IntProperty(const std::string& name, int min, int max, int defaultValue);

        int GetMin() const { return m_min; }
        int GetMax() const { return m_max; }

        // IProperty interface
        std::string ValueToString(const std::any& value) const override;

        std::any StringToValue(const std::string& str) const override;

        std::vector<std::string> GetPossibleValuesAsStrings() const override;

    private:
        static std::vector<int> GenerateRange(int min, int max)
        {
            std::vector<int> range;
            for (int i = min; i <= max; ++i)
            {
                range.push_back(i);
            }
            return range;
        }
    };

    /**
     * @brief Direction property implementation
     */
    class DirectionProperty : public Property<Direction>
    {
    public:
        explicit DirectionProperty(const std::string& name);

        DirectionProperty(const std::string& name, const std::vector<Direction>& allowedDirections);

        DirectionProperty(const std::string& name, const std::vector<Direction>& allowedDirections, Direction defaultValue);

        // IProperty interface
        std::string ValueToString(const std::any& value) const override;

        std::any StringToValue(const std::string& str) const override;

        std::vector<std::string> GetPossibleValuesAsStrings() const override;

        // Convenience methods
        static std::shared_ptr<DirectionProperty> CreateHorizontal(const std::string& name);

        static std::shared_ptr<DirectionProperty> CreateVertical(const std::string& name);
    };

    /**
     * @brief Template for enum properties
     */
    template <typename E>
    class EnumProperty : public Property<E>
    {
    private:
        std::function<std::string(E)>        m_enumToString;
        std::function<E(const std::string&)> m_stringToEnum;

    public:
        EnumProperty(const std::string&                   name,
                     const std::vector<E>&                possibleValues,
                     std::function<std::string(E)>        enumToString,
                     std::function<E(const std::string&)> stringToEnum)
            : Property<E>(name, possibleValues)
              , m_enumToString(std::move(enumToString))
              , m_stringToEnum(std::move(stringToEnum))
        {
        }

        EnumProperty(const std::string&                   name,
                     const std::vector<E>&                possibleValues,
                     const E&                             defaultValue,
                     std::function<std::string(E)>        enumToString,
                     std::function<E(const std::string&)> stringToEnum)
            : Property<E>(name, possibleValues, defaultValue)
              , m_enumToString(std::move(enumToString))
              , m_stringToEnum(std::move(stringToEnum))
        {
        }

        // IProperty interface
        std::string ValueToString(const std::any& value) const override
        {
            try
            {
                E enumValue = std::any_cast<E>(value);
                return m_enumToString(enumValue);
            }
            catch (const std::bad_any_cast&)
            {
                return m_enumToString(this->m_defaultValue);
            }
        }

        std::any StringToValue(const std::string& str) const override
        {
            return std::any(m_stringToEnum(str));
        }

        std::vector<std::string> GetPossibleValuesAsStrings() const override
        {
            std::vector<std::string> strings;
            for (const E& enumValue : this->m_possibleValues)
            {
                strings.push_back(m_enumToString(enumValue));
            }
            return strings;
        }
    };
}
