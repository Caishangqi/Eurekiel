#include "Engine/Graphic/Integration/DrawBindingHelper.hpp"

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Resource/BindlessRootSignature.hpp"
#include "Engine/Graphic/Resource/Buffer/BufferHelper.hpp"
#include "Engine/Graphic/Shader/Uniform/CustomImageManager.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"

namespace enigma::graphic
{
    void DrawBindingHelper::BindEngineBuffers(ID3D12GraphicsCommandList* cmdList, UniformManager* uniformMgr)
    {
        // [VALIDATION] Check parameters
        if (!cmdList || !uniformMgr)
        {
            LogWarn(core::LogRenderer, "[DrawBindingHelper] Invalid parameters in BindEngineBuffers");
            return;
        }

        // [ENGINE BUFFERS] Bind slots 0-14 using Root CBV
        // Technical Details:
        // - Slots 0-14 are Engine Reserved CBV slots
        // - Use SetGraphicsRootConstantBufferView for direct binding
        // - Skip slots with invalid GPU addresses (address == 0)
        for (uint32_t slot = 0; slot <= BufferHelper::MAX_ENGINE_RESERVED_SLOT; ++slot)
        {
            // Get GPU virtual address from UniformManager
            D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = uniformMgr->GetEngineBufferGPUAddress(slot);

            // [OPTIONAL BINDING] Skip if buffer not registered or not uploaded
            if (cbvAddress != 0)
            {
                cmdList->SetGraphicsRootConstantBufferView(slot, cbvAddress);
            }
        }
    }

    void DrawBindingHelper::BindCustomBufferTable(ID3D12GraphicsCommandList* cmdList, UniformManager* uniformMgr, uint32_t ringIndex)
    {
        // [VALIDATION] Check parameters
        if (!cmdList || !uniformMgr)
        {
            LogWarn(core::LogRenderer, "[DrawBindingHelper] Invalid parameters in BindCustomBufferTable");
            return;
        }

        // [FIX] [CUSTOM BUFFER TABLE] Bind slot 15 using Descriptor Table with Ring Index
        // Technical Details:
        // - Slot 15 is Custom Buffer Descriptor Table slot
        // - Custom buffers use space1 in HLSL (register(bN, space1))
        // - ringIndex selects the correct Descriptor Table slice for this draw
        // - Ring Descriptor Table architecture: Each Draw uses a different Descriptor Table offset
        D3D12_GPU_DESCRIPTOR_HANDLE customBufferTableHandle =
            uniformMgr->GetCustomBufferDescriptorTableGPUHandle(ringIndex);

        // [OPTIONAL BINDING] Bind if custom buffers exist
        if (customBufferTableHandle.ptr != 0)
        {
            cmdList->SetGraphicsRootDescriptorTable(
                BindlessRootSignature::ROOT_DESCRIPTOR_TABLE_CUSTOM,
                customBufferTableHandle
            );
        }
        else
        {
            // [INFO] No custom buffers registered, skip binding
            // This is normal when user shaders don't use custom buffers
        }
    }

    void DrawBindingHelper::PrepareCustomImages(CustomImageManager* customImgMgr)
    {
        // [VALIDATION] Check parameter
        if (!customImgMgr)
        {
            LogWarn(core::LogRenderer, "[DrawBindingHelper] Invalid parameter in PrepareCustomImages");
            return;
        }

        // [CUSTOM IMAGES] Upload pending custom image data to GPU
        // Technical Details:
        // - Deferred upload pattern: CPU updates are batched
        // - PrepareCustomImagesForDraw() uploads dirty textures to GPU
        // - Called before draw calls that may use custom images
        customImgMgr->PrepareCustomImagesForDraw();
    }
} // namespace enigma::graphic
