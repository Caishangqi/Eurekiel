#include "ChunkHelper.hpp"

namespace enigma::voxel
{
    int64_t ChunkHelper::PackCoordinates(int32_t x, int32_t y)
    {
        // Convert signed 32-bit integers to unsigned for proper bit manipulation
        uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(x));
        uint64_t uy = static_cast<uint64_t>(static_cast<uint32_t>(y));

        // Pack: high 32 bits = y, low 32 bits = x
        return static_cast<int64_t>((uy << 32) | ux);
    }

    void ChunkHelper::UnpackCoordinates(int64_t packed, int32_t& x, int32_t& y)
    {
        uint64_t upacked = static_cast<uint64_t>(packed);

        // Unpack: extract high and low 32 bits, then convert back to signed
        x = static_cast<int32_t>(static_cast<uint32_t>(upacked & 0xFFFFFFFF));
        y = static_cast<int32_t>(static_cast<uint32_t>(upacked >> 32));
    }
}
