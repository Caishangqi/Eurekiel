#pragma once

#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief Buffer更新频率枚举
     *
     * 教学要点：
     * - PerObject: 每个Draw调用更新一次（高频，10,000次/帧）
     * - PerPass:   每个渲染Pass更新一次（中频，24次/帧）
     * - PerFrame:  每帧更新一次（低频，60次/秒）
     * - Static:    一次设置后不再更新（极低频）
     *
     * 用途：
     * - 决定Ring Buffer大小（PerObject需要大量副本）
     * - 优化内存和性能（Static只需1份，PerObject需要10,000份）
     */
    enum class UpdateFrequency : uint8_t
    {
        PerObject = 0, // 每个Draw调用更新一次（高频）
        PerPass   = 1, // 每个渲染Pass更新一次（中频）
        PerFrame  = 2, // 每帧更新一次（低频）
        Static    = 3  // 一次设置后不再更新（极低频）
    };
}
