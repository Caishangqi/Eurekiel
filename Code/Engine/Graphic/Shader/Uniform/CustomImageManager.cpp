#include "CustomImageManager.hpp"
#include "UniformManager.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include <utility>

namespace enigma::graphic
{
    // ========================================================================
    // 构造与析构
    // ========================================================================

    CustomImageManager::CustomImageManager(
        UniformManager*       uniformManager,
        std::function<bool()> advancePassScopeCallback
    )
        : m_uniformManager(uniformManager)
        , m_advancePassScopeCallback(std::move(advancePassScopeCallback))
    {
        // [INIT] Initialize all texture pointers to nullptr
        m_textures.fill(nullptr);

        // [INIT] Initialize CustomImageUniforms data (the constructor has been automatically initialized to 0)
        // The constructors of m_currentCustomImage and m_lastDrawCustomImage will initialize all indexes to 0

        LogInfo(core::LogRenderer, "[CustomImageManager] Initialized with %d slots", MAX_CUSTOM_IMAGE_SLOTS);
    }

    // ========================================================================
    // Texture slot management API - set/get/clear
    // ========================================================================

    void CustomImageManager::SetCustomImage(int slotIndex, D12Texture* texture)
    {
        // [REQUIRED] 验证槽位索引有效性
        if (!IsValidSlotIndex(slotIndex))
        {
            ERROR_AND_DIE(Stringf("Invalid slot index: %d (valid range: 0-%d)",slotIndex, MAX_CUSTOM_IMAGE_SLOTS - 1))
        }

        const uint32_t bindlessIndex = ResolveBindlessIndex(texture);
        m_textures[slotIndex] = texture;

        if (m_currentCustomImage.GetCustomImageIndex(slotIndex) == bindlessIndex)
        {
            return;
        }

        m_currentCustomImage.SetCustomImageIndex(slotIndex, bindlessIndex);
        m_dirtySlots.set(static_cast<size_t>(slotIndex));
    }

    D12Texture* CustomImageManager::GetCustomImage(int slotIndex) const
    {
        // [REQUIRED] 验证槽位索引有效性
        if (!IsValidSlotIndex(slotIndex))
        {
            LogError(core::LogRenderer, "[CustomImageManager] Invalid slot index: %d (valid range: 0-%d)", slotIndex, MAX_CUSTOM_IMAGE_SLOTS - 1);
            return nullptr;
        }

        // [REQUIRED] 返回m_textures数组中的纹理指针
        return m_textures[slotIndex];
    }

    void CustomImageManager::ClearCustomImage(int slotIndex)
    {
        SetCustomImage(slotIndex, nullptr);
    }

    // ========================================================================
    // Draw之前调用的API - GPU上传和状态保存
    // ========================================================================

    void CustomImageManager::PrepareCustomImagesForDraw()
    {
        // [REQUIRED] 验证UniformManager指针有效性
        if (m_uniformManager == nullptr)
        {
            LogError(core::LogRenderer,
                     "[CustomImageManager] UniformManager is nullptr, cannot upload CustomImage data");
            return;
        }

        if (!m_hasCommittedSnapshotForCurrentScope)
        {
            CommitCurrentSnapshot();
            return;
        }

        if (m_dirtySlots.none())
        {
            return;
        }

        if (AreSnapshotsEqual(m_currentCustomImage, m_lastCommittedCustomImage))
        {
            m_dirtySlots.reset();
            return;
        }

        if (!m_advancePassScopeCallback)
        {
            ERROR_AND_DIE("CustomImageManager: conflicting snapshot requires pass-scope advance callback");
        }

        if (!m_advancePassScopeCallback())
        {
            ERROR_AND_DIE("CustomImageManager: failed to advance pass scope for conflicting snapshot");
        }

        CommitCurrentSnapshot();
    }

    // ========================================================================
    // 帧管理API - 帧开始时重置状态
    // ========================================================================

    void CustomImageManager::OnBeginFrame()
    {
        m_hasCommittedSnapshotForCurrentScope = false;
    }

    void CustomImageManager::OnPassScopeChanged()
    {
        m_hasCommittedSnapshotForCurrentScope = false;
    }

    uint32_t CustomImageManager::ResolveBindlessIndex(D12Texture* texture) const
    {
        if (texture != nullptr)
        {
            return texture->GetBindlessIndex();
        }

        auto defaultTexture = D3D12RenderSystem::GetDefaultWhiteTexture();
        if (!defaultTexture)
        {
            ERROR_AND_DIE("CustomImageManager: default white texture is unavailable")
        }

        return defaultTexture->GetBindlessIndex();
    }

    bool CustomImageManager::AreSnapshotsEqual(const CustomImageUniforms& lhs, const CustomImageUniforms& rhs) const
    {
        for (int slotIndex = 0; slotIndex < MAX_CUSTOM_IMAGE_SLOTS; ++slotIndex)
        {
            if (lhs.customImageIndices[slotIndex] != rhs.customImageIndices[slotIndex])
            {
                return false;
            }
        }

        return true;
    }

    void CustomImageManager::CommitCurrentSnapshot()
    {
        m_uniformManager->UploadBuffer<CustomImageUniforms>(m_currentCustomImage);
        m_lastCommittedCustomImage         = m_currentCustomImage;
        m_hasCommittedSnapshotForCurrentScope = true;
        m_dirtySlots.reset();
    }

    // ========================================================================
    // 调试支持 - 查询槽位状态
    // ========================================================================

    int CustomImageManager::GetUsedSlotCount() const
    {
        int count = 0;
        for (int i = 0; i < MAX_CUSTOM_IMAGE_SLOTS; ++i)
        {
            if (m_textures[i] != nullptr)
            {
                count++;
            }
        }
        return count;
    }

    bool CustomImageManager::IsSlotUsed(int slotIndex) const
    {
        // [REQUIRED] 验证槽位索引有效性
        if (!IsValidSlotIndex(slotIndex))
        {
            return false;
        }

        // [REQUIRED] 检查槽位是否已设置（非nullptr）
        return m_textures[slotIndex] != nullptr;
    }

    // ========================================================================
    // 教学要点总结
    // ========================================================================

    /**
     * 实现要点回顾：
     *
     * 1. **构造函数**：
     *    - 依赖注入UniformManager指针
     *    - 初始化所有纹理指针为nullptr
     *    - CustomImageUniform自动初始化为0
     *
     * 2. **SetCustomImage()**：
     *    - 只修改CPU端数据（m_currentCustomImage和m_textures）
     *    - 通过D12Texture::GetBindlessIndex()获取Bindless索引
     *    - 不触发GPU上传（延迟到PrepareCustomImagesForDraw）
     *    - 支持nullptr清除槽位
     *
     * 3. **PrepareCustomImagesForDraw()**：
     *    - 通过UniformManager::UploadBuffer()上传数据到GPU
     *    - 保存状态到m_lastDrawCustomImage（实现"复制上一次Draw的数据"）
     *    - m_currentCustomImage保持不变（自动保留数据）
     *
     * 4. **OnBeginFrame()**：
     *    - 当前实现为空（保留接口）
     *    - CustomImage槽位在帧间保持（不需要Flip）
     *
     * 5. **设计原则**：
     *    - 单一职责原则：只负责CustomImage槽位管理
     *    - 依赖倒置原则：依赖UniformManager抽象接口
     *    - 开闭原则：通过组合UniformManager扩展功能
     *    - 接口隔离原则：提供清晰的槽位管理API
     *
     * 6. **与UniformManager协作**：
     *    - SetCustomImage()：修改CPU端数据
     *    - PrepareCustomImagesForDraw()：通过UniformManager上传GPU数据
     *    - 延迟上传策略，优化性能
     *
     * 7. **错误处理**：
     *    - 槽位索引范围检查
     *    - nullptr纹理处理
     *    - UniformManager空指针检查
     *    - 详细的日志输出
     */
} // namespace enigma::graphic
