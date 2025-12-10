#pragma once
#include <cstdint>
#include <string>

namespace enigma::voxel
{
    /**
     * @brief Stairs shape enumeration for StairsBlock
     *
     * Defines the five possible shapes of stairs based on adjacent stair blocks:
     * - STRAIGHT: Basic stair shape, no adjacent stairs or perpendicular alignment
     * - INNER_LEFT: Inner corner formed by perpendicular stairs (left turn)
     * - INNER_RIGHT: Inner corner formed by perpendicular stairs (right turn)
     * - OUTER_LEFT: Outer corner formed by back neighbor (left turn)
     * - OUTER_RIGHT: Outer corner formed by back neighbor (right turn)
     *
     * Shape is auto-calculated based on neighboring stairs with matching half property.
     */
    enum class StairsShape : uint8_t
    {
        STRAIGHT = 0,
        INNER_LEFT = 1,
        INNER_RIGHT = 2,
        OUTER_LEFT = 3,
        OUTER_RIGHT = 4
    };

    /**
     * @brief Convert StairsShape enum to string representation
     * @param shape The StairsShape value to convert
     * @return String representation ("straight", "inner_left", etc.)
     */
    inline const char* StairsShapeToString(StairsShape shape)
    {
        switch (shape)
        {
        case StairsShape::STRAIGHT: return "straight";
        case StairsShape::INNER_LEFT: return "inner_left";
        case StairsShape::INNER_RIGHT: return "inner_right";
        case StairsShape::OUTER_LEFT: return "outer_left";
        case StairsShape::OUTER_RIGHT: return "outer_right";
        default: return "straight"; // [DEFAULT] Fallback
        }
    }

    /**
     * @brief Convert string to StairsShape enum
     * @param str String representation ("straight", "inner_left", etc.)
     * @return Corresponding StairsShape value
     */
    inline StairsShape StringToStairsShape(const std::string& str)
    {
        if (str == "straight") return StairsShape::STRAIGHT;
        if (str == "inner_left") return StairsShape::INNER_LEFT;
        if (str == "inner_right") return StairsShape::INNER_RIGHT;
        if (str == "outer_left") return StairsShape::OUTER_LEFT;
        if (str == "outer_right") return StairsShape::OUTER_RIGHT;

        // [WARNING] Invalid string, default to STRAIGHT
        return StairsShape::STRAIGHT;
    }

    /**
     * @brief Get model suffix for stairs shape
     * @param shape The StairsShape to query
     * @return Model file suffix ("stairs", "inner_stairs", or "outer_stairs")
     *
     * Maps shape enum to model parent names:
     * - STRAIGHT: "stairs" (block/stairs.json)
     * - INNER_LEFT/RIGHT: "inner_stairs" (block/inner_stairs.json)
     * - OUTER_LEFT/RIGHT: "outer_stairs" (block/outer_stairs.json)
     */
    inline const char* GetModelSuffix(StairsShape shape)
    {
        switch (shape)
        {
        case StairsShape::STRAIGHT:
            return "stairs";

        case StairsShape::INNER_LEFT:
        case StairsShape::INNER_RIGHT:
            return "inner_stairs";

        case StairsShape::OUTER_LEFT:
        case StairsShape::OUTER_RIGHT:
            return "outer_stairs";

        default:
            return "stairs"; // [DEFAULT] Fallback to straight stairs
        }
    }

    /**
     * @brief Check if shape is an inner corner
     * @param shape The StairsShape to check
     * @return True if shape is INNER_LEFT or INNER_RIGHT
     */
    inline bool IsInnerCorner(StairsShape shape)
    {
        return shape == StairsShape::INNER_LEFT || shape == StairsShape::INNER_RIGHT;
    }

    /**
     * @brief Check if shape is an outer corner
     * @param shape The StairsShape to check
     * @return True if shape is OUTER_LEFT or OUTER_RIGHT
     */
    inline bool IsOuterCorner(StairsShape shape)
    {
        return shape == StairsShape::OUTER_LEFT || shape == StairsShape::OUTER_RIGHT;
    }

    /**
     * @brief Check if shape is a corner (inner or outer)
     * @param shape The StairsShape to check
     * @return True if shape is any corner type
     */
    inline bool IsCorner(StairsShape shape)
    {
        return IsInnerCorner(shape) || IsOuterCorner(shape);
    }
}
