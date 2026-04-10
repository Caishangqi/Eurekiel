#pragma once
#include "Engine/Core/Vertex_PCU.hpp"

typedef Vertex_PCUTBN Vertex;
typedef unsigned int  Index;

namespace enigma::graphic
{
    // ========================================================================
    // Graphic Module Common Constants
    // ========================================================================
    // These constants define system-wide limits for the Graphic module.
    // All modules should reference these constants instead of hardcoding values.
    //
    // Teaching Points:
    // 1. Single Source of Truth - all limits defined in one place
    // 2. Compile-time validation - static_assert ensures consistency
    // 3. Semantic naming - MAX_DRAWS_PER_FRAME vs MAX_RING_FRAMES for clarity
    // ========================================================================

    static constexpr uint32_t MIN_FRAMES_IN_FLIGHT_DEPTH            = 1;
    static constexpr uint32_t MAX_SUPPORTED_FRAMES_IN_FLIGHT_DEPTH  = 3;
    static constexpr uint32_t DEFAULT_ACTIVE_FRAMES_IN_FLIGHT_DEPTH = 2;
    static constexpr uint32_t DEFAULT_REQUESTED_FRAMES_IN_FLIGHT_DEPTH =
        MAX_SUPPORTED_FRAMES_IN_FLIGHT_DEPTH;

    struct FramesInFlightConfig
    {
        uint32_t requestedDepth = DEFAULT_REQUESTED_FRAMES_IN_FLIGHT_DEPTH;
        uint32_t activeDepth = DEFAULT_ACTIVE_FRAMES_IN_FLIGHT_DEPTH;
        uint32_t maxSupportedDepth = MAX_SUPPORTED_FRAMES_IN_FLIGHT_DEPTH;
        bool allowRuntimeReconfigure = false;
    };

    constexpr uint32_t ClampFramesInFlightDepth(
        uint32_t depth,
        uint32_t maxSupportedDepth = MAX_SUPPORTED_FRAMES_IN_FLIGHT_DEPTH) noexcept
    {
        const uint32_t sanitizedMaxDepth =
            maxSupportedDepth < MIN_FRAMES_IN_FLIGHT_DEPTH
            ? MIN_FRAMES_IN_FLIGHT_DEPTH
            : maxSupportedDepth;

        if (depth < MIN_FRAMES_IN_FLIGHT_DEPTH)
        {
            return MIN_FRAMES_IN_FLIGHT_DEPTH;
        }

        if (depth > sanitizedMaxDepth)
        {
            return sanitizedMaxDepth;
        }

        return depth;
    }

    constexpr bool IsFramesInFlightDepthSupported(
        uint32_t depth,
        uint32_t maxSupportedDepth = MAX_SUPPORTED_FRAMES_IN_FLIGHT_DEPTH) noexcept
    {
        const uint32_t sanitizedMaxDepth =
            maxSupportedDepth < MIN_FRAMES_IN_FLIGHT_DEPTH
            ? MIN_FRAMES_IN_FLIGHT_DEPTH
            : maxSupportedDepth;
        return depth >= MIN_FRAMES_IN_FLIGHT_DEPTH && depth <= sanitizedMaxDepth;
    }

    constexpr FramesInFlightConfig NormalizeFramesInFlightConfig(FramesInFlightConfig config) noexcept
    {
        config.maxSupportedDepth =
            config.maxSupportedDepth < MIN_FRAMES_IN_FLIGHT_DEPTH
            ? MIN_FRAMES_IN_FLIGHT_DEPTH
            : config.maxSupportedDepth;
        config.requestedDepth = ClampFramesInFlightDepth(config.requestedDepth, config.maxSupportedDepth);
        config.activeDepth = ClampFramesInFlightDepth(config.activeDepth, config.maxSupportedDepth);
        return config;
    }

    constexpr FramesInFlightConfig GetDefaultFramesInFlightConfig() noexcept
    {
        return NormalizeFramesInFlightConfig(FramesInFlightConfig{});
    }

    constexpr uint32_t GetDefaultRequestedFramesInFlightDepth() noexcept
    {
        return GetDefaultFramesInFlightConfig().requestedDepth;
    }

    constexpr uint32_t GetDefaultActiveFramesInFlightDepth() noexcept
    {
        return GetDefaultFramesInFlightConfig().activeDepth;
    }

    constexpr uint32_t GetMaxSupportedFramesInFlightDepth() noexcept
    {
        return GetDefaultFramesInFlightConfig().maxSupportedDepth;
    }

    /**
     * @brief Current compiled active frames-in-flight depth
     *
     * Frame-partitioned resource layouts still use this value today.
     * Requested depth can differ from active depth until runtime reconfiguration
     * is wired through the remaining subsystems.
     */
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = DEFAULT_ACTIVE_FRAMES_IN_FLIGHT_DEPTH;

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
    static constexpr uint32_t MAX_DRAWS_PER_FRAME = 1024;

    /**
     * @brief Total Custom CBV Descriptor Pool size
     *
     * Frame-isolated Ring Descriptor Table: each in-flight frame gets its own
     * descriptor region to prevent CPU/GPU race conditions.
     * Calculated from MAX_FRAMES_IN_FLIGHT * MAX_DRAWS_PER_FRAME * MAX_CUSTOM_BUFFERS
     */
    static constexpr uint32_t CUSTOM_CBV_DESCRIPTOR_POOL_SIZE = MAX_FRAMES_IN_FLIGHT * MAX_DRAWS_PER_FRAME * MAX_CUSTOM_BUFFERS;

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
    static_assert(IsFramesInFlightDepthSupported(
                      DEFAULT_ACTIVE_FRAMES_IN_FLIGHT_DEPTH,
                      MAX_SUPPORTED_FRAMES_IN_FLIGHT_DEPTH),
                  "Default active frames-in-flight depth must stay within the supported range");
    static_assert(IsFramesInFlightDepthSupported(
                      DEFAULT_REQUESTED_FRAMES_IN_FLIGHT_DEPTH,
                      MAX_SUPPORTED_FRAMES_IN_FLIGHT_DEPTH),
                  "Default requested frames-in-flight depth must stay within the supported range");
    static_assert(MAX_FRAMES_IN_FLIGHT == DEFAULT_ACTIVE_FRAMES_IN_FLIGHT_DEPTH,
                  "MAX_FRAMES_IN_FLIGHT must mirror the compiled active depth");
    static_assert(MAX_CUSTOM_BUFFERS > 0, "MAX_CUSTOM_BUFFERS must be positive");
    static_assert(MAX_DRAWS_PER_FRAME > 0, "MAX_DRAWS_PER_FRAME must be positive");
    static_assert(CUSTOM_CBV_DESCRIPTOR_POOL_SIZE <= 1000000, "Descriptor Pool exceeds 1M limit");
}
