#pragma once

#include <cstdint>
#include <string>

namespace enigma::graphic
{
    /**
     * @brief Iris兼容的合成渲染Pass类型枚举
     *
     * 直接对应Iris源码中的CompositePass.java枚举，定义了不同类型的
     * 合成渲染阶段。每个Pass对应不同的渲染目的和着色器程序集。
     *
     * 基于Iris源码的发现：
     * - BEGIN: 初始化阶段，对应begin1-99着色器
     * - PREPARE: 准备阶段，对应prepare1-99着色器
     * - DEFERRED: 延迟光照阶段，对应deferred1-99着色器
     * - COMPOSITE: 合成阶段，对应composite1-99着色器
     * - DEBUG: 调试阶段，用于开发测试(Enigma扩展)
     *
     * 教学要点：
     * - 理解渲染管线的阶段划分
     * - 学习枚举在状态管理中的应用
     * - 掌握Iris架构的Pass概念
     *
     * Iris架构对应：
     * Java: public enum CompositePass
     * C++:  enum class CompositePass : uint32_t
     */
    enum class CompositePass : uint32_t
    {
        /**
         * @brief Begin阶段 - 渲染初始化
         *
         * 对应Iris中的BEGIN pass，执行begin1-99着色器程序。
         * 主要用于：
         * - 清屏和初始状态设置
         * - 预处理和准备工作
         * - 全局效果初始化
         */
        BEGIN = 0,

        /**
         * @brief Prepare阶段 - G-Buffer准备
         *
         * 对应Iris中的PREPARE pass，执行prepare1-99着色器程序。
         * 主要用于：
         * - G-Buffer数据预处理
         * - 深度预处理
         * - 阴影图准备
         */
        PREPARE,

        /**
         * @brief Deferred阶段 - 延迟光照计算
         *
         * 对应Iris中的DEFERRED pass，执行deferred1-99着色器程序。
         * 重要发现：在Iris中，延迟光照是通过CompositeRenderer实现的！
         * 主要用于：
         * - 延迟光照计算
         * - 体积光效果
         * - 全局光照处理
         */
        DEFERRED,

        /**
         * @brief Composite阶段 - 最终合成
         *
         * 对应Iris中的COMPOSITE pass，执行composite1-99着色器程序。
         * 主要用于：
         * - 后处理效果合成
         * - 色调映射
         * - 抗锯齿处理
         * - 最终颜色校正
         */
        COMPOSITE,

        /**
         * @brief Debug阶段 - 调试渲染 (Enigma扩展)
         *
         * Enigma引擎扩展的调试阶段，用于开发和测试。
         * 主要用于：
         * - 调试几何体渲染
         * - Bindless纹理测试
         * - 管线验证
         * - 开发工具集成
         */
        DEBUG
    };

    /**
     * @brief 将CompositePass枚举转换为字符串
     *
     * 用于调试输出和日志记录。
     *
     * @param pass 合成Pass类型
     * @return 对应的字符串表示
     */
    inline std::string CompositePassToString(CompositePass pass)
    {
        switch (pass)
        {
            case CompositePass::BEGIN:     return "BEGIN";
            case CompositePass::PREPARE:   return "PREPARE";
            case CompositePass::DEFERRED:  return "DEFERRED";
            case CompositePass::COMPOSITE: return "COMPOSITE";
            case CompositePass::DEBUG:     return "DEBUG";
            default:                       return "UNKNOWN";
        }
    }

    /**
     * @brief 从字符串解析CompositePass枚举
     *
     * 用于配置文件解析和用户输入处理。
     *
     * @param passName Pass名称字符串
     * @return 对应的CompositePass枚举值，解析失败返回BEGIN
     */
    inline CompositePass StringToCompositePass(const std::string& passName)
    {
        if (passName == "BEGIN")     return CompositePass::BEGIN;
        if (passName == "PREPARE")   return CompositePass::PREPARE;
        if (passName == "DEFERRED")  return CompositePass::DEFERRED;
        if (passName == "COMPOSITE") return CompositePass::COMPOSITE;
        if (passName == "DEBUG")     return CompositePass::DEBUG;
        return CompositePass::BEGIN; // 默认值
    }

} // namespace enigma::graphic