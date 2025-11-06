/**
 * @file ProgramSet.hpp
 * @brief Shader Pack 程序集容器 - 对应 Iris ProgramSet
 * @date 2025-10-05
 *
 * 职责:
 * 1. 管理所有着色器程序 (47 个单一程序 + 6 个程序数组)
 * 2. Compute Shader 统一存储在 ShaderSource 中 (DirectX 12 架构优势)
 * 3. 提供程序查询接口
 *
 * DirectX 12 架构优势:
 * - Iris: ProgramSource (图形) + ComputeSource (计算) 分离
 * - DirectX 12: ShaderSource 统一管理,Compute Shader 作为可选字段
 * - 简化架构,避免重复管理
 */

#pragma once

#include "ProgramId.hpp"
#include "ProgramArrayId.hpp"
#include <unordered_map>
#include <array>
#include <optional>
#include <memory>

#include "ShaderSource.hpp"

namespace enigma::graphic
{
    /**
     * @class ProgramSet
     * @brief Shader Pack 程序集容器 - DirectX 12 简化版本
     *
     * 教学要点:
     * - 对应 Iris 的 ProgramSet.java
     * - 管理 47 个单一着色器程序
     * - 管理 6 个程序数组 (每个最多 100 个程序)
     * - ShaderSource 统一管理图形和计算着色器
     *
     * 设计决策:
     * - std::unordered_map 代替 Java EnumMap
     * - std::array<ShaderSource*, 100> 固定大小数组
     * - std::optional 表示可选程序
     * - 智能指针管理生命周期
     */
    class ProgramSet
    {
    public:
        static constexpr size_t MAX_PROGRAM_ARRAY_SIZE = 100;

        ProgramSet() = default;
        ~ProgramSet(); // 在 .cpp 中定义（Pimpl 惯用法，避免 incomplete type 错误）

        // 禁用拷贝
        ProgramSet(const ProgramSet&)            = delete;
        ProgramSet& operator=(const ProgramSet&) = delete;

        // 支持移动
        ProgramSet(ProgramSet&&) noexcept            = default;
        ProgramSet& operator=(ProgramSet&&) noexcept = default;

        // ========================================================================
        // 单一程序访问 - 对应 Iris get(ProgramId)
        // ========================================================================

        /**
         * @brief 获取单一着色器程序
         * @param id 程序 ID
         * @return 如果程序存在且有效返回 ShaderSource，否则返回 nullopt
         */
        std::optional<ShaderSource*> Get(ProgramId id) const;

        /**
         * @brief 获取单一着色器程序 (无验证)
         * @param id 程序 ID
         * @return 程序指针 (可能为 nullptr)
         */
        ShaderSource* GetRaw(ProgramId id) const;

        // ========================================================================
        // 程序数组访问 - 对应 Iris getComposite(ProgramArrayId)
        // ========================================================================

        /**
         * @brief 获取程序数组
         * @param id 程序数组 ID
         * @return 程序数组 (固定 100 个元素，可能为 nullptr)
         */
        const std::array<ShaderSource*, MAX_PROGRAM_ARRAY_SIZE>& GetComposite(ProgramArrayId id) const;

        // ========================================================================
        // 程序注册 - 用于 ShaderPackLoader
        // ========================================================================

        void RegisterProgram(ProgramId id, std::unique_ptr<ShaderSource> source);
        void RegisterArrayProgram(ProgramArrayId id, size_t index, std::unique_ptr<ShaderSource> source);

        // ========================================================================
        // 批量访问 - 用于遍历所有程序
        // ========================================================================

        /**
         * @brief 获取所有单一程序的映射表（用于批量操作）
         * @return 程序映射表的常量引用
         * 
         * 教学要点:
         * - 与Get(ProgramId)的区别：单个查询 vs 批量遍历
         * - 返回内部容器的const引用，避免拷贝
         * - 典型用途：缓存所有程序、统计、调试输出
         */
        const std::unordered_map<ProgramId, std::unique_ptr<ShaderSource>>& GetPrograms() const;

        // ========================================================================
        // 统计和验证
        // ========================================================================

        size_t GetLoadedProgramCount() const;
        size_t GetLoadedArrayCount() const;
        bool   Validate() const;

    private:
        /// 单一程序映射 (47 个)
        std::unordered_map<ProgramId, std::unique_ptr<ShaderSource>> m_singlePrograms;

        /// 程序数组映射 (6 个类型 × 100 个程序)
        std::unordered_map<
            ProgramArrayId,
            std::array<std::unique_ptr<ShaderSource>, MAX_PROGRAM_ARRAY_SIZE>
        > m_programArrays;
    };
} // namespace enigma::graphic
