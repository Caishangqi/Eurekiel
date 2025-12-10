#pragma once
#include <cstdint>
#include <string>

namespace enigma::voxel
{
    /**
     * @brief Half type enumeration for blocks with vertical halves (stairs, slabs)
     *
     * Defines the two possible vertical halves:
     * - BOTTOM: Lower half of the block (Y: 0.0 - 0.5)
     * - TOP: Upper half of the block (Y: 0.5 - 1.0)
     *
     * Used by:
     * - SlabBlock: BOTTOM/TOP determine slab placement (DOUBLE uses SlabType instead)
     * - StairsBlock: BOTTOM/TOP determine stairs placement height
     */
    enum class HalfType : uint8_t
    {
        BOTTOM = 0,
        TOP = 1
    };

    /**
     * @brief Convert HalfType enum to string representation
     * @param type The HalfType value to convert
     * @return String representation ("bottom" or "top")
     */
    inline const char* HalfTypeToString(HalfType type)
    {
        switch (type)
        {
        case HalfType::BOTTOM: return "bottom";
        case HalfType::TOP: return "top";
        default: return "bottom"; // [DEFAULT] Fallback to bottom
        }
    }

    /**
     * @brief Convert string to HalfType enum
     * @param str String representation ("bottom" or "top")
     * @return Corresponding HalfType value
     */
    inline HalfType StringToHalfType(const std::string& str)
    {
        if (str == "bottom") return HalfType::BOTTOM;
        if (str == "top") return HalfType::TOP;

        // [WARNING] Invalid string, default to BOTTOM
        return HalfType::BOTTOM;
    }

    /**
     * @brief Check if half type is the bottom half
     * @param type The HalfType to check
     * @return True if type is BOTTOM
     */
    inline bool IsBottomHalf(HalfType type)
    {
        return type == HalfType::BOTTOM;
    }

    /**
     * @brief Check if half type is the top half
     * @param type The HalfType to check
     * @return True if type is TOP
     */
    inline bool IsTopHalf(HalfType type)
    {
        return type == HalfType::TOP;
    }

    /**
     * @brief Get the opposite half type
     * @param type The HalfType to flip
     * @return TOP if type is BOTTOM, BOTTOM if type is TOP
     */
    inline HalfType GetOppositeHalf(HalfType type)
    {
        return (type == HalfType::BOTTOM) ? HalfType::TOP : HalfType::BOTTOM;
    }
}
