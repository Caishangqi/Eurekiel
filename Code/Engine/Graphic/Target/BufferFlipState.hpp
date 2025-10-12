#pragma once

#include <bitset>
#include <cstdint>
#include <type_traits>

namespace enigma::graphic
{
    // ============================================================================
    // BufferFlipState<N> - 通用 Main/Alt 双缓冲翻转状态管理（模板类）
    // ============================================================================

    /**
     * @brief 管理任意数量资源的 Main/Alt 翻转状态
     * @tparam N 资源数量（RenderTarget 为 16, ShadowColor 为 8）
     *
     * **Iris 架构核心**:
     * - RenderTargets.java 和 ShadowRenderTargets.java 都使用 boolean[] flipped
     * - 我们使用 std::bitset<N> 统一实现，提高性能和内存效率
     *
     * **设计原则**:
     * - **DRY 原则**: 避免在 RenderTargetManager 和 ShadowRenderTargetManager 中重复代码
     * - **单一职责**: 专注于翻转状态管理，不关心资源类型
     * - **模板设计**: 支持任意数量的资源翻转（编译期优化）
     *
     * **内存布局优化**:
     * - N=16: std::bitset<16> 仅 2 字节
     * - N=8:  std::bitset<8>  仅 1 字节
     * - O(1) 复杂度的翻转操作
     *
     * **业务逻辑**:
     * - false = 读 Main 写 Alt
     * - true  = 读 Alt 写 Main
     * - 每帧结束后调用 Flip() 切换状态
     *
     * **使用示例**:
     * @code
     * // RenderTargetManager (16 个 colortex)
     * BufferFlipState<16> colorFlipState;
     * colorFlipState.Flip(0); // 翻转 colortex0
     *
     * // ShadowRenderTargetManager (8 个 shadowcolor)
     * BufferFlipState<8> shadowFlipState;
     * shadowFlipState.FlipAll(); // 翻转所有 shadowcolor
     * @endcode
     *
     * **Iris 源码对应**:
     * - RenderTargets.java:78-85       → BufferFlipState<16>
     * - ShadowRenderTargets.java:78-85 → BufferFlipState<8>
     */
    template <size_t N>
    class BufferFlipState
    {
    public:
        /**
         * @brief 默认构造函数，初始化所有资源为未翻转状态
         *
         * 教学要点: std::bitset 默认初始化为全 0 (false)
         */
        BufferFlipState() = default;

        /**
         * @brief 翻转指定资源的 Main/Alt 状态
         * @param index 资源索引 [0, N-1]
         *
         * **业务逻辑**:
         * - false → true (读 Main 写 Alt → 读 Alt 写 Main)
         * - true → false (读 Alt 写 Main → 读 Main 写 Alt)
         *
         * 教学要点: 使用 std::bitset::flip() 实现 O(1) 翻转
         *
         * **对应 Iris**:
         * @code{.java}
         * public void flip(int target) {
         *     flipped[target] = !flipped[target];
         * }
         * @endcode
         */
        void Flip(int index)
        {
            if (IsValidIndex(index))
            {
                m_flipped.flip(index);
            }
        }

        /**
         * @brief 检查指定资源是否已翻转
         * @param index 资源索引 [0, N-1]
         * @return false = 读 Main 写 Alt, true = 读 Alt 写 Main
         *
         * **对应 Iris**:
         * @code{.java}
         * public boolean isFlipped(int target) {
         *     return flipped[target];
         * }
         * @endcode
         */
        bool IsFlipped(int index) const
        {
            return IsValidIndex(index) ? m_flipped.test(index) : false;
        }

        /**
         * @brief 重置所有资源为初始状态 (读 Main 写 Alt)
         *
         * 教学要点: std::bitset::reset() 将所有位置为 false
         */
        void Reset()
        {
            m_flipped.reset(); // 全部置为 false
        }

        /**
         * @brief 批量翻转所有资源
         *
         * **使用场景**: 每帧结束时调用，切换所有资源的 Main/Alt 状态
         *
         * 教学要点: std::bitset::flip() 无参数时翻转所有位
         */
        void FlipAll()
        {
            m_flipped.flip(); // 翻转所有位
        }

        /**
         * @brief 获取原始 bitset 用于 GPU 上传
         * @return 根据 N 返回合适大小的整数类型
         *
         * **返回类型**:
         * - N <= 16: uint16_t (2 字节)
         * - N <= 32: uint32_t (4 字节)
         * - N <= 64: uint64_t (8 字节)
         *
         * 教学要点: C++20 std::bitset::to_ullong() 保证二进制兼容性
         *
         * **GPU 上传示例**:
         * @code
         * uint16_t flipMask = flipState.ToUInt();
         * // 将 flipMask 上传到 GPU 常量缓冲
         * @endcode
         */
        auto ToUInt() const
        {
            if constexpr (N <= 16)
            {
                return static_cast<uint16_t>(m_flipped.to_ullong());
            }
            else if constexpr (N <= 32)
            {
                return static_cast<uint32_t>(m_flipped.to_ullong());
            }
            else
            {
                return static_cast<uint64_t>(m_flipped.to_ullong());
            }
        }

        /**
         * @brief 获取资源数量
         * @return 模板参数 N
         */
        static constexpr size_t GetSize() { return N; }

    private:
        /**
         * @brief 验证索引有效性 (内部辅助函数)
         * @param index 资源索引
         * @return true if valid [0, N-1]
         */
        bool IsValidIndex(int index) const
        {
            return index >= 0 && index < static_cast<int>(N);
        }

        std::bitset<N> m_flipped; // N 个资源的翻转状态

        // ========================================================================
        // 教学要点总结
        // ========================================================================

        /**
         * 1. **模板设计优势**:
         *    - 编译期确定大小，零运行时开销
         *    - 支持任意数量的资源翻转（RenderTarget=16, ShadowColor=8）
         *    - 类型安全，避免数组越界
         *
         * 2. **std::bitset 极致优化**:
         *    - N=16: 仅 2 字节内存占用
         *    - N=8:  仅 1 字节内存占用
         *    - O(1) 翻转操作，无分支预测失败
         *
         * 3. **DRY 原则实践**:
         *    - RenderTargetManager 和 ShadowRenderTargetManager 共享代码
         *    - 避免 bug 修复和优化的双重维护
         *    - 符合 SOLID 原则中的单一职责原则
         *
         * 4. **Iris 架构对应**:
         *    - Iris: boolean[] flipped (Java 数组)
         *    - DX12: std::bitset<N> (C++ 模板 + 位运算优化)
         *    - 功能等价，性能更优
         *
         * 5. **GPU 上传支持**:
         *    - ToUInt() 返回适当大小的整数
         *    - 可直接上传到 GPU 常量缓冲
         *    - 支持 HLSL 位运算 (flipMask & (1 << index))
         */
    };

    // ============================================================================
    // 类型别名 - 常用实例化
    // ============================================================================

    /**
     * @brief RenderTarget 翻转状态（16 个 colortex）
     *
     * 使用场景: RenderTargetManager
     */
    using RenderTargetFlipState = BufferFlipState<16>;

    /**
     * @brief Shadow 翻转状态（8 个 shadowcolor）
     *
     * 使用场景: ShadowRenderTargetManager
     */
    using ShadowFlipState = BufferFlipState<8>;

    // ============================================================================
    // 静态断言 - 编译期验证
    // ============================================================================

    // 验证内存布局符合预期
    static_assert(sizeof(RenderTargetFlipState) <= 16, "RenderTargetFlipState should be <= 16 bytes");
    static_assert(sizeof(ShadowFlipState) <= 8, "ShadowFlipState should be <= 2 bytes");

    // 验证模板实例化正确
    static_assert(RenderTargetFlipState::GetSize() == 16, "RenderTargetFlipState should manage 16 resources");
    static_assert(ShadowFlipState::GetSize() == 8, "ShadowFlipState should manage 8 resources");
} // namespace enigma::graphic
