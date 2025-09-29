#pragma once

#include <cstdint>
#include <string>

namespace enigma::graphic
{
    /**
     * @brief Iris兼容的程序数组ID枚举
     *
     * 直接对应Iris源码中的ProgramArrayId.java枚举，定义了不同的
     * 着色器程序数组类型。每个数组ID对应一组相关的着色器程序。
     *
     * 基于Iris源码的发现：
     * - Begin: begin1-99着色器程序数组
     * - Prepare: prepare1-99着色器程序数组
     * - Deferred: deferred1-99着色器程序数组 (延迟光照)
     * - Composite: composite1-99着色器程序数组
     * - ShadowComposite: shadowcomp1-99着色器程序数组
     * - Debug: debug1-99着色器程序数组 (Enigma扩展)
     *
     * 教学要点：
     * - 理解着色器程序的组织方式
     * - 学习Iris的程序数组概念
     * - 掌握着色器管理的架构设计
     *
     * Iris架构对应：
     * Java: public enum ProgramArrayId
     * C++:  enum class ProgramArrayId : uint32_t
     */
    enum class ProgramArrayId : uint32_t
    {
        /**
         * @brief Begin程序数组
         *
         * 对应Iris中的Begin程序集，包含begin1-99着色器。
         * 用于渲染初始化和预处理阶段。
         */
        Begin = 0,

        /**
         * @brief Prepare程序数组
         *
         * 对应Iris中的Prepare程序集，包含prepare1-99着色器。
         * 用于G-Buffer准备和预处理阶段。
         */
        Prepare,

        /**
         * @brief Deferred程序数组
         *
         * 对应Iris中的Deferred程序集，包含deferred1-99着色器。
         * 用于延迟光照计算阶段。
         * 重要：这是Iris延迟光照的核心实现！
         */
        Deferred,

        /**
         * @brief Composite程序数组
         *
         * 对应Iris中的Composite程序集，包含composite1-99着色器。
         * 用于后处理和最终合成阶段。
         */
        Composite,

        /**
         * @brief ShadowComposite程序数组
         *
         * 对应Iris中的ShadowComposite程序集，包含shadowcomp1-99着色器。
         * 用于阴影后处理和合成阶段。
         */
        ShadowComposite,

        /**
         * @brief Debug程序数组 (Enigma扩展)
         *
         * Enigma引擎扩展的调试程序数组，包含debug1-99着色器。
         * 用于开发调试和管线验证。
         */
        Debug
    };

    /**
     * @brief 将ProgramArrayId枚举转换为字符串
     *
     * 用于调试输出、日志记录和着色器文件命名。
     *
     * @param arrayId 程序数组ID
     * @return 对应的字符串表示
     */
    inline std::string ProgramArrayIdToString(ProgramArrayId arrayId)
    {
        switch (arrayId)
        {
            case ProgramArrayId::Begin:           return "Begin";
            case ProgramArrayId::Prepare:         return "Prepare";
            case ProgramArrayId::Deferred:        return "Deferred";
            case ProgramArrayId::Composite:       return "Composite";
            case ProgramArrayId::ShadowComposite: return "ShadowComposite";
            case ProgramArrayId::Debug:           return "Debug";
            default:                              return "Unknown";
        }
    }

    /**
     * @brief 从字符串解析ProgramArrayId枚举
     *
     * 用于配置文件解析和着色器包加载。
     *
     * @param arrayName 程序数组名称字符串
     * @return 对应的ProgramArrayId枚举值，解析失败返回Begin
     */
    inline ProgramArrayId StringToProgramArrayId(const std::string& arrayName)
    {
        if (arrayName == "Begin")           return ProgramArrayId::Begin;
        if (arrayName == "Prepare")         return ProgramArrayId::Prepare;
        if (arrayName == "Deferred")        return ProgramArrayId::Deferred;
        if (arrayName == "Composite")       return ProgramArrayId::Composite;
        if (arrayName == "ShadowComposite") return ProgramArrayId::ShadowComposite;
        if (arrayName == "Debug")           return ProgramArrayId::Debug;
        return ProgramArrayId::Begin; // 默认值
    }

    /**
     * @brief 获取程序数组的着色器文件前缀
     *
     * 用于动态加载着色器文件，例如：
     * - Begin -> "begin"，对应begin1.fsh, begin2.fsh等
     * - Composite -> "composite"，对应composite1.fsh, composite2.fsh等
     *
     * @param arrayId 程序数组ID
     * @return 对应的文件前缀字符串
     */
    inline std::string GetShaderFilePrefix(ProgramArrayId arrayId)
    {
        switch (arrayId)
        {
            case ProgramArrayId::Begin:           return "begin";
            case ProgramArrayId::Prepare:         return "prepare";
            case ProgramArrayId::Deferred:        return "deferred";
            case ProgramArrayId::Composite:       return "composite";
            case ProgramArrayId::ShadowComposite: return "shadowcomp";
            case ProgramArrayId::Debug:           return "debug";
            default:                              return "unknown";
        }
    }

} // namespace enigma::graphic