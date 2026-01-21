#pragma once

namespace enigma::registry::block
{
    /**
     * @brief Block render shape types
     * 
     * Determines how a block is rendered in the world.
     * [MINECRAFT REF] RenderShape.java
     */
    enum class RenderShape
    {
        INVISIBLE, // Not rendered by standard renderer (liquids use dedicated LiquidBlockRenderer)
        ENTITYBLOCK_ANIMATED, // Entity-based animated rendering (chests, signs, etc.)
        MODEL // Standard model rendering (default for most blocks)
    };

    /**
     * @brief Convert RenderShape to string for debugging
     */
    inline const char* RenderShapeToString(RenderShape shape)
    {
        switch (shape)
        {
        case RenderShape::INVISIBLE: return "INVISIBLE";
        case RenderShape::ENTITYBLOCK_ANIMATED: return "ENTITYBLOCK_ANIMATED";
        case RenderShape::MODEL: return "MODEL";
        default: return "UNKNOWN";
        }
    }
}
