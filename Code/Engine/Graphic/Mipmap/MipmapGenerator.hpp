#pragma once

// ============================================================================
// MipmapGenerator.hpp - Pure static engine service for GPU mipmap generation
//
// Layer 0: Core service that dispatches per-mip downsampling via Compute Shader.
// Uses SM6.6 Bindless architecture with PerDispatch uniform strategy.
//
// Initialization: Subscribes to RendererEvents::OnPipelineReady.
// When pipeline is ready, compiles all filter shader variants upfront.
// ============================================================================

#include "MipmapCommon.hpp"
#include "MipmapConfig.hpp"
#include <cstdint>
#include <memory>
#include <map>

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
     * Event-driven initialization via RendererEvents::OnPipelineReady:
     * - Registers MipGenUniforms with UniformManager (PerDispatch, slot b11)
     * - Compiles all filter shader variants from MipmapCommon::MIP_FILTER_SHADER_PATHS
     *
     * Per-mip dispatch loop:
     * - Uploads MipGenUniforms via PerDispatch strategy (auto-advances)
     * - Dispatches compute shader with ceil(w/8) x ceil(h/8) thread groups
     * - Inserts UAV barrier between mip levels
     */
    class MipmapGenerator
    {
    public:
        MipmapGenerator() = delete;

        /// Call once during engine startup (before OnPipelineReady fires).
        /// Subscribes to RendererEvents::OnPipelineReady for shader compilation.
        static void Initialize();

        /// Generate mip chain for a texture using compute shader.
        /// @param texture Target texture (must have mipLevels > 1 and UAV flag)
        /// @param config  Mipmap generation configuration (FilterMode or custom shader)
        static void GenerateMips(D12Texture* texture,
                                 const MipmapConfig& config = MipmapConfig::Default());

        /// Get cached shader for a MipFilterMode (nullptr if not yet compiled)
        static ShaderProgram* GetFilterShader(MipFilterMode mode);

    private:
        // MipFilterMode -> compiled ShaderProgram (populated on OnPipelineReady)
        static std::map<MipFilterMode, std::unique_ptr<ShaderProgram>> s_shaderCache;

        static bool s_initialized;

        // Event callback: compiles all shader variants + registers uniform buffer
        static void onPipelineReady();
        static void registerUniformBuffer();
        static void compileAllVariants();
    };
} // namespace enigma::graphic
