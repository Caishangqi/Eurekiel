#pragma once

#include "DescriptorHandle.hpp"
#include <optional>
#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief 资源绑定特征类 - 可选的Bindless扩展
     *
     * 教学要点:
     * 1. 使用组合模式而非继承，遵循"组合优于继承"原则
     * 2. 可选特性设计：只有需要Bindless的资源才启用
     * 3. 类型安全：编译时确定资源是否支持Bindless
     * 4. 零开销抽象：不使用时无性能影响
     *
     * 设计原则:
     * - 单一职责：只负责Bindless相关状态
     * - 开放封闭：可扩展支持其他绑定方式
     * - 接口隔离：不强制所有资源都支持Bindless
     */
    class ResourceBindingTraits final
    {
    public:
        /**
         * @brief 默认构造 - 未绑定状态
         */
        ResourceBindingTraits() : m_isRegistered(false)
        {
        }

        /**
         * @brief 析构函数 - RAII自动清理
         */
        ~ResourceBindingTraits() = default;

        // ========================================================================
        // Bindless状态管理
        // ========================================================================

        /**
         * @brief 设置Bindless绑定信息
         * @param handle 描述符句柄
         * @param index 全局索引
         */
        void SetBindlessBinding(DescriptorHandle&& handle, uint32_t index)
        {
            m_bindlessHandle = std::move(handle);
            m_bindlessIndex  = index;
            m_isRegistered   = true;
        }

        /**
         * @brief 清除Bindless绑定
         */
        void ClearBindlessBinding()
        {
            m_bindlessHandle.reset();
            m_bindlessIndex.reset();
            m_isRegistered = false;
        }

        /**
         * @brief 检查是否已注册到Bindless系统
         */
        bool IsBindlessRegistered() const { return m_isRegistered; }

        /**
         * @brief 获取Bindless索引
         */
        std::optional<uint32_t> GetBindlessIndex() const { return m_bindlessIndex; }

        /**
         * @brief 获取描述符句柄
         */
        const std::optional<DescriptorHandle>& GetDescriptorHandle() const { return m_bindlessHandle; }

        /**
         * @brief 移动构造和移动赋值支持
         */
        ResourceBindingTraits(ResourceBindingTraits&&)            = default;
        ResourceBindingTraits& operator=(ResourceBindingTraits&&) = default;

        // 禁用拷贝
        ResourceBindingTraits(const ResourceBindingTraits&)            = delete;
        ResourceBindingTraits& operator=(const ResourceBindingTraits&) = delete;

    private:
        std::optional<DescriptorHandle> m_bindlessHandle; // Bindless描述符句柄
        std::optional<uint32_t>         m_bindlessIndex; // 全局Bindless索引
        bool                            m_isRegistered; // 是否已注册标记
    };
} // namespace enigma::graphic
