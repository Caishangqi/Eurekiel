#include "PropertyTypes.hpp"

using namespace enigma::voxel;

std::string enigma::voxel::DirectionToString(Direction dir)
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

Direction enigma::voxel::StringToDirection(const std::string& str)
{
    if (str == "south") return Direction::SOUTH;
    if (str == "east") return Direction::EAST;
    if (str == "west") return Direction::WEST;
    if (str == "up") return Direction::UP;
    if (str == "down") return Direction::DOWN;
    return Direction::NORTH; // default
}

BooleanProperty::BooleanProperty(const std::string& name) : Property<bool>(name, {false, true}, false)
{
}

BooleanProperty::BooleanProperty(const std::string& name, bool defaultValue) : Property<bool>(name, {false, true}, defaultValue)
{
}

std::string BooleanProperty::ValueToString(const std::any& value) const
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

std::any BooleanProperty::StringToValue(const std::string& str) const
{
    return std::any(str == "true" || str == "1" || str == "yes");
}

std::vector<std::string> BooleanProperty::GetPossibleValuesAsStrings() const
{
    return {"false", "true"};
}

IntProperty::IntProperty(const std::string& name, int min, int max)
    : Property<int>(name, GenerateRange(min, max), min), m_min(min), m_max(max)
{
}

IntProperty::IntProperty(const std::string& name, int min, int max, int defaultValue)
    : Property<int>(name, GenerateRange(min, max), defaultValue), m_min(min), m_max(max)
{
}

std::string IntProperty::ValueToString(const std::any& value) const
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

std::any IntProperty::StringToValue(const std::string& str) const
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

std::vector<std::string> IntProperty::GetPossibleValuesAsStrings() const
{
    std::vector<std::string> strings;
    for (int i = m_min; i <= m_max; ++i)
    {
        strings.push_back(std::to_string(i));
    }
    return strings;
}

DirectionProperty::DirectionProperty(const std::string& name)
    : Property<Direction>(name,
                          {Direction::NORTH, Direction::SOUTH, Direction::EAST, Direction::WEST, Direction::UP, Direction::DOWN},
                          Direction::NORTH)
{
}

DirectionProperty::DirectionProperty(const std::string& name, const std::vector<Direction>& allowedDirections)
    : Property<Direction>(name, allowedDirections,
                          allowedDirections.empty() ? Direction::NORTH : allowedDirections[0])
{
}

DirectionProperty::DirectionProperty(const std::string& name, const std::vector<Direction>& allowedDirections, Direction defaultValue)
    : Property<Direction>(name, allowedDirections, defaultValue)
{
}

std::string DirectionProperty::ValueToString(const std::any& value) const
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

std::any DirectionProperty::StringToValue(const std::string& str) const
{
    return std::any(StringToDirection(str));
}

std::vector<std::string> DirectionProperty::GetPossibleValuesAsStrings() const
{
    std::vector<std::string> strings;
    for (const Direction& dir : m_possibleValues)
    {
        strings.push_back(DirectionToString(dir));
    }
    return strings;
}

std::shared_ptr<DirectionProperty> DirectionProperty::CreateHorizontal(const std::string& name)
{
    return std::make_shared<DirectionProperty>(name,
                                               std::vector<Direction>{Direction::NORTH, Direction::SOUTH, Direction::EAST, Direction::WEST});
}

std::shared_ptr<DirectionProperty> DirectionProperty::CreateVertical(const std::string& name)
{
    return std::make_shared<DirectionProperty>(name,
                                               std::vector<Direction>{Direction::UP, Direction::DOWN});
}
