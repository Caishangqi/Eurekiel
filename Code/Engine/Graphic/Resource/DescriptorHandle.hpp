#pragma once

#include "GlobalDescriptorHeapManager.hpp"
#include <memory>

namespace enigma::graphic
{
    // 前向声明
    class GlobalDescriptorHeapManager;

    /**
     * @brief DescriptorHandle类 - RAII描述符句柄封装
     *
     * 教学要点:
     * 1. RAII (Resource Acquisition Is Initialization) - 现代C++核心概念
     * 2. 构造时获取资源，析构时自动释放，避免内存泄漏
     * 3. 移动语义 - 现代C++性能优化的关键
     * 4. 智能指针模式 - 自动管理资源生命周期
     *
     * 设计原则:
     * - 单一职责: 只负责单个描述符的生命周期管理
     * - 异常安全: 即使发生异常也能正确释放资源
     * - 零开销抽象: 编译时优化，运行时无额外开销
     * - 移动优先: 避免不必要的资源复制
     *
     * @note 这是现代C++和DirectX 12最佳实践的完美结合
     */
    class DescriptorHandle final
    {
        // Declare non-member swap function as friend to allow access to private Swap method
        friend void swap(DescriptorHandle& lhs, DescriptorHandle& rhs) noexcept;
    private:
        // 核心数据
        GlobalDescriptorHeapManager::DescriptorAllocation m_allocation; // 描述符分配信息
        std::weak_ptr<GlobalDescriptorHeapManager>        m_heapManager; // 堆管理器弱引用 (避免循环依赖)
        bool                                        m_ownsResource; // 是否拥有资源所有权

    public:
        /**
         * @brief 默认构造函数 - 创建无效句柄
         */
        DescriptorHandle() noexcept;

        /**
         * @brief 构造函数 - 从描述符分配创建
         * @param allocation 描述符分配信息
         * @param heapManager 堆管理器共享指针
         */
        DescriptorHandle(const GlobalDescriptorHeapManager::DescriptorAllocation& allocation, std::shared_ptr<GlobalDescriptorHeapManager> heapManager) noexcept;

        /**
         * @brief 析构函数 - RAII自动释放资源
         *
         * 教学要点:
         * 1. 析构时自动调用堆管理器释放描述符
         * 2. 异常安全 - 析构函数不抛异常
         * 3. 弱引用检查 - 避免悬空指针
         *
         * 实现指导:
         * ```cpp
         * // 析构时自动调用SafeRelease
         * SafeRelease();
         * ```
         */
        ~DescriptorHandle() noexcept;

        // ========================================================================
        // 拷贝和移动语义
        // ========================================================================

        /**
         * @brief 禁用拷贝构造函数
         *
         * 教学要点: 描述符是独占资源，不允许拷贝避免重复释放
         */
        DescriptorHandle(const DescriptorHandle&) = delete;

        /**
         * @brief 禁用拷贝赋值操作符
         */
        DescriptorHandle& operator=(const DescriptorHandle&) = delete;

        /**
         * @brief 移动构造函数
         * @param other 被移动的对象
         *
         * 教学要点:
         * 1. 现代C++性能优化核心 - 移动而非拷贝
         * 2. 转移资源所有权，避免额外分配
         * 3. noexcept保证 - 提升STL容器性能
         *
         * 实现指导:
         * ```cpp
         * // 1. 调用MoveFrom辅助方法转移资源
         * MoveFrom(std::move(other));
         *
         * // 2. 或者直接实现：
         * m_allocation = std::move(other.m_allocation);
         * m_heapManager = std::move(other.m_heapManager);
         * m_ownsResource = other.m_ownsResource;
         *
         * // 3. 重置源对象为无效状态
         * other.Reset();  // 确保源对象不再拥有资源
         * ```
         */
        DescriptorHandle(DescriptorHandle&& other) noexcept;

        /**
         * @brief 移动赋值操作符
         * @param other 被移动的对象
         * @return 引用自身
         *
         * 实现指导:
         * ```cpp
         * // 1. 自我赋值检查
         * if (this != &other) {
         *     // 2. 先释放当前资源
         *     SafeRelease();
         *
         *     // 3. 转移新资源
         *     MoveFrom(std::move(other));
         * }
         * return *this;
         * ```
         */
        DescriptorHandle& operator=(DescriptorHandle&& other) noexcept;

        // ========================================================================
        // 资源访问接口
        // ========================================================================

        /**
         * @brief 获取CPU描述符句柄
         * @return CPU句柄，无效时返回空句柄
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const noexcept;

        /**
         * @brief 获取GPU描述符句柄
         * @return GPU句柄，无效时返回空句柄
         *
         * 教学要点: GPU句柄用于着色器内资源访问
         */
        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const noexcept;

        /**
         * @brief 获取堆内索引
         * @return 索引值，无效时返回UINT32_MAX
         */
        uint32_t GetHeapIndex() const noexcept;

        /**
         * @brief 获取堆类型
         * @return 堆类型
         */
        GlobalDescriptorHeapManager::HeapType GetHeapType() const noexcept;

        // ========================================================================
        // 状态查询接口
        // ========================================================================

        /**
         * @brief 检查句柄是否有效
         * @return 有效返回true，否则返回false
         *
         * 教学要点: 始终检查资源有效性，防止使用悬空句柄
         */
        bool IsValid() const noexcept;

        /**
         * @brief 检查是否拥有资源
         * @return 拥有返回true，否则返回false
         */
        bool OwnsResource() const noexcept;

        /**
         * @brief 检查堆管理器是否仍然存在
         * @return 存在返回true，否则返回false
         */
        bool IsHeapManagerAlive() const noexcept;

        // ========================================================================
        // 资源管理接口
        // ========================================================================

        /**
         * @brief 释放描述符 (提前释放)
         *
         * 教学要点:
         * 1. 提供显式释放接口，支持提前清理
         * 2. 释放后句柄变为无效状态
         * 3. 多次调用释放是安全的 (幂等操作)
         */
        void Release() noexcept;

        /**
         * @brief 重置句柄为无效状态
         *
         * 教学要点: 重置不会释放资源，只清除句柄状态
         */
        void Reset() noexcept;

        /**
         * @brief 分离资源所有权
         * @return 描述符分配信息
         *
         * 教学要点:
         * 1. 转移所有权给调用者，句柄不再管理资源
         * 2. 调用者负责手动释放资源
         * 3. 类似std::unique_ptr::release()的语义
         */
        GlobalDescriptorHeapManager::DescriptorAllocation Detach() noexcept;

        // ========================================================================
        // 比较操作符
        // ========================================================================

        /**
         * @brief 相等比较操作符
         * @param other 比较对象
         * @return 相等返回true
         *
         * 教学要点: 基于CPU句柄地址和索引比较
         */
        bool operator==(const DescriptorHandle& other) const noexcept;

        /**
         * @brief 不等比较操作符
         * @param other 比较对象
         * @return 不等返回true
         */
        bool operator!=(const DescriptorHandle& other) const noexcept;

        /**
         * @brief 小于比较操作符 (用于STL容器排序)
         * @param other 比较对象
         * @return 小于返回true
         */
        bool operator<(const DescriptorHandle& other) const noexcept;

        // ========================================================================
        // 转换操作符
        // ========================================================================

        /**
         * @brief 布尔转换操作符
         * @return 有效返回true，无效返回false
         *
         * 教学要点: 支持if(handle)语法，类似智能指针
         */
        explicit operator bool() const noexcept;

        // ========================================================================
        // 调试和诊断接口
        // ========================================================================

        /**
         * @brief 获取调试信息字符串
         * @return 调试信息
         */
        std::string GetDebugInfo() const;

        /**
         * @brief 验证句柄完整性
         * @return 完整性检查通过返回true
         *
         * 教学要点: 调试模式下的完整性检查，生产环境可禁用
         */
        bool ValidateIntegrity() const noexcept;

        // ========================================================================
        // 静态工厂方法
        // ========================================================================

        /**
         * @brief 创建无效句柄
         * @return 无效的描述符句柄
         *
         * 教学要点: 工厂方法模式，提供语义化的创建接口
         */
        static DescriptorHandle CreateInvalid() noexcept;

        /**
         * @brief 创建非拥有句柄 (用于观察现有描述符)
         * @param allocation 描述符分配信息
         * @return 非拥有的描述符句柄
         *
         * 教学要点: 观察者模式，不参与资源管理只提供访问
         */
        static DescriptorHandle CreateNonOwning(const GlobalDescriptorHeapManager::DescriptorAllocation& allocation) noexcept;

    public:
        // 常量定义
        static constexpr uint32_t INVALID_INDEX = UINT32_MAX;

    private:
        // ========================================================================
        // 私有辅助方法
        // ========================================================================

        /**
         * @brief 安全释放资源
         *
         * 实现指导:
         * ```cpp
         * // 1. 检查是否拥有资源且资源有效
         * if (m_ownsResource && m_allocation.isValid) {
         *     // 2. 检查堆管理器是否还存在 (弱引用检查)
         *     if (auto heapManager = m_heapManager.lock()) {
         *         // 3. 根据堆类型调用对应的释放方法
         *         if (m_allocation.heapType == GlobalDescriptorHeapManager::HeapType::CBV_SRV_UAV) {
         *             heapManager->FreeCbvSrvUav(m_allocation);
         *         } else {
         *             heapManager->FreeSampler(m_allocation);
         *         }
         *     }
         *
         *     // 4. 重置为无效状态
         *     m_allocation.Reset();
         *     m_ownsResource = false;
         * }
         * ```
         */
        void SafeRelease() noexcept;

        /**
         * @brief 移动资源从另一个句柄
         * @param other 源句柄
         *
         * 实现指导:
         * ```cpp
         * // 1. 转移所有成员变量
         * m_allocation = std::move(other.m_allocation);
         * m_heapManager = std::move(other.m_heapManager);
         * m_ownsResource = other.m_ownsResource;
         *
         * // 2. 重置源对象为无效状态，避免双重释放
         * other.m_allocation.Reset();
         * other.m_heapManager.reset();
         * other.m_ownsResource = false;
         * ```
         */
        void MoveFrom(DescriptorHandle&& other) noexcept;

        /**
         * @brief 交换两个句柄的内容
         * @param other 交换对象
         */
        void Swap(DescriptorHandle& other) noexcept;
    };

    // ========================================================================
    // 非成员函数
    // ========================================================================

    /**
     * @brief 交换两个句柄
     * @param lhs 左操作数
     * @param rhs 右操作数
     *
     * 教学要点: 支持std::swap特化，提升STL算法性能
     */
    void swap(DescriptorHandle& lhs, DescriptorHandle& rhs) noexcept;
} // namespace enigma::graphic

// ========================================================================
// std::hash特化 (用于std::unordered_map等)
// ========================================================================

namespace std
{
    template <>
    struct hash<enigma::graphic::DescriptorHandle>
    {
        size_t operator()(const enigma::graphic::DescriptorHandle& handle) const noexcept
        {
            // 基于CPU句柄地址和索引计算哈希
            auto cpuHandle = handle.GetCpuHandle();
            auto index     = handle.GetHeapIndex();

            size_t h1 = std::hash<size_t>{}(cpuHandle.ptr);
            size_t h2 = std::hash<uint32_t>{}(index);

            return h1 ^ (h2 << 1); // 简单的哈希组合
        }
    };
}

/**
 * @file DescriptorHandle.hpp
 * @brief 现代C++和DirectX 12最佳实践示例
 *
 * 这个文件展示了以下现代C++概念:
 * 1. RAII资源管理
 * 2. 移动语义和完美转发
 * 3. 智能指针模式
 * 4. 异常安全编程
 * 5. STL容器兼容性
 * 6. 工厂方法模式
 * 7. 观察者模式
 * 8. 操作符重载最佳实践
 *
 * DirectX 12特性展示:
 * 1. 描述符句柄管理
 * 2. CPU/GPU句柄区分
 * 3. 堆类型管理
 * 4. 资源生命周期
 */
