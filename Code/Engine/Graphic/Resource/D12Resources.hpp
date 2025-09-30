#pragma once

#include <memory>
#include <string>
#include <optional>
#include <d3d12.h>
#include <dxgi1_6.h>
#include "BindlessResourceTypes.hpp"

namespace enigma::graphic
{
    /**
     * @brief D12Resource类 - DirectX 12资源的基础封装
     *
     * 教学要点:
     * 1. 这是所有DX12资源的基类，封装ID3D12Resource的公共操作
     * 2. 使用RAII管理资源生命周期，确保正确释放GPU内存
     * 3. 支持资源状态追踪，这是DX12的重要特性
     * 4. 专门为Bindless架构和延迟渲染设计
     *
     * DirectX 12特性:
     * - 显式资源状态管理 (不同于DX11的隐式状态)
     * - 支持资源别名和子资源
     * - GPU虚拟地址访问 (用于Bindless绑定)
     *
     * 架构设计 (Milestone 2.3):
     * - **可选Bindless支持**: 使用ResourceBindingTraits组合模式
     * - **零开销抽象**: 不使用Bindless时无额外性能开销
     * - **职责分离**: 资源管理与绑定管理分离
     * - **类型安全**: 编译时确定资源是否支持Bindless
     * - **统一注册接口**: 所有资源类型使用相同的便捷注册方法
     *
     * @note 这是新渲染系统专用的资源封装，支持现代Bindless架构
     */
    class D12Resource : public std::enable_shared_from_this<D12Resource>
    {
    protected:
        ID3D12Resource*       m_resource; // DX12资源指针
        D3D12_RESOURCE_STATES m_currentState; // 当前资源状态
        std::string           m_debugName; // 调试名称
        size_t                m_size; // 资源大小 (字节)
        bool                  m_isValid; // 资源有效性标记

        // SM6.6 Bindless索引 (Milestone 2.7) - 替代ResourceBindingTraits
        static constexpr uint32_t INVALID_BINDLESS_INDEX = UINT32_MAX;
        uint32_t m_bindlessIndex = INVALID_BINDLESS_INDEX;  // Bindless全局索引

    public:
        /**
         * @brief 构造函数
         * 
         * 教学要点: 基类构造函数初始化通用属性
         */
        D12Resource();

        /**
         * @brief 虚析构函数
         * 
         * 教学要点: 虚析构函数确保派生类正确释放资源
         */
        virtual ~D12Resource();

        // ========================================================================
        // 资源访问接口
        // ========================================================================

        /**
         * @brief 获取DX12资源指针
         * @return ID3D12Resource指针
         */
        ID3D12Resource* GetResource() const { return m_resource; }

        /**
         * @brief 获取GPU虚拟地址 (用于Bindless绑定)
         * @return GPU虚拟地址
         * 
         * 教学要点: GPU虚拟地址是DX12 Bindless资源绑定的关键
         */
        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;

        /**
         * @brief 获取当前资源状态
         * @return 资源状态枚举
         */
        D3D12_RESOURCE_STATES GetCurrentState() const { return m_currentState; }

        /**
         * @brief 设置资源状态 (通常由资源状态追踪器调用)
         * @param newState 新的资源状态
         */
        void SetCurrentState(D3D12_RESOURCE_STATES newState) { m_currentState = newState; }

        /**
         * @brief 获取资源大小
         * @return 资源大小 (字节)
         */
        size_t GetSize() const { return m_size; }

        /**
         * @brief 检查资源是否有效
         * @return 有效返回true，否则返回false
         */
        bool IsValid() const { return m_isValid && m_resource != nullptr; }

        /**
         * @brief 设置调试名称 (用于PIX调试) - 虚函数支持子类自定义行为
         * @param name 调试名称
         *
         * 教学要点: 虚函数允许子类重写调试名称设置逻辑
         * 例如，纹理类可以在设置名称时同时更新纹理视图的名称
         */
        virtual void SetDebugName(const std::string& name);

        /**
         * @brief 获取调试名称 - 虚函数支持子类自定义格式
         * @return 调试名称字符串
         *
         * 教学要点: 虚函数允许子类返回格式化的调试信息
         * 例如，缓冲区类可以在名称中包含大小信息
         */
        virtual const std::string& GetDebugName() const { return m_debugName; }

        /**
         * @brief 获取详细调试信息 - 纯虚函数要求子类必须实现
         * @return 详细调试信息字符串
         *
         * 教学要点: 纯虚函数强制所有子类提供具体的调试信息实现
         * 这遵循了接口隔离原则，每个资源类型都有其特定的调试信息
         */
        virtual std::string GetDebugInfo() const = 0;

        // ========================================================================
        // SM6.6 Bindless资源管理接口 (Milestone 2.7简化版)
        // ========================================================================

        /**
         * @brief 检查是否已注册到Bindless系统
         * @return 已注册返回true(索引有效)
         *
         * 教学要点:
         * 1. SM6.6简化设计：从两层检查简化为单一索引有效性检查
         * 2. 零额外开销：不再使用std::optional,直接判断索引
         */
        bool IsBindlessRegistered() const
        {
            return m_bindlessIndex != INVALID_BINDLESS_INDEX;
        }

        /**
         * @brief 获取Bindless全局索引
         * @return 全局索引,INVALID_BINDLESS_INDEX表示未注册
         *
         * 教学要点:
         * 1. SM6.6简化：从std::optional<uint32_t>改为uint32_t直接返回
         * 2. 使用INVALID_BINDLESS_INDEX代替std::nullopt
         * 3. 更高效,无额外堆内存分配
         */
        uint32_t GetBindlessIndex() const { return m_bindlessIndex; }

        /**
         * @brief 设置Bindless索引(由BindlessIndexAllocator调用)
         * @param index 全局索引
         *
         * 教学要点:
         * 1. SM6.6简化：移除DescriptorHandle参数
         * 2. 直接设置索引,无需检查启用状态
         * 3. 描述符创建由CreateDescriptorInGlobalHeap()负责
         */
        void SetBindlessIndex(uint32_t index)
        {
            m_bindlessIndex = index;
        }

        /**
         * @brief 清除Bindless索引
         *
         * 教学要点: SM6.6简化 - 直接重置索引为无效值
         */
        void ClearBindlessIndex()
        {
            m_bindlessIndex = INVALID_BINDLESS_INDEX;
        }

        /**
         * @brief 在全局描述符堆中创建描述符(纯虚函数)
         * @param device DX12设备
         * @param heapManager 全局描述符堆管理器
         *
         * 教学要点:
         * 1. SM6.6架构中,描述符创建与索引分配分离
         * 2. 子类必须实现如何在全局堆中创建自己的描述符
         * 3. 描述符创建到指定索引(m_bindlessIndex)
         * 4. 全局堆容量1M,支持百万级资源
         *
         * 实现示例(D12Texture):
         * ```cpp
         * void CreateDescriptorInGlobalHeap(ID3D12Device* device,
         *                                   GlobalDescriptorHeapManager* heapManager) override
         * {
         *     if (!IsBindlessRegistered())
         *         return;
         *
         *     D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
         *     // ... 配置srvDesc ...
         *     heapManager->CreateShaderResourceView(device, m_resource, &srvDesc, m_bindlessIndex);
         * }
         * ```
         */
        virtual void CreateDescriptorInGlobalHeap(
            ID3D12Device* device,
            class GlobalDescriptorHeapManager* heapManager) = 0;

        // ========================================================================
        // 便捷注册接口 (Milestone 2.3新增) - 统一的资源注册方法
        // ========================================================================

        /**
         * @brief 注册到Bindless资源管理器 (通过D3D12RenderSystem统一接口)
         * @return 成功返回全局索引，失败返回nullopt
         *
         * 教学要点:
         * 1. 分层架构：D12Resource -> D3D12RenderSystem -> ShaderResourceBinder -> BindlessResourceManager
         * 2. 统一接口设计：所有资源类型都使用相同的注册接口
         * 3. 自动类型检测：基于资源类型自动选择正确的注册方法
         * 4. 依赖倒置原则：依赖于抽象(D3D12RenderSystem)而非具体实现(BindlessResourceManager)
         *
         * 实现指导:
         * ```cpp
         * // 1. 检查是否已经注册
         * if (IsBindlessRegistered()) {
         *     return std::nullopt;  // 避免重复注册
         * }
         *
         * // 2. 通过D3D12RenderSystem统一接口注册
         * auto resourceType = GetDefaultBindlessResourceType();
         * return D3D12RenderSystem::RegisterResourceToBindless(shared_from_this(), resourceType);
         * ```
         */
        std::optional<uint32_t> RegisterToBindlessManager();

        /**
         * @brief 从Bindless资源管理器注销 (通过D3D12RenderSystem统一接口)
         * @return 成功返回true，失败返回false
         *
         * 教学要点:
         * 1. 分层架构：D12Resource -> D3D12RenderSystem -> ShaderResourceBinder -> BindlessResourceManager
         * 2. 依赖倒置原则：依赖于抽象接口而非具体实现
         * 3. RAII设计：确保资源正确注销，避免资源泄漏
         *
         * 实现指导:
         * ```cpp
         * if (!IsBindlessRegistered()) {
         *     return false;  // 未注册无需注销
         * }
         * return D3D12RenderSystem::UnregisterResourceFromBindless(shared_from_this());
         * ```
         */
        bool UnregisterFromBindlessManager();

    protected:
        /**
         * @brief 获取资源的默认Bindless类型 (由子类重写)
         * @return 资源的默认类型
         *
         * 教学要点: 虚函数让子类定义自己的默认Bindless类型
         * - D12Texture返回BindlessResourceType::Texture2D
         * - D12Buffer根据用途返回ConstantBuffer或StructuredBuffer
         */
        virtual BindlessResourceType GetDefaultBindlessResourceType() const = 0;

    protected:
        /**
         * @brief 设置资源指针 (由派生类调用)
         * @param resource DX12资源指针
         * @param initialState 初始资源状态
         * @param size 资源大小
         */
        void SetResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, size_t size);

        /**
         * @brief 释放资源
         * 
         * 教学要点: 安全释放DX12资源，避免重复释放
         */
        virtual void ReleaseResource();

    protected:
        // 支持移动语义（派生类需要访问）
        D12Resource(D12Resource&&)            = default;
        D12Resource& operator=(D12Resource&&) = default;

    private:
        // 禁用拷贝构造和赋值
        D12Resource(const D12Resource&)            = delete;
        D12Resource& operator=(const D12Resource&) = delete;
    };
} // namespace enigma::graphic
