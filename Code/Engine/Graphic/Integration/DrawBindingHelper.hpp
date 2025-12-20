#pragma once

#include <cstdint>
#include <d3d12.h>

namespace enigma::graphic
{
    // Forward declarations
    class UniformManager;
    class CustomImageManager;

    /**
     * @class DrawBindingHelper
     * @brief Static helper class for resource binding in Draw functions
     *
     * Purpose:
     * - Encapsulates Engine Buffer binding logic (slots 0-14)
     * - Encapsulates Custom Buffer Descriptor Table binding logic (slot 15)
     * - Eliminates duplicate code across Draw/DrawIndexed/DrawInstanced functions
     *
     * Design Pattern:
     * - Pure static utility class (all methods static)
     * - No state (no member variables)
     * - Single responsibility (resource binding only)
     * - Follows BufferHelper pattern
     *
     * Technical Details:
     * - Engine Buffers: Use Root CBV (SetGraphicsRootConstantBufferView)
     * - Custom Buffer Table: Use Descriptor Table (SetGraphicsRootDescriptorTable)
     * - Slot 15: Custom Buffer Descriptor Table (BindlessRootSignature::ROOT_DESCRIPTOR_TABLE_CUSTOM)
     *
     * Usage Example:
     * ```cpp
     * // In Draw function
     * DrawBindingHelper::BindEngineBuffers(cmdList, uniformMgr);
     * DrawBindingHelper::BindCustomBufferTable(cmdList, uniformMgr, currentDrawCount);
     * DrawBindingHelper::PrepareCustomImages(customImgMgr);
     * ```
     */
    class DrawBindingHelper
    {
    public:
        /**
         * @brief Bind Engine Buffers (slots 0-14) using Root CBV
         * @param cmdList Graphics command list
         * @param uniformMgr Uniform manager instance
         *
         * Implementation:
         * - Iterates through slots 0-14
         * - Gets GPU virtual address from UniformManager
         * - Calls SetGraphicsRootConstantBufferView if address is valid
         *
         * @note Skips invalid slots (address == 0)
         */
        static void BindEngineBuffers(ID3D12GraphicsCommandList* cmdList, UniformManager* uniformMgr);

        /**
         * @brief Bind Custom Buffer Descriptor Table (slot 15)
         * @param cmdList Graphics command list
         * @param uniformMgr Uniform manager instance
         *
         * [REFACTORED] Removed ringIndex parameter - now obtained internally from uniformMgr
         *
         * Implementation:
         * - Gets current draw count from UniformManager
         * - Gets Custom Buffer Descriptor Table GPU handle with ringIndex offset
         * - Calls SetGraphicsRootDescriptorTable(15, handle) if valid
         * - Ring Descriptor Table architecture: each Draw uses different Descriptor Table offset
         *
         * @note Custom Buffer Table handle comes from UniformManager, not CustomImageManager
         * @note ringIndex is calculated as uniformMgr->GetCurrentDrawCount() internally
         */
        static void BindCustomBufferTable(ID3D12GraphicsCommandList* cmdList, UniformManager* uniformMgr);

        /**
         * @brief Prepare custom images for drawing
         * @param customImgMgr Custom image manager instance
         *
         * Implementation:
         * - Calls customImgMgr->PrepareCustomImagesForDraw()
         * - Uploads pending custom image data to GPU
         *
         * @note Should be called before draw calls that use custom images
         */
        static void PrepareCustomImages(CustomImageManager* customImgMgr);

    private:
        // [REQUIRED] Delete constructor to prevent instantiation
        DrawBindingHelper()  = delete;
        ~DrawBindingHelper() = delete;

        // [REQUIRED] Delete copy and move operations
        DrawBindingHelper(const DrawBindingHelper&)            = delete;
        DrawBindingHelper& operator=(const DrawBindingHelper&) = delete;
        DrawBindingHelper(DrawBindingHelper&&)                 = delete;
        DrawBindingHelper& operator=(DrawBindingHelper&&)      = delete;
    };
} // namespace enigma::graphic
