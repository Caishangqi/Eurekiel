#include "CustomImageManager.hpp"
#include "UniformManager.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // 构造与析构
    // ========================================================================

    CustomImageManager::CustomImageManager(UniformManager* uniformManager)
        : m_uniformManager(uniformManager)
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
        m_textures[slotIndex] = texture;

        if (texture != nullptr)
        {
            uint32_t bindlessIndex = texture->GetBindlessIndex();
            m_currentCustomImage.SetCustomImageIndex(slotIndex, bindlessIndex);
        }
        else
        {
            // [FIX] 使用默认白色纹理而非index=0
            auto defaultTexture = D3D12RenderSystem::GetDefaultWhiteTexture();
            if (defaultTexture)
            {
                uint32_t bindlessIndex = defaultTexture->GetBindlessIndex();
                m_currentCustomImage.SetCustomImageIndex(slotIndex, bindlessIndex);
            }
            else
            {
                m_currentCustomImage.SetCustomImageIndex(slotIndex, 0);
                ERROR_AND_DIE("Fail to Create and Bind Texture")
            }
        }
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

        // [REQUIRED] 通过UniformManager上传m_currentCustomImage到GPU
        // [IMPORTANT] 使用模板方法UploadBuffer<CustomImageUniforms>()
        m_uniformManager->UploadBuffer<CustomImageUniforms>(m_currentCustomImage);

        // [REQUIRED] 保存当前状态到m_lastDrawCustomImage
        // [IMPORTANT] 实现"复制上一次Draw的数据"机制
        m_lastDrawCustomImage = m_currentCustomImage;
    }

    // ========================================================================
    // 帧管理API - 帧开始时重置状态
    // ========================================================================

    void CustomImageManager::OnBeginFrame()
    {
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
