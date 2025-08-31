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

namespace enigma::voxel::property
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
    inline std::string DirectionToString(Direction dir)
    {
        switch (dir)
        {
            case Direction::NORTH: return "north";
            case Direction::SOUTH: return "south";
            case Direction::EAST: return "east";
            case Direction::WEST: return "west";
            case Direction::UP: return "up";
            case Direction::DOWN: return "down";
            default: return "north";
        }
    }
    
    /**
     * @brief Convert string to Direction enum
     */
    inline Direction StringToDirection(const std::string& str)
    {
        if (str == "south") return Direction::SOUTH;
        if (str == "east") return Direction::EAST;
        if (str == "west") return Direction::WEST;
        if (str == "up") return Direction::UP;
        if (str == "down") return Direction::DOWN;
        return Direction::NORTH; // default
    }
    
    /**
     * @brief Boolean property implementation
     */
    class BooleanProperty : public Property<bool>
    {
    public:
        explicit BooleanProperty(const std::string& name)
            : Property<bool>(name, {false, true}, false)
        {
        }
        
        BooleanProperty(const std::string& name, bool defaultValue)
            : Property<bool>(name, {false, true}, defaultValue)
        {
        }
        
        // IProperty interface
        std::string ValueToString(const std::any& value) const override
        {
            try
            {
                bool boolValue = std::any_cast<bool>(value);
                return boolValue ? "true" : "false";
            }
            catch (const std::bad_any_cast&)
            {
                return "false";
            }
        }
        
        std::any StringToValue(const std::string& str) const override
        {
            return std::any(str == "true" || str == "1" || str == "yes");
        }
        
        std::vector<std::string> GetPossibleValuesAsStrings() const override
        {
            return {"false", "true"};
        }
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
        IntProperty(const std::string& name, int min, int max)
            : Property<int>(name, GenerateRange(min, max), min), m_min(min), m_max(max)
        {
        }
        
        IntProperty(const std::string& name, int min, int max, int defaultValue)
            : Property<int>(name, GenerateRange(min, max), defaultValue), m_min(min), m_max(max)
        {
        }
        
        int GetMin() const { return m_min; }
        int GetMax() const { return m_max; }
        
        // IProperty interface
        std::string ValueToString(const std::any& value) const override
        {
            try
            {
                int intValue = std::any_cast<int>(value);
                return std::to_string(intValue);
            }
            catch (const std::bad_any_cast&)
            {
                return std::to_string(m_min);
            }
        }
        
        std::any StringToValue(const std::string& str) const override
        {
            try
            {
                int value = std::stoi(str);
                // Clamp to valid range
                value = std::max(m_min, std::min(m_max, value));
                return std::any(value);
            }
            catch (const std::exception&)
            {
                return std::any(m_min);
            }
        }
        
        std::vector<std::string> GetPossibleValuesAsStrings() const override
        {
            std::vector<std::string> strings;
            for (int i = m_min; i <= m_max; ++i)
            {
                strings.push_back(std::to_string(i));
            }
            return strings;
        }
        
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
        explicit DirectionProperty(const std::string& name)
            : Property<Direction>(name, 
                {Direction::NORTH, Direction::SOUTH, Direction::EAST, Direction::WEST, Direction::UP, Direction::DOWN},
                Direction::NORTH)
        {
        }
        
        DirectionProperty(const std::string& name, const std::vector<Direction>& allowedDirections)
            : Property<Direction>(name, allowedDirections, 
                allowedDirections.empty() ? Direction::NORTH : allowedDirections[0])
        {
        }
        
        DirectionProperty(const std::string& name, const std::vector<Direction>& allowedDirections, Direction defaultValue)
            : Property<Direction>(name, allowedDirections, defaultValue)
        {
        }
        
        // IProperty interface
        std::string ValueToString(const std::any& value) const override
        {
            try
            {
                Direction dirValue = std::any_cast<Direction>(value);
                return DirectionToString(dirValue);
            }
            catch (const std::bad_any_cast&)
            {
                return "north";
            }
        }
        
        std::any StringToValue(const std::string& str) const override
        {
            return std::any(StringToDirection(str));
        }
        
        std::vector<std::string> GetPossibleValuesAsStrings() const override
        {
            std::vector<std::string> strings;
            for (const Direction& dir : m_possibleValues)
            {
                strings.push_back(DirectionToString(dir));
            }
            return strings;
        }
        
        // Convenience methods
        static std::shared_ptr<DirectionProperty> CreateHorizontal(const std::string& name)
        {
            return std::make_shared<DirectionProperty>(name, 
                std::vector<Direction>{Direction::NORTH, Direction::SOUTH, Direction::EAST, Direction::WEST});
        }
        
        static std::shared_ptr<DirectionProperty> CreateVertical(const std::string& name)
        {
            return std::make_shared<DirectionProperty>(name, 
                std::vector<Direction>{Direction::UP, Direction::DOWN});
        }
    };
    
    /**
     * @brief Template for enum properties
     */
    template<typename E>
    class EnumProperty : public Property<E>
    {
    private:
        std::function<std::string(E)> m_enumToString;
        std::function<E(const std::string&)> m_stringToEnum;
        
    public:
        EnumProperty(const std::string& name, 
                    const std::vector<E>& possibleValues,
                    std::function<std::string(E)> enumToString,
                    std::function<E(const std::string&)> stringToEnum)
            : Property<E>(name, possibleValues)
            , m_enumToString(std::move(enumToString))
            , m_stringToEnum(std::move(stringToEnum))
        {
        }
        
        EnumProperty(const std::string& name, 
                    const std::vector<E>& possibleValues,
                    const E& defaultValue,
                    std::function<std::string(E)> enumToString,
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
