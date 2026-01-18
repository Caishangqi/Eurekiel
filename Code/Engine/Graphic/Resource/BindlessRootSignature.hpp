#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp" // For MAX_CUSTOM_BUFFERS, MAX_DRAWS_PER_FRAME

namespace enigma::graphic
{
    /**
     * @brief BindlessRootSignature - Root CBV Architecture with Dynamic Sampler
     *
     * Key Features:
     * 1. Root CBV Architecture - 15 Root Descriptors (b0-b14), optimal performance
     * 2. SM6.6 Bindless: CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED for textures/buffers
     * 3. [NEW] Dynamic Sampler: SAMPLER_HEAP_DIRECTLY_INDEXED for runtime sampler config
     * 4. Global shared Root Signature: All PSOs use the same Root Signature
     *
     * Root Signature Layout (31 DWORDs = 48.4% budget):
     * ```
     * Slot 0-14:  Root CBV (30 DWORDs, 2 DWORDs each)
     *   - b0-b6:  Various uniforms
     *   - b7:     MatricesUniforms / SamplerIndicesBuffer
     *   - b8-b14: Reserved
     * Slot 15:    Custom Buffer Descriptor Table (1 DWORD) - space1 isolation
     * ```
     *
     * Dynamic Sampler Access (HLSL):
     * ```hlsl
     * // [NEW] Dynamic sampler via SamplerDescriptorHeap
     * SamplerState GetSampler(uint index) {
     *     return SamplerDescriptorHeap[index];
     * }
     * // Usage: texture.Sample(GetSampler(samplerIndices.linear), uv);
     * ```
     *
     * Architecture Benefits:
     * - Root CBV: Optimal GPU hardware path
     * - Dynamic Sampler: Runtime configuration via SetSamplerState API
     * - No static sampler limitations
     */
    class BindlessRootSignature final
    {
    public:
        /**
         * @brief Root Signature Layout Enum (31 DWORDs total budget)
         *
         * Key Points:
         * 1. Root CBV: 2 DWORDs each (64-bit GPU virtual address)
         * 2. Root Constants: 1 DWORD each
         * 3. Descriptor Table: 1 DWORD (GPU Heap offset)
         * 4. DX12 Limit: Max 64 DWORDs (256 bytes)
         */
        enum RootParameterIndex : uint32_t
        {
            // Root CBV槽位 (Slot 0-14, 28 DWORDs)
            ROOT_CBV_UNDEFINE_0 = 0,
            ROOT_CBV_PER_OBJECT = 1, // PerObjectUniforms
            ROOT_CBV_CUSTOM_IMAGE = 2, // CustomImageUniforms
            ROOT_CBV_UNDEFINE_3 = 3,
            ROOT_CBV_UNDEFINE_4 = 4,
            ROOT_CBV_UNDEFINE_5 = 5,
            ROOT_CBV_UNDEFINE_6 = 6,
            ROOT_CBV_MATRICES = 7, // b7  - MatricesUniforms
            ROOT_CBV_UNDEFINE_8 = 8, // SamplerIndicesUniforms
            ROOT_CBV_UNDEFINE_9 = 9,
            ROOT_CBV_UNDEFINE_10 = 10,
            ROOT_CBV_UNDEFINE_11 = 11,
            ROOT_CBV_UNDEFINE_12 = 12,
            ROOT_CBV_UNDEFINE_13 = 13,

            ROOT_CBV_UNDEFINE_14 = 14,
            ROOT_DESCRIPTOR_TABLE_CUSTOM = 15, // Descriptor Table for custom uniform

            ROOT_PARAMETER_COUNT = 16
        };

        /**
         * @brief Custom Buffer system constants (referenced from EnigmaGraphicCommon.hpp)
         *
         * [REFACTORED] These constants are now defined in EnigmaGraphicCommon.hpp
         * and aliased here for backward compatibility.
         *
         * [FIX] Ring Descriptor Table architecture:
         * - MAX_RING_FRAMES: Maximum number of Draws in a single frame
         * - Each Draw uses a different Descriptor Table copy
         * - Total number of Descriptors = MAX_RING_FRAMES * MAX_CUSTOM_BUFFERS
         */
        // [REFACTORED] Use constants from EnigmaGraphicCommon.hpp
        static constexpr uint32_t MAX_CUSTOM_BUFFERS = enigma::graphic::MAX_CUSTOM_BUFFERS;
        static constexpr uint32_t MAX_RING_FRAMES    = enigma::graphic::MAX_DRAWS_PER_FRAME; // Alias for backward compatibility

        /**
         * @brief Root Constants配置
         */
        static constexpr uint32_t ROOT_CONSTANTS_NUM_32BIT_VALUES = 1; // NoiseTexture index
        static constexpr uint32_t ROOT_CONSTANTS_SIZE_BYTES       = 4; // 1 DWORD = 4 bytes

        /**
         * @brief Root Signature预算统计
         */
        static constexpr uint32_t ROOT_SIGNATURE_DWORD_COUNT = 31; // 28 + 1 + 1 + 1 [NEW]
        static constexpr uint32_t ROOT_SIGNATURE_MAX_DWORDS  = 64; // DX12限制
        static constexpr float    ROOT_SIGNATURE_BUDGET_USED = 48.4f; // 31/64 = 48.4% [NEW]

        // [NEW] 编译期预算验证
        static_assert(ROOT_SIGNATURE_DWORD_COUNT <= ROOT_SIGNATURE_MAX_DWORDS,
                      "Root Signature exceeds 64 DWORDs limit");

    public:
        /**
         * @brief Constructor
         */
        BindlessRootSignature();

        /**
         * @brief Destructor - RAII auto release
         */
        ~BindlessRootSignature() = default;

        // Disable copy, support move
        BindlessRootSignature(const BindlessRootSignature&)            = delete;
        BindlessRootSignature& operator=(const BindlessRootSignature&) = delete;
        BindlessRootSignature(BindlessRootSignature&&)                 = default;
        BindlessRootSignature& operator=(BindlessRootSignature&&)      = default;

        // ========================================================================
        // Lifecycle Management
        // ========================================================================

        /**
         * @brief Initialize Root Signature
         * @return true on success, false on failure
         *
         * Key Steps:
         * 1. Get device from D3D12RenderSystem::GetDevice()
         * 2. Create Root Signature desc with Root CBV parameters
         * 3. Set SM6.6 Bindless flags:
         *    - D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
         *    - D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED [NEW]
         * 4. Serialize and create Root Signature object
         */
        bool Initialize();

        /**
         * @brief Release all resources
         */
        void Shutdown();

        /**
         * @brief Check if initialized
         */
        bool IsInitialized() const { return m_initialized; }

        // ========================================================================
        // Root Signature Access Interface
        // ========================================================================

        /**
         * @brief Get Root Signature object
         * @return Root Signature pointer (for PSO creation and command list binding)
         */
        ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }

        /**
         * @brief Bind Root Signature to command list
         * @param commandList Command list
         *
         * Notes:
         * 1. Only need to bind once per frame (not per Pass)
         * 2. Usually called in BeginFrame
         * 3. Global shared - all PSOs use the same Root Signature
         */
        void BindToCommandList(ID3D12GraphicsCommandList* commandList) const;

    private:
        // Root Signature object
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

        // Initialization state
        bool m_initialized = false;

        // ========================================================================
        // Private Helper Methods
        // ========================================================================

        /**
         * @brief Create Root Signature
         * @param device DX12 device
         * @return true on success
         *
         * Implementation:
         * 1. Create Root Parameter array (16 slots)
         * 2. Set Root Signature desc with SM6.6 flags
         * 3. No static samplers (dynamic sampler system)
         * 4. Serialize and create Root Signature
         */
        bool CreateRootSignature(ID3D12Device* device);
    };
} // namespace enigma::graphic
