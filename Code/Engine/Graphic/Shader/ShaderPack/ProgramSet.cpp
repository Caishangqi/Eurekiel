/**
 * @file ProgramSet.cpp
 * @brief Shader Pack 程序集容器实现 - DirectX 12 简化版本
 * @date 2025-10-05
 *
 * 职责:
 * 1. 管理所有着色器程序 (47 个单一程序 + 6 个程序数组)
 * 2. Compute Shader 统一存储在 ShaderSource 中 (DirectX 12 架构优势)
 * 3. 提供程序查询和注册接口
 *
 * DirectX 12 架构优势:
 * - Iris: ProgramSource (图形) + ComputeSource (计算) 分离
 * - DirectX 12: ShaderSource 统一管理,Compute Shader 作为可选字段
 * - 简化架构,避免重复管理
 */

#include "ProgramSet.hpp"
#include "ShaderSource.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // 构造函数和析构函数（Pimpl 惯用法）
    // ========================================================================

    ProgramSet::~ProgramSet() = default; // 在这里定义，此时 ShaderSource 已完整定义

    // ========================================================================
    // 单一程序访问 - 对应 Iris get(ProgramId)
    // ========================================================================

    std::optional<ShaderSource*> ProgramSet::Get(ProgramId id) const
    {
        auto it = m_singlePrograms.find(id);
        if (it == m_singlePrograms.end())
        {
            return std::nullopt;
        }

        // 自动调用 IsValid() 验证
        ShaderSource* source = it->second.get();
        if (source && source->IsValid())
        {
            return source;
        }

        return std::nullopt;
    }

    ShaderSource* ProgramSet::GetRaw(ProgramId id) const
    {
        auto it = m_singlePrograms.find(id);
        if (it != m_singlePrograms.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    // ========================================================================
    // 程序数组访问 - 对应 Iris getComposite(ProgramArrayId)
    // ========================================================================

    const std::array<ShaderSource*, ProgramSet::MAX_PROGRAM_ARRAY_SIZE>&
    ProgramSet::GetComposite(ProgramArrayId id) const
    {
        // 教学要点:
        // Iris 如果找不到数组，返回空数组 new ProgramSource[numPrograms]
        // 我们返回静态空数组避免动态分配

        static std::array<ShaderSource*, MAX_PROGRAM_ARRAY_SIZE> emptyArray{};

        auto it = m_programArrays.find(id);
        if (it == m_programArrays.end())
        {
            return emptyArray;
        }

        // 需要转换 unique_ptr 数组为裸指针数组
        // 这里使用线程局部存储避免多线程问题
        thread_local std::array<ShaderSource*, MAX_PROGRAM_ARRAY_SIZE> rawPointers{};

        const auto& uniquePtrArray = it->second;
        for (size_t i = 0; i < MAX_PROGRAM_ARRAY_SIZE; ++i)
        {
            rawPointers[i] = uniquePtrArray[i].get();
        }

        return rawPointers;
    }

    // ========================================================================
    // 程序注册 - 用于 ShaderPackLoader
    // ========================================================================

    void ProgramSet::RegisterProgram(ProgramId id, std::unique_ptr<ShaderSource> source)
    {
        if (!source)
        {
            return;
        }

        // 设置 ProgramSet 反向引用
        source->SetParent(this);

        // 插入或替换
        m_singlePrograms[id] = std::move(source);
    }

    void ProgramSet::RegisterArrayProgram(ProgramArrayId id, size_t index, std::unique_ptr<ShaderSource> source)
    {
        if (index >= MAX_PROGRAM_ARRAY_SIZE)
        {
            return;
        }

        if (!source)
        {
            return;
        }

        // 设置 ProgramSet 反向引用
        source->SetParent(this);

        // 确保数组存在
        auto& array  = m_programArrays[id];
        array[index] = std::move(source);
    }

    // ========================================================================
    // 批量访问 - 用于遍历所有程序
    // ========================================================================

    const std::unordered_map<ProgramId, std::unique_ptr<ShaderSource>>&
    ProgramSet::GetPrograms() const
    {
        return m_singlePrograms;
    }

    // ========================================================================
    // 统计和验证
    // ========================================================================

    size_t ProgramSet::GetLoadedProgramCount() const
    {
        size_t count = 0;

        // 统计单一程序
        for (const auto& [id, source] : m_singlePrograms)
        {
            if (source && source->IsValid())
            {
                ++count;
            }
        }

        return count;
    }

    size_t ProgramSet::GetLoadedArrayCount() const
    {
        size_t count = 0;

        // 统计程序数组
        for (const auto& [id, array] : m_programArrays)
        {
            for (const auto& source : array)
            {
                if (source && source->IsValid())
                {
                    ++count;
                }
            }
        }

        return count;
    }

    bool ProgramSet::Validate() const
    {
        // 教学要点:
        // 验证必需程序是否存在
        // 对应 Iris 的验证逻辑

        // 检查基础程序是否存在
        auto basic = Get(ProgramId::Basic);
        if (!basic.has_value())
        {
            return false;
        }

        auto textured = Get(ProgramId::Textured);
        if (!textured.has_value())
        {
            return false;
        }

        // 可以添加更多验证逻辑

        return true;
    }
} // namespace enigma::graphic
