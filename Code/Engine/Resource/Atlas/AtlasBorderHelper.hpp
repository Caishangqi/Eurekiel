#pragma once
#include "AtlasConfig.hpp"
#include "../../Core/Image.hpp"
#include "../../Math/IntVec2.hpp"

namespace enigma::resource
{
    /**
     * @class AtlasBorderHelper
     * @brief Pure static utility for atlas border extrusion operations
     *
     * Extrudes sprite edge pixels into surrounding padding area to prevent
     * mipmap color bleeding across atlas tile boundaries.
     *
     * Design: Pure static, no state, deleted constructors (project Helper pattern).
     */
    class AtlasBorderHelper
    {
    public:
        AtlasBorderHelper()                                    = delete;
        AtlasBorderHelper(const AtlasBorderHelper&)            = delete;
        AtlasBorderHelper& operator=(const AtlasBorderHelper&) = delete;

        /**
         * @brief Extrude sprite edge pixels into padding area on the atlas image
         * @param atlasImage     Target atlas image to modify (in-place)
         * @param spritePosition Top-left position of the sprite content (not the cell)
         * @param spriteSize     Size of the sprite content area
         * @param padding        Number of pixels to extrude in each direction
         * @param mode           Extrusion mode (CLAMP_TO_EDGE or NONE)
         */
        static void ExtrudeBorders(
            Image&              atlasImage,
            const IntVec2&      spritePosition,
            const IntVec2&      spriteSize,
            int                 padding,
            BorderExtrusionMode mode);
    };
}
