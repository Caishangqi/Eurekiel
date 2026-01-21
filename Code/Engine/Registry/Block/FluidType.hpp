#pragma once

namespace enigma::registry::block
{
    /**
     * @brief Fluid type enumeration for liquid blocks
     * 
     * [MINECRAFT REF] Fluids.java registry
     * 
     * Used by LiquidBlock to determine:
     * - Same-type face culling (water next to water)
     * - Fluid rendering properties
     * - Fluid physics behavior
     */
    enum class FluidType
    {
        EMPTY, // No fluid (air, solid blocks)
        WATER, // Water fluid
        LAVA // Lava fluid
    };

    /**
     * @brief Convert FluidType to string for debugging
     */
    inline const char* FluidTypeToString(FluidType type)
    {
        switch (type)
        {
        case FluidType::EMPTY: return "EMPTY";
        case FluidType::WATER: return "WATER";
        case FluidType::LAVA: return "LAVA";
        default: return "UNKNOWN";
        }
    }

    /**
     * @brief Parse FluidType from string (for YAML loading)
     * @param str String representation ("water", "lava", "empty")
     * @return Parsed FluidType, defaults to EMPTY if unknown
     */
    inline FluidType ParseFluidType(const char* str)
    {
        if (!str) return FluidType::EMPTY;

        // Case-insensitive first character check
        if (str[0] == 'w' || str[0] == 'W') return FluidType::WATER;
        if (str[0] == 'l' || str[0] == 'L') return FluidType::LAVA;
        return FluidType::EMPTY;
    }
}
