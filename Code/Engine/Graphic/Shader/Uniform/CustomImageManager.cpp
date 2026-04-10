#include "CustomImageManager.hpp"
#include "UniformManager.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include <utility>

namespace enigma::graphic
{
    CustomImageManager::CustomImageManager(
        UniformManager*       uniformManager,
        std::function<bool()> advancePassScopeCallback
    )
        : m_uniformManager(uniformManager)
        , m_advancePassScopeCallback(std::move(advancePassScopeCallback))
    {
        m_textures.fill(nullptr);

        LogInfo(core::LogRenderer, "[CustomImageManager] Initialized with %d slots", MAX_CUSTOM_IMAGE_SLOTS);
    }

    void CustomImageManager::SetCustomImage(int slotIndex, D12Texture* texture)
    {
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
        if (!IsValidSlotIndex(slotIndex))
        {
            LogError(core::LogRenderer, "[CustomImageManager] Invalid slot index: %d (valid range: 0-%d)", slotIndex, MAX_CUSTOM_IMAGE_SLOTS - 1);
            return nullptr;
        }

        return m_textures[slotIndex];
    }

    void CustomImageManager::ClearCustomImage(int slotIndex)
    {
        SetCustomImage(slotIndex, nullptr);
    }

    void CustomImageManager::PrepareCustomImagesForDraw()
    {
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

    void CustomImageManager::OnFrameSlotAcquired()
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
        m_lastCommittedCustomImage            = m_currentCustomImage;
        m_hasCommittedSnapshotForCurrentScope = true;
        m_dirtySlots.reset();
    }

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
        if (!IsValidSlotIndex(slotIndex))
        {
            return false;
        }

        return m_textures[slotIndex] != nullptr;
    }
} // namespace enigma::graphic
