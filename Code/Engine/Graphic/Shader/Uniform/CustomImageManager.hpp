#pragma once

#include <array>
#include <cstdint>
#include "Engine/Graphic/Shader/Uniform/CustomImageUniform.hpp"

namespace enigma::graphic
{
    // Forward declarations
    class UniformManager;
    class D12Texture;

    // ============================================================================
    // CustomImageManager - 自定义材质槽位管理器
    // ============================================================================

    /**
     * @class CustomImageManager
     * @brief 管理16个CustomImage槽位的纹理绑定和GPU上传
     *
     * **架构定位**:
     * - 职责: 管理CustomImage槽位的纹理绑定和状态追踪
     * - 协作: 依赖UniformManager进行GPU Buffer上传
     * - 模式: Manager模式（有状态管理，非Helper类）
     *
     * **核心职责**:
     * 1. **纹理槽位管理** - 设置/获取/清除16个CustomImage槽位
     * 2. **状态追踪** - 维护当前和上一次Draw的CustomImage数据
     * 3. **GPU上传协调** - 在Draw完成时通过UniformManager上传数据
     * 4. **帧管理** - 在帧开始时重置状态
     *
     * **与UniformManager协作**:
     * - CustomImageManager负责管理CustomImage槽位状态
     * - UniformManager负责GPU Buffer的创建和上传
     * - 通过依赖注入实现松耦合（符合依赖倒置原则）
     *
     * **设计原则**:
     * - 单一职责原则: 只负责CustomImage槽位管理
     * - 开闭原则: 通过组合UniformManager扩展功能
     * - 依赖倒置原则: 依赖UniformManager抽象接口
     * - 接口隔离原则: 提供清晰的槽位管理API
     *
     * **使用示例**:
     * ```cpp
     * // 初始化（在RendererSubsystem中）
     * m_customImageManager = std::make_unique<CustomImageManager>(m_uniformManager.get());
     *
     * // 设置自定义材质
     * m_customImageManager->SetCustomImage(0, myTexture);  // customImage0
     * m_customImageManager->SetCustomImage(1, anotherTex); // customImage1
     *
     * // Draw之前上传数据
     * m_customImageManager->PrepareCustomImagesForDraw();
     *
     * // 帧开始时重置
     * m_customImageManager->OnBeginFrame();
     * ```
     *
     * **内存布局**:
     * - m_currentCustomImage: 64 bytes (CustomImageUniform)
     * - m_lastDrawCustomImage: 64 bytes (CustomImageUniform)
     * - m_textures: 128 bytes (16 × 8 bytes指针)
     * - m_uniformManager: 8 bytes (指针)
     * - 总计: ~264 bytes
     *
     * **与RenderTargetManager对比**:
     * - RenderTargetManager: 管理16个RenderTarget + Flip机制
     * - CustomImageManager: 管理16个CustomImage槽位 + 状态追踪
     * - 相似点: 都是Manager模式，都管理16个槽位
     * - 不同点: CustomImage无需Flip，只需状态复制
     */
    class CustomImageManager
    {
    public:
        // ========================================================================
        // 常量定义 - CustomImage槽位数量
        // ========================================================================

        static constexpr int MAX_CUSTOM_IMAGE_SLOTS = 16; // 最大CustomImage槽位数量

        // ========================================================================
        // 构造与析构
        // ========================================================================

        /**
         * @brief 构造CustomImageManager并注入UniformManager依赖
         * @param uniformManager UniformManager指针（依赖注入）
         *
         * 教学要点:
         * - 使用explicit防止隐式转换
         * - 通过依赖注入实现松耦合
         * - 初始化所有成员变量为默认状态
         * - 所有纹理指针初始化为nullptr
         *
         * 设计原则:
         * - 依赖倒置原则: 依赖UniformManager抽象接口
         * - 单一职责原则: 只负责CustomImage管理
         *
         * @note uniformManager不能为nullptr，否则无法上传数据
         */
        explicit CustomImageManager(UniformManager* uniformManager);

        /**
         * @brief 默认析构函数
         *
         * 教学要点:
         * - 纹理指针由外部管理生命周期（非所有权）
         * - CustomImageUniform是值类型，自动析构
         * - 无需手动释放资源
         */
        ~CustomImageManager() = default;

        // ========================================================================
        // 纹理槽位管理API - 设置/获取/清除
        // ========================================================================

        /**
         * @brief 设置指定槽位的纹理
         * @param slotIndex 槽位索引 [0-15]
         * @param texture 纹理指针（可以为nullptr表示清除）
         *
         * 教学要点:
         * - 只修改CPU端数据（m_currentCustomImage和m_textures）
         * - 不触发GPU上传（延迟到PrepareCustomImagesForDraw）
         * - 支持运行时动态更新任意槽位
         * - 范围检查确保安全
         *
         * 业务逻辑:
         * 1. 验证slotIndex有效性 [0-15]
         * 2. 保存纹理指针到m_textures[slotIndex]
         * 3. 如果texture非空，获取Bindless索引并设置到m_currentCustomImage
         * 4. 如果texture为空，清除槽位（索引设为0）
         *
         * 使用示例:
         * ```cpp
         * // 设置customImage0
         * customImageMgr->SetCustomImage(0, myTexture);
         *
         * // 清除customImage1
         * customImageMgr->SetCustomImage(1, nullptr);
         * ```
         *
         * @note 此方法不触发GPU上传，需要调用PrepareCustomImagesForDraw()
         */
        void SetCustomImage(int slotIndex, D12Texture* texture);

        /**
         * @brief 获取指定槽位的纹理指针
         * @param slotIndex 槽位索引 [0-15]
         * @return D12Texture* 纹理指针（可能为nullptr）
         *
         * 教学要点:
         * - const方法，不修改对象状态
         * - 返回原始指针（非所有权）
         * - 范围检查确保安全
         *
         * 使用示例:
         * ```cpp
         * D12Texture* tex = customImageMgr->GetCustomImage(0);
         * if (tex != nullptr) {
         *     // 使用纹理...
         * }
         * ```
         */
        D12Texture* GetCustomImage(int slotIndex) const;

        /**
         * @brief 清除指定槽位的纹理
         * @param slotIndex 槽位索引 [0-15]
         *
         * 教学要点:
         * - 等价于SetCustomImage(slotIndex, nullptr)
         * - 提供更清晰的语义
         * - 将槽位索引设为0（无效索引）
         *
         * 使用示例:
         * ```cpp
         * customImageMgr->ClearCustomImage(0);  // 清空customImage0
         * ```
         */
        void ClearCustomImage(int slotIndex);

        // ========================================================================
        // Draw时调用的API - GPU上传和状态保存
        // ========================================================================

        /**
         * @brief Draw之前调用，上传CustomImage数据到GPU并准备绘制
         *
         * 教学要点:
         * - 在每次Draw Call之前调用（在Root Signature绑定之后）
         * - 通过UniformManager上传m_currentCustomImage到GPU
         * - 保存当前状态到m_lastDrawCustomImage（实现"复制上一次Draw的数据"）
         * - 不修改m_currentCustomImage（保留数据供下次Draw使用）
         *
         * 业务逻辑:
         * 1. 调用m_uniformManager->UploadBuffer(m_currentCustomImage)上传数据
         * 2. 复制m_currentCustomImage到m_lastDrawCustomImage
         * 3. m_currentCustomImage保持不变（无需显式复制）
         *
         * 调用时机:
         * - 在DrawVertexArray()中，Draw Call之前调用
         * - 在Root Signature绑定之后调用
         *
         * 使用示例:
         * ```cpp
         * // 在DrawVertexArray中
         * void DrawVertexArray(...) {
         *     // ... 准备PSO和绑定资源 ...
         *     m_customImageManager->PrepareCustomImagesForDraw();  // [FIX] Draw之前上传
         *     cmdList->DrawInstanced(...);  // Draw Call
         * }
         * ```
         *
         * @note 必须在UniformManager注册CustomImageUniform后才能调用
         */
        void PrepareCustomImagesForDraw();

        // ========================================================================
        // 帧管理API - 帧开始时重置状态
        // ========================================================================

        /**
         * @brief 帧开始时调用，重置状态
         *
         * 教学要点:
         * - 在每帧开始时调用（BeginFrame）
         * - 可选的状态重置逻辑
         * - 当前实现为空（保留接口供未来扩展）
         *
         * 设计考虑:
         * - CustomImage槽位在帧间保持（不像RenderTarget需要Flip）
         * - 如果需要帧间清理，可以在此实现
         * - 保留接口符合开闭原则
         *
         * 使用示例:
         * ```cpp
         * // 在RendererSubsystem::BeginFrame中
         * void BeginFrame() {
         *     m_customImageManager->OnBeginFrame();
         *     // ... 其他帧开始逻辑 ...
         * }
         * ```
         */
        void OnBeginFrame();

        // ========================================================================
        // 调试支持 - 查询槽位状态
        // ========================================================================

        /**
         * @brief 获取已使用的槽位数量
         * @return int 非空槽位数量 [0-16]
         *
         * 教学要点:
         * - 遍历m_textures数组统计非nullptr指针
         * - 用于调试和性能分析
         *
         * 使用示例:
         * ```cpp
         * int usedSlots = customImageMgr->GetUsedSlotCount();
         * LogInfo("CustomImage used slots: %d", usedSlots);
         * ```
         */
        int GetUsedSlotCount() const;

        /**
         * @brief 检查指定槽位是否已设置
         * @param slotIndex 槽位索引 [0-15]
         * @return bool true=已设置，false=未设置
         *
         * 使用示例:
         * ```cpp
         * if (customImageMgr->IsSlotUsed(0)) {
         *     // customImage0已设置...
         * }
         * ```
         */
        bool IsSlotUsed(int slotIndex) const;

    private:
        // ========================================================================
        // 私有成员变量
        // ========================================================================

        /**
         * @brief UniformManager指针 - 用于GPU Buffer上传
         *
         * 教学要点:
         * - 依赖注入，不拥有所有权
         * - 通过UniformManager::UploadBuffer()上传数据
         * - 符合依赖倒置原则
         */
        UniformManager* m_uniformManager;

        /**
         * @brief 当前准备Draw的CustomImage数据
         *
         * 教学要点:
         * - 通过SetCustomImage()修改
         * - 在PrepareCustomImagesForDraw()时上传到GPU
         * - 保留数据供下次Draw使用（无需显式复制）
         */
        CustomImageUniform m_currentCustomImage;

        /**
         * @brief 上一次Draw的CustomImage数据
         *
         * 教学要点:
         * - 在PrepareCustomImagesForDraw()时从m_currentCustomImage复制
         * - 实现"复制上一次Draw的数据"机制
         * - 用于调试和状态追踪
         */
        CustomImageUniform m_lastDrawCustomImage;

        /**
         * @brief 纹理指针数组 - 16个CustomImage槽位
         *
         * 教学要点:
         * - 使用std::array确保固定大小
         * - 存储原始指针（非所有权）
         * - 通过SetCustomImage()设置
         * - nullptr表示槽位未使用
         */
        std::array<D12Texture*, MAX_CUSTOM_IMAGE_SLOTS> m_textures;

        // ========================================================================
        // 私有辅助方法
        // ========================================================================

        /**
         * @brief 验证槽位索引有效性
         * @param slotIndex 槽位索引
         * @return bool true=有效 [0-15]，false=无效
         *
         * 教学要点:
         * - 内部辅助函数，简化范围检查
         * - 避免重复代码
         */
        bool IsValidSlotIndex(int slotIndex) const
        {
            return slotIndex >= 0 && slotIndex < MAX_CUSTOM_IMAGE_SLOTS;
        }

        // ========================================================================
        // 禁用拷贝和移动 (遵循RAII原则)
        // ========================================================================

        CustomImageManager(const CustomImageManager&)            = delete;
        CustomImageManager& operator=(const CustomImageManager&) = delete;
        CustomImageManager(CustomImageManager&&)                 = delete;
        CustomImageManager& operator=(CustomImageManager&&)      = delete;

        // ========================================================================
        // 教学要点总结
        // ========================================================================

        /**
         * 1. **Manager模式**:
         *    - 有状态管理（m_currentCustomImage, m_lastDrawCustomImage, m_textures）
         *    - 不是Helper类（Helper类必须无状态）
         *    - 遵循RenderTargetManager的设计模式
         *
         * 2. **依赖倒置原则**:
         *    - 依赖UniformManager抽象接口
         *    - 通过依赖注入实现松耦合
         *    - 不直接管理GPU Buffer
         *
         * 3. **单一职责原则**:
         *    - 只负责CustomImage槽位管理
         *    - GPU上传委托给UniformManager
         *    - 不涉及渲染逻辑
         *
         * 4. **状态管理**:
         *    - m_currentCustomImage: 当前准备Draw的数据
         *    - m_lastDrawCustomImage: 上一次Draw的数据
         *    - m_textures: 纹理指针数组（非所有权）
         *
         * 5. **与UniformManager协作**:
         *    - SetCustomImage(): 只修改CPU端数据
         *    - PrepareCustomImagesForDraw(): 通过UniformManager上传GPU数据
         *    - 延迟上传策略，优化性能
         *
         * 6. **与RenderTargetManager对比**:
         *    - 相似: Manager模式，16个槽位，状态管理
         *    - 不同: 无Flip机制，只需状态复制
         *    - 简化: 无需BufferFlipState，无需Main/Alt纹理
         */
    };
} // namespace enigma::graphic
