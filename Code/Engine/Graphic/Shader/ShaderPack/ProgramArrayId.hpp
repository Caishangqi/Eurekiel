/**
 * @file ProgramArrayId.hpp
 * @brief Iris兼容的程序数组ID枚举 - 对应 ProgramArrayId.java
 * @date 2025-10-04
 *
 * 对应 Iris 源码:
 * F:\...\Iris\shaderpack\loading\ProgramArrayId.java
 *
 * 设计要点:
 * - ProgramId: 单一程序 (每个枚举值对应一个着色器程序)
 * - ProgramArrayId: 程序数组 (每个枚举值对应100个槽位的着色器数组)
 *
 * 文件命名规则:
 * - ProgramId: gbuffers_terrain.vsh (无数字后缀)
 * - ProgramArrayId: composite.vsh, composite1.vsh, composite2.vsh...composite99.vsh
 *
 * 数组槽位分配:
 * - 槽位 0: composite.vsh/fsh
 * - 槽位 1: composite1.vsh/fsh
 * - 槽位 2: composite2.vsh/fsh
 * - ...
 * - 槽位 99: composite99.vsh/fsh
 */

#pragma once

#include <string>
#include <cstdint>

namespace enigma::graphic
{
    /**
     * @enum ProgramArrayId
     * @brief 程序数组类型枚举
     *
     * Iris 设计原则:
     * - 每个枚举值对应一组动态数量的着色器程序 (0-99个槽位)
     * - 支持自定义数量的后处理Pass
     * - 文件名带数字后缀 (例: composite1.vsh, deferred5.vsh)
     *
     * 对应 Iris ProgramArrayId.java 的完整6个数组类型
     */
    enum class ProgramArrayId : uint32_t
    {
        Setup, // setup, setup1...setup99
        Begin, // begin, begin1...begin99
        ShadowComposite, // shadowcomp, shadowcomp1...shadowcomp99
        Prepare, // prepare, prepare1...prepare99
        Deferred, // deferred, deferred1...deferred99
        Composite, // composite, composite1...composite99

        COUNT // 总数 = 6
    };

    /**
     * @brief 获取程序数组的源文件前缀
     *
     * Iris 规则: group.getBaseName()
     * 例如: Composite → "composite"
     *       Deferred → "deferred"
     *
     * 使用示例:
     * ```cpp
     * std::string prefix = GetProgramArrayPrefix(ProgramArrayId::Composite);
     * // prefix = "composite"
     * // 文件名: composite.vsh, composite1.vsh, composite2.vsh...composite99.vsh
     * ```
     *
     * @param arrayId 程序数组ID
     * @return 源文件名前缀 (例: "composite", "deferred", "shadowcomp")
     */
    inline std::string GetProgramArrayPrefix(ProgramArrayId arrayId)
    {
        switch (arrayId)
        {
        case ProgramArrayId::Setup: return "setup";
        case ProgramArrayId::Begin: return "begin";
        case ProgramArrayId::ShadowComposite: return "shadowcomp";
        case ProgramArrayId::Prepare: return "prepare";
        case ProgramArrayId::Deferred: return "deferred";
        case ProgramArrayId::Composite: return "composite";
        default: return "unknown";
        }
    }

    /**
     * @brief 获取程序数组的槽位数量
     *
     * Iris 实现: 所有程序数组固定100个槽位 (索引0-99)
     * 参考: ProgramArrayId.java:15-18
     * ```java
     * ProgramArrayId(ProgramGroup group, int numPrograms) {
     *     this.numPrograms = numPrograms; // 固定传入100
     * }
     * ```
     *
     * @param arrayId 程序数组ID
     * @return 槽位数量 (固定100)
     */
    inline constexpr int GetProgramArraySlotCount(ProgramArrayId arrayId)
    {
        // Iris 所有数组类型都是100个槽位
        (void)arrayId; // 避免未使用警告
        return 100;
    }

    /**
     * @brief 获取指定槽位的源文件名 (不含扩展名)
     *
     * 文件命名规则:
     * - 槽位 0: prefix (例: "composite")
     * - 槽位 1-99: prefix + index (例: "composite1", "composite2"..."composite99")
     *
     * 使用示例:
     * ```cpp
     * std::string name0 = GetProgramArraySlotName(ProgramArrayId::Composite, 0);
     * // name0 = "composite"
     *
     * std::string name5 = GetProgramArraySlotName(ProgramArrayId::Composite, 5);
     * // name5 = "composite5"
     * ```
     *
     * @param arrayId 程序数组ID
     * @param slotIndex 槽位索引 (0-99)
     * @return 源文件名 (不含扩展名)
     */
    inline std::string GetProgramArraySlotName(ProgramArrayId arrayId, int slotIndex)
    {
        std::string prefix = GetProgramArrayPrefix(arrayId);

        if (slotIndex == 0)
        {
            // 槽位0: 不带数字后缀 (例: "composite")
            return prefix;
        }
        else
        {
            // 槽位1-99: 带数字后缀 (例: "composite1", "composite2"..."composite99")
            return prefix + std::to_string(slotIndex);
        }
    }

    /**
     * @brief 将 ProgramArrayId 转换为字符串 (用于调试输出)
     *
     * @param arrayId 程序数组ID
     * @return 枚举值字符串表示
     */
    inline std::string ProgramArrayIdToString(ProgramArrayId arrayId)
    {
        switch (arrayId)
        {
        case ProgramArrayId::Setup: return "Setup";
        case ProgramArrayId::Begin: return "Begin";
        case ProgramArrayId::ShadowComposite: return "ShadowComposite";
        case ProgramArrayId::Prepare: return "Prepare";
        case ProgramArrayId::Deferred: return "Deferred";
        case ProgramArrayId::Composite: return "Composite";
        default: return "Unknown";
        }
    }

    /**
     * @brief 从字符串解析 ProgramArrayId (用于配置文件加载)
     *
     * @param arrayName 程序数组名称字符串
     * @return 对应的ProgramArrayId枚举值 (解析失败返回Composite)
     */
    inline ProgramArrayId StringToProgramArrayId(const std::string& arrayName)
    {
        if (arrayName == "Setup" || arrayName == "setup") return ProgramArrayId::Setup;
        if (arrayName == "Begin" || arrayName == "begin") return ProgramArrayId::Begin;
        if (arrayName == "ShadowComposite" || arrayName == "shadowcomp") return ProgramArrayId::ShadowComposite;
        if (arrayName == "Prepare" || arrayName == "prepare") return ProgramArrayId::Prepare;
        if (arrayName == "Deferred" || arrayName == "deferred") return ProgramArrayId::Deferred;
        if (arrayName == "Composite" || arrayName == "composite") return ProgramArrayId::Composite;

        return ProgramArrayId::Composite; // 默认值
    }
} // namespace enigma::graphic
