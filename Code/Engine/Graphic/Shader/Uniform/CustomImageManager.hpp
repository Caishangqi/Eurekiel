#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <functional>
#include "Engine/Graphic/Shader/Uniform/CustomImageUniforms.hpp"

namespace enigma::graphic
{
    class UniformManager;
    class D12Texture;

    /**
     * Tracks custom image slot bindings and uploads the active snapshot through
     * UniformManager when draw submission requires it.
     */
    class CustomImageManager
    {
    public:
        static constexpr int MAX_CUSTOM_IMAGE_SLOTS = 16;

        /**
         * Stores the upload dependency and the optional pass-scope advance hook.
         */
        explicit CustomImageManager(
            UniformManager*       uniformManager,
            std::function<bool()> advancePassScopeCallback = {}
        );

        ~CustomImageManager() = default;

        /**
         * Updates CPU-side slot state. GPU upload stays deferred until draw prep.
         */
        void SetCustomImage(int slotIndex, D12Texture* texture);

        // Returns the current texture bound to a slot, or nullptr if empty.
        D12Texture* GetCustomImage(int slotIndex) const;

        void ClearCustomImage(int slotIndex);

        // Uploads the current snapshot before draw submission.
        void PrepareCustomImagesForDraw();

        /**
         * Clears frame-scope commit state after the active frame slot is acquired.
         */
        void OnFrameSlotAcquired();

        // Clears pass-scope commit state when the renderer opens a new scope.
        void OnPassScopeChanged();

        int GetUsedSlotCount() const;

        bool IsSlotUsed(int slotIndex) const;

    private:
        UniformManager* m_uniformManager = nullptr;

        // CPU-side snapshot prepared for the next draw.
        CustomImageUniforms m_currentCustomImage;

        // Last snapshot uploaded for the current pass scope.
        CustomImageUniforms m_lastCommittedCustomImage;

        std::bitset<MAX_CUSTOM_IMAGE_SLOTS> m_dirtySlots;

        bool m_hasCommittedSnapshotForCurrentScope = false;

        // Non-owning texture pointers for each custom image slot.
        std::array<D12Texture*, MAX_CUSTOM_IMAGE_SLOTS> m_textures;

        std::function<bool()> m_advancePassScopeCallback;

        bool IsValidSlotIndex(int slotIndex) const
        {
            return slotIndex >= 0 && slotIndex < MAX_CUSTOM_IMAGE_SLOTS;
        }

        uint32_t ResolveBindlessIndex(D12Texture* texture) const;

        bool AreSnapshotsEqual(const CustomImageUniforms& lhs, const CustomImageUniforms& rhs) const;

        void CommitCurrentSnapshot();

        CustomImageManager(const CustomImageManager&)            = delete;
        CustomImageManager& operator=(const CustomImageManager&) = delete;
        CustomImageManager(CustomImageManager&&)                 = delete;
        CustomImageManager& operator=(CustomImageManager&&)      = delete;
    };
} // namespace enigma::graphic
