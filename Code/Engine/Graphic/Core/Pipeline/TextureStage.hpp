#pragma once

#include <cstdint>
#include <string>

namespace enigma::graphic
{
    /**
     * @brief Iris兼容的纹理阶段枚举
     *
     * 直接对应Iris源码中的TextureStage.java枚举，定义了不同渲染阶段
     * 中纹理的使用状态和绑定阶段。每个阶段对应不同的纹理绑定需求。
     *
     * 基于Iris源码的发现：
     * - BEGIN: 初始阶段纹理
     * - PREPARE: 准备阶段纹理
     * - DEFERRED: 延迟渲染阶段纹理
     * - COMPOSITE_AND_FINAL: 合成和最终输出阶段纹理
     * - SHADOWCOMP: 阴影合成阶段纹理
     * - DEBUG: 调试阶段纹理 (Enigma扩展)
     *
     * 教学要点：
     * - 理解纹理在不同渲染阶段的角色
     * - 学习纹理生命周期管理
     * - 掌握Iris的纹理阶段概念
     *
     * Iris架构对应：
     * Java: public enum TextureStage
     * C++:  enum class TextureStage : uint32_t
     */
    enum class TextureStage : uint32_t
    {
        /**
         * @brief Begin阶段纹理
         *
         * 对应Iris中的BEGIN阶段，用于渲染初始化。
         * 主要包括：
         * - 初始化纹理
         * - 噪声纹理
         * - 预处理用纹理
         */
        BEGIN = 0,

        /**
         * @brief Prepare阶段纹理
         *
         * 对应Iris中的PREPARE阶段，用于G-Buffer准备。
         * 主要包括：
         * - G-Buffer纹理
         * - 深度纹理
         * - 法线纹理
         */
        PREPARE,

        /**
         * @brief Deferred阶段纹理
         *
         * 对应Iris中的DEFERRED阶段，用于延迟光照计算。
         * 主要包括：
         * - 光照缓冲纹理
         * - 阴影纹理
         * - 体积光纹理
         */
        DEFERRED,

        /**
         * @brief Composite和Final阶段纹理
         *
         * 对应Iris中的COMPOSITE_AND_FINAL阶段，用于后处理和最终输出。
         * 主要包括：
         * - 后处理中间纹理
         * - 最终颜色纹理
         * - 输出纹理
         */
        COMPOSITE_AND_FINAL,

        /**
         * @brief Shadow Composite阶段纹理
         *
         * 对应Iris中的SHADOWCOMP阶段，用于阴影后处理。
         * 主要包括：
         * - 阴影图纹理
         * - 阴影过滤纹理
         * - 阴影合成纹理
         */
        SHADOWCOMP,

        /**
         * @brief Debug阶段纹理 (Enigma扩展)
         *
         * Enigma引擎扩展的调试阶段纹理。
         * 主要包括：
         * - 调试渲染目标
         * - 测试纹理
         * - Bindless纹理数组
         */
        DEBUG
    };

    /**
     * @brief 将TextureStage枚举转换为字符串
     *
     * 用于调试输出和日志记录。
     *
     * @param stage 纹理阶段
     * @return 对应的字符串表示
     */
    inline std::string TextureStageToString(TextureStage stage)
    {
        switch (stage)
        {
            case TextureStage::BEGIN:               return "BEGIN";
            case TextureStage::PREPARE:             return "PREPARE";
            case TextureStage::DEFERRED:            return "DEFERRED";
            case TextureStage::COMPOSITE_AND_FINAL: return "COMPOSITE_AND_FINAL";
            case TextureStage::SHADOWCOMP:          return "SHADOWCOMP";
            case TextureStage::DEBUG:               return "DEBUG";
            default:                                return "UNKNOWN";
        }
    }

    /**
     * @brief 从字符串解析TextureStage枚举
     *
     * 用于配置文件解析和用户输入处理。
     *
     * @param stageName 阶段名称字符串
     * @return 对应的TextureStage枚举值，解析失败返回BEGIN
     */
    inline TextureStage StringToTextureStage(const std::string& stageName)
    {
        if (stageName == "BEGIN")               return TextureStage::BEGIN;
        if (stageName == "PREPARE")             return TextureStage::PREPARE;
        if (stageName == "DEFERRED")            return TextureStage::DEFERRED;
        if (stageName == "COMPOSITE_AND_FINAL") return TextureStage::COMPOSITE_AND_FINAL;
        if (stageName == "SHADOWCOMP")          return TextureStage::SHADOWCOMP;
        if (stageName == "DEBUG")               return TextureStage::DEBUG;
        return TextureStage::BEGIN; // 默认值
    }

    /**
     * @brief 检查纹理阶段是否需要特殊处理
     *
     * 某些纹理阶段需要特殊的绑定或管理方式。
     *
     * @param stage 纹理阶段
     * @return 是否需要特殊处理
     */
    inline bool RequiresSpecialHandling(TextureStage stage)
    {
        return stage == TextureStage::SHADOWCOMP ||
               stage == TextureStage::DEBUG;
    }

} // namespace enigma::graphic