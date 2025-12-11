#include "PropertyTypes.hpp"

#include "Engine/Core/EngineCommon.hpp"

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

// ============================================================================
// [WARNING] DO NOT MODIFY THIS FUNCTION WITHOUT UNDERSTANDING THE FOLLOWING:
// ============================================================================
//
// This function rotates a Direction enum to match the vertex rotation applied
// in RenderMesh::ApplyBlockRotation(). Three critical aspects must be correct:
//
// 1. ROTATION ORDER: Must match matrix multiplication order (X first, then Y)
// 2. ROTATION DIRECTION: Must match matrix rotation direction (counter-clockwise)
// 3. ANGLE NORMALIZATION: Always normalize to [0, 360) before computing steps
//
// If any of these are wrong, face culling will be incorrect for rotated blocks
// (e.g., stairs with half=top, or any block with x/y rotation in blockstate).
//
// See RenderMesh.cpp::ApplyBlockRotation() for the vertex rotation implementation.
// ============================================================================
Direction enigma::voxel::RotateDirection(Direction dir, int rotX, int rotY)
{
    // Normalize rotations to 0, 90, 180, 270
    // [IMPORTANT] Always normalize BEFORE computing steps to handle negative angles correctly
    // Example: -90 → 270 (not -1 step, but 3 steps in CCW direction)
    rotX = ((rotX % 360) + 360) % 360;
    rotY = ((rotY % 360) + 360) % 360;

    // Coordinate system mapping:
    // SimpleMiner: +X=Forward, +Y=Left, +Z=Up
    // Minecraft:   +X=East,    +Y=Up,   +Z=South
    //
    // Direction mapping (SimpleMiner perspective):
    // NORTH = +Y (left in SimpleMiner)
    // SOUTH = -Y (right in SimpleMiner)
    // EAST  = +X (forward in SimpleMiner)
    // WEST  = -X (backward in SimpleMiner)
    // UP    = +Z (up in SimpleMiner)
    // DOWN  = -Z (down in SimpleMiner)

    Direction result = dir;

    // ============================================================================
    // [CRITICAL] ROTATION ORDER: Must match matrix multiplication order!
    // ============================================================================
    //
    // In RenderMesh.cpp, the matrix is built as:
    //   rotationMatrix.AppendZRotation(-rotY)  // M = Rz(-rotY)
    //   rotationMatrix.AppendXRotation(-rotX)  // M = Rz(-rotY) * Rx(-rotX)
    //
    // When transforming a vector: v' = M * v = Rz * Rx * v
    // Matrix multiplication applies RIGHT-TO-LEFT, so:
    //   1. First, X rotation is applied to v
    //   2. Then, Z(Y) rotation is applied to the result
    //
    // Therefore, RotateDirection must apply rotations in the SAME order:
    //   1. First apply X rotation
    //   2. Then apply Y(Z) rotation
    //
    // [WARNING] DO NOT change this order! It will break culling for half=top stairs.
    // ============================================================================

    // STEP 1: Apply X rotation FIRST (around X axis in SimpleMiner)
    // This rotates: NORTH<->UP<->SOUTH<->DOWN cycle, EAST/WEST unchanged
    //
    // [CRITICAL] ROTATION DIRECTION: Must be counter-clockwise (CCW) to match matrix
    // MakeXRotationDegrees uses right-hand rule: positive angle rotates Y → Z
    // Rotation cycle (CCW): NORTH(+Y) → UP(+Z) → SOUTH(-Y) → DOWN(-Z) → NORTH
    //
    // [WARNING] DO NOT reverse this direction! The old buggy code had:
    //   NORTH → DOWN → SOUTH → UP (clockwise - WRONG!)
    // Correct direction is:
    //   NORTH → UP → SOUTH → DOWN (counter-clockwise - CORRECT!)
    int xSteps = rotX / 90;
    for (int i = 0; i < xSteps; ++i)
    {
        // Counter-clockwise rotation around X-axis (matches AppendXRotation positive direction)
        // NORTH(+Y) → UP(+Z) → SOUTH(-Y) → DOWN(-Z) → NORTH
        switch (result)
        {
        case Direction::NORTH: result = Direction::UP; // +Y → +Z
            break;
        case Direction::UP: result = Direction::SOUTH; // +Z → -Y
            break;
        case Direction::SOUTH: result = Direction::DOWN; // -Y → -Z
            break;
        case Direction::DOWN: result = Direction::NORTH; // -Z → +Y
            break;
        case Direction::EAST: break; // Unchanged
        case Direction::WEST: break; // Unchanged
        }
    }

    // STEP 2: Apply Y rotation SECOND (around vertical/Z axis in SimpleMiner)
    // This rotates horizontal directions: NORTH, SOUTH, EAST, WEST
    //
    // [CRITICAL] ROTATION DIRECTION: Must be counter-clockwise (CCW) to match matrix
    // MakeZRotationDegrees uses right-hand rule: positive angle rotates X → Y
    // Rotation cycle (CCW): EAST(+X) → NORTH(+Y) → WEST(-X) → SOUTH(-Y) → EAST
    int ySteps = rotY / 90;
    for (int i = 0; i < ySteps; ++i)
    {
        // Counter-clockwise rotation (matches AppendZRotation positive direction)
        switch (result)
        {
        case Direction::EAST: result = Direction::NORTH; // +X → +Y
            break;
        case Direction::NORTH: result = Direction::WEST; // +Y → -X
            break;
        case Direction::WEST: result = Direction::SOUTH; // -X → -Y
            break;
        case Direction::SOUTH: result = Direction::EAST; // -Y → +X
            break;
        case Direction::UP: break; // Unchanged
        case Direction::DOWN: break; // Unchanged
        }
    }

    return result;
}
