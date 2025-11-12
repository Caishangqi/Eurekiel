#pragma once
#include <cstdint>

namespace enigma::voxel
{
    /**
     * @brief Chunk coordinate utilities
     * 
     * Pure static utility class for chunk coordinate operations.
     * Follows shrimp-rules.md Helper class design pattern.
     */
    class ChunkHelper
    {
    public:
        /**
         * @brief Pack 2D chunk coordinates into a single 64-bit integer
         * @param x Chunk X coordinate
         * @param y Chunk Y coordinate
         * @return Packed 64-bit coordinate
         */
        static int64_t PackCoordinates(int32_t x, int32_t y);

        /**
         * @brief Unpack 64-bit integer into 2D chunk coordinates
         * @param packed Packed coordinate
         * @param x Output chunk X coordinate
         * @param y Output chunk Y coordinate
         */
        static void UnpackCoordinates(int64_t packed, int32_t& x, int32_t& y);
    };
}
