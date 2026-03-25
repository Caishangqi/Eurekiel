#pragma once

// ============================================================================
// MipmapGenerator.hpp - Pure static engine service for GPU mipmap generation
//
// Layer 0: Core service that dispatches per-mip downsampling via Compute Shader.
// Uses SM6.6 Bindless architecture with PerDispatch uniform strategy.
// ============================================================================

#include "MipmapConfig.hpp"
#include <cstdint>
#include <memory>

namespace enigma::graphic
{
    class D12Texture;
    class ShaderProgram;

    // CPU-side uniform struct (matches HLSL cbuffer MipGenUniforms layout)
    struct MipGenUniforms
    {
        uint32_t srcTextureIndex;      // Bindless SRV index
        uint32_t dstMipUavIndex;       // Bindless UAV index (temp)
        uint32_t srcMipLevel;          // Source mip to read from
        uint32_t samplerBindlessIndex; // Sampler bindless index (from SamplerProvider)
        uint32_t dstWidth;             // Destination mip width
        uint32_t dstHeight;            // Destination mip height
        uint32_t _pad[2];             // Padding to 32 bytes
    };
    static_assert(sizeof(MipGenUniforms) == 32, "MipGenUniforms must be 32 bytes to match HLSL cbuffer");

    /**
     * @class MipmapGenerator
     * @brief Pure static engine service for GPU mipmap generation
     *
     * Lazy-initialized on first GenerateMips() call:
     * - Compiles built-in box filter compute shader (MipmapGeneration.cs.hlsl)
     * - Registers MipGenUniforms with UniformManager (PerDispatch, slot b11, maxCount=16)
     *
     * Per-mip dispatch loop:
     * - Allocates temp UAV bindless index per mip level
     * - Uploads MipGenUniforms via PerDispatch strategy (auto-advances)
     * - Dispatches compute shader with ceil(w/8) x ceil(h/8) thread groups
     * - Inserts UAV barrier between mip levels
     * - Frees temp bindless index after each mip
     */
    class MipmapGenerator
    {
    public:
        MipmapGenerator() = delete;

        /**
         * @brief Generate mip chain for a texture using compute shader
         * @param texture Target texture (must have mipLevels > 1 and UAV flag)
         * @param config Mipmap generation configuration
         */
        static void GenerateMips(D12Texture* texture,
                                 const MipmapConfig& config = MipmapConfig::Default());

    private:
        // Lazy-initialized on first GenerateMips() call
        static std::unique_ptr<ShaderProgram> s_builtInComputeProgram;
        static bool s_initialized;

        static void ensureInitialized();
        static void compileBuiltInShader();
        static void registerUniformBuffer();
    };
} // namespace enigma::graphic
