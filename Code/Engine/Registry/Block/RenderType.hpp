#pragma once

namespace enigma::registry::block
{
    /**
     * @brief Render pass classification for blocks
     * 
     * Determines which render pass a block belongs to.
     * Used by ChunkMeshHelper to route blocks to correct render pass.
     * 
     * [MINECRAFT REF] ItemBlockRenderTypes / RenderType
     */
    enum class RenderType
    {
        SOLID, // Fully opaque blocks, no transparency (stone, dirt, etc.)
        CUTOUT, // Alpha test blocks (leaves, grass) - no depth sorting needed
        TRANSLUCENT // Alpha blend blocks (water, stained glass) - requires depth sorting
    };

    /**
     * @brief Convert RenderType to string for debugging
     */
    inline const char* RenderTypeToString(RenderType type)
    {
        switch (type)
        {
        case RenderType::SOLID: return "SOLID";
        case RenderType::CUTOUT: return "CUTOUT";
        case RenderType::TRANSLUCENT: return "TRANSLUCENT";
        default: return "UNKNOWN";
        }
    }

    /**
     * @brief Parse RenderType from string (for YAML loading)
     * @param str String representation ("opaque", "cutout", "translucent")
     * @return Parsed RenderType, defaults to SOLID if unknown
     */
    inline RenderType ParseRenderType(const char* str)
    {
        if (!str) return RenderType::SOLID;

        // Case-insensitive comparison
        if (str[0] == 'c' || str[0] == 'C') return RenderType::CUTOUT;
        if (str[0] == 't' || str[0] == 'T') return RenderType::TRANSLUCENT;
        return RenderType::SOLID;
    }
}
