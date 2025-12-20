#pragma once
#include "Engine/Core/Vertex_PCU.hpp"

typedef Vertex_PCUTBN Vertex;
typedef unsigned int  Index;

namespace enigma::graphic
{
    // ========================================================================
    // [NEW] Graphic Module Common Constants
    // ========================================================================
    // These constants define system-wide limits for the Graphic module.
    // All modules should reference these constants instead of hardcoding values.
    //
    // Teaching Points:
    // 1. Single Source of Truth - all limits defined in one place
    // 2. Compile-time validation - static_assert ensures consistency
    // 3. Semantic naming - MAX_DRAWS_PER_FRAME vs MAX_RING_FRAMES for clarity
    // ========================================================================

    /**
     * @brief Maximum number of Custom Buffers (space=1 Descriptor Table slots)
     *
     * This limits the number of custom constant buffers that can be registered
     * with space=1 (Descriptor Table binding). Slots 0-99 are available.
     *
     * @note Engine Buffers (space=0) use Root CBV slots 0-14 separately.
     */
    static constexpr uint32_t MAX_CUSTOM_BUFFERS = 100;

    /**
     * @brief Maximum number of Draws per frame (Ring Descriptor Table copies)
     *
     * [FIX] Ring Descriptor Table architecture:
     * - Each Draw uses a different Descriptor Table copy
     * - Prevents CBV Descriptor overwrite between multi-draw calls
     * - Total Descriptor Pool size = MAX_DRAWS_PER_FRAME * MAX_CUSTOM_BUFFERS
     *
     * @note This effectively limits per-frame draw call count for Custom Buffers
     * @warning Exceeding this limit will cause Ring Buffer index wrap-around
     */
    static constexpr uint32_t MAX_DRAWS_PER_FRAME = 64;

    /**
     * @brief Total Custom CBV Descriptor Pool size
     *
     * Calculated from MAX_DRAWS_PER_FRAME * MAX_CUSTOM_BUFFERS
     * Example: 64 * 100 = 6400 Descriptors
     */
    static constexpr uint32_t CUSTOM_CBV_DESCRIPTOR_POOL_SIZE = MAX_DRAWS_PER_FRAME * MAX_CUSTOM_BUFFERS;

    /**
     * @brief Maximum Ring Buffer capacity for Engine Buffers (space=0)
     *
     * Engine Buffers use direct Ring Buffer (not Ring Descriptor Table),
     * so they are not limited by MAX_DRAWS_PER_FRAME.
     * This value controls maximum per-frame draws for PerObject Engine Buffers.
     *
     * Memory impact: Buffer size * ENGINE_BUFFER_RING_CAPACITY
     * Example: MatricesUniforms (1280 bytes) * 10000 = 12.8 MB
     *
     * @note This is separate from MAX_DRAWS_PER_FRAME which limits Custom Buffers
     */
    static constexpr uint32_t ENGINE_BUFFER_RING_CAPACITY = 10000;

    // Compile-time validation
    static_assert(MAX_CUSTOM_BUFFERS > 0, "MAX_CUSTOM_BUFFERS must be positive");
    static_assert(MAX_DRAWS_PER_FRAME > 0, "MAX_DRAWS_PER_FRAME must be positive");
    static_assert(CUSTOM_CBV_DESCRIPTOR_POOL_SIZE <= 1000000, "Descriptor Pool exceeds 1M limit");
}
