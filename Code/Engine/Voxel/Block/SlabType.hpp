#pragma once
#include <cstdint>
#include <string>

namespace enigma::voxel
{
    /**
     * @brief Slab type enumeration for SlabBlock
     *
     * Defines the three possible states of a slab:
     * - BOTTOM: Lower half of the block (Y: 0.0 - 0.5)
     * - TOP: Upper half of the block (Y: 0.5 - 1.0)
     * - DOUBLE: Full block (merged slab, Y: 0.0 - 1.0)
     */
    enum class SlabType : uint8_t
    {
        BOTTOM = 0,
        TOP = 1,
        DOUBLE = 2
    };

    /**
     * @brief Convert SlabType enum to string representation
     * @param type The SlabType value to convert
     * @return String representation ("bottom", "top", or "double")
     */
    inline const char* SlabTypeToString(SlabType type)
    {
        switch (type)
        {
        case SlabType::BOTTOM: return "bottom";
        case SlabType::TOP: return "top";
        case SlabType::DOUBLE: return "double";
        default: return "bottom"; // [DEFAULT] Fallback to bottom
        }
    }

    /**
     * @brief Convert string to SlabType enum
     * @param str String representation ("bottom", "top", or "double")
     * @return Corresponding SlabType value
     * @throws std::invalid_argument if string is not recognized
     */
    inline SlabType StringToSlabType(const std::string& str)
    {
        if (str == "bottom") return SlabType::BOTTOM;
        if (str == "top") return SlabType::TOP;
        if (str == "double") return SlabType::DOUBLE;

        // [WARNING] Invalid string, default to BOTTOM
        return SlabType::BOTTOM;
    }

    /**
     * @brief Check if slab type represents a full block
     * @param type The SlabType to check
     * @return True if type is DOUBLE (full block), false otherwise
     */
    inline bool IsFullBlock(SlabType type)
    {
        return type == SlabType::DOUBLE;
    }

    /**
     * @brief Check if slab type is opaque (blocks light)
     * @param type The SlabType to check
     * @return True if type is DOUBLE, false for BOTTOM or TOP
     *
     * Only double slabs (full blocks) are opaque and block light completely.
     * Bottom and top slabs allow light to pass through their empty half.
     */
    inline bool IsSlabOpaque(SlabType type)
    {
        return type == SlabType::DOUBLE;
    }
}
