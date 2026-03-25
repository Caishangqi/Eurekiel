#include "MipmapGenerator.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Shader/Program/ShaderProgram.hpp"
#include "Engine/Graphic/Shader/PSO/PSOManager.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Shader/Compiler/DXCCompiler.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Resource/BindlessRootSignature.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

// ============================================================================
// MipmapGenerator.cpp - GPU mipmap generation via compute shader
// ============================================================================

namespace enigma::graphic
{
    // Static member definitions
    std::unique_ptr<ShaderProgram> MipmapGenerator::s_builtInComputeProgram = nullptr;
    bool MipmapGenerator::s_initialized = false;

    // ========================================================================
    // Lazy Initialization
    // ========================================================================

    void MipmapGenerator::ensureInitialized()
    {
        if (s_initialized)
            return;

        compileBuiltInShader();
        registerUniformBuffer();
        s_initialized = true;
    }

    void MipmapGenerator::compileBuiltInShader()
    {
        DXCCompiler compiler;
        if (!compiler.Initialize())
        {
            ERROR_AND_DIE("MipmapGenerator: Failed to initialize DXC compiler");
        }

        DXCCompiler::CompileOptions options;
        options.entryPoint = "main";
        options.target = "cs_6_6";
        options.enableOptimization = true;
        options.enableDebugInfo = false;
        options.enable16BitTypes = true;

        // Add include paths for shader resolution
        options.includePaths.push_back(L".enigma/assets/engine/shaders/");
        options.includePaths.push_back(L".enigma/assets/engine/shaders/core/");

        // Compile from the shader file (relative to working directory = Run/)
        DXCCompiler::CompileResult result = compiler.CompileShaderFromFile(
            L".enigma/assets/engine/shaders/core/mipmapGeneration.cs.hlsl",
            options);

        if (!result.success)
        {
            LogError("MipmapGenerator", "Compute shader compile error: %s", result.errorMessage.c_str());
            ERROR_AND_DIE("MipmapGenerator: built-in compute shader compilation failed");
        }

        // Create CompiledShader and wrap in ShaderProgram
        CompiledShader compiled;
        compiled.stage = ShaderStage::Compute;
        compiled.name = "MipmapGeneration";
        compiled.entryPoint = "main";
        compiled.profile = "cs_6_6";
        compiled.success = true;
        compiled.bytecode = std::move(result.bytecode);

        s_builtInComputeProgram = ShaderProgram::CreateCompute(
            std::move(compiled),
            ShaderType::Composite);

        if (!s_builtInComputeProgram || !s_builtInComputeProgram->IsValid())
        {
            ERROR_AND_DIE("MipmapGenerator: Failed to create compute ShaderProgram");
        }
    }

    void MipmapGenerator::registerUniformBuffer()
    {
        auto* uniformMgr = g_theRendererSubsystem->GetUniformManager();
        if (!uniformMgr)
        {
            ERROR_AND_DIE("MipmapGenerator: UniformManager not available");
        }

        // Register MipGenUniforms at slot b11 (ROOT_CBV_MIPGEN) with PerDispatch strategy
        // maxCount must cover ALL mipmap dispatches per frame (not just one pass).
        // With N mipmapped RTs and M composite passes, total = N * mipLevels * M.
        // e.g. 1 RT * 10 mips * 4 composite passes = 40 dispatches. Use 64 for headroom.
        uniformMgr->RegisterBuffer<MipGenUniforms>(
            11, UpdateFrequency::PerDispatch, BufferSpace::Engine, 64);
    }

    // ========================================================================
    // GenerateMips - Main entry point
    // ========================================================================

    void MipmapGenerator::GenerateMips(D12Texture* texture, const MipmapConfig& config)
    {
        // Lazy init
        ensureInitialized();

        // Validate texture
        if (!texture || !texture->GetResource())
        {
            ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: null or invalid texture");
            return;
        }

        uint32_t mipLevels = texture->GetMipLevels();
        if (mipLevels <= 1)
        {
            return; // Nothing to generate
        }

        // Resolve compute program (custom or built-in)
        ShaderProgram* computeProgram = config.computeShader
            ? config.computeShader
            : s_builtInComputeProgram.get();

        // Get Compute PSO
        auto* psoManager = g_theRendererSubsystem->GetPSOManager();
        ID3D12PipelineState* computePSO = psoManager->GetOrCreateComputePSO(computeProgram);
        if (!computePSO)
        {
            ERROR_AND_DIE("MipmapGenerator: Failed to get Compute PSO");
        }

        // Get required systems
        auto* uniformMgr = g_theRendererSubsystem->GetUniformManager();
        auto* cmdList = D3D12RenderSystem::GetCurrentCommandList();
        ID3D12Resource* resource = texture->GetResource();
        ID3D12RootSignature* rootSig = D3D12RenderSystem::GetBindlessRootSignature()->GetRootSignature();

        // Calculate mip range
        uint32_t startMip = config.startMipLevel;
        uint32_t maxMip = mipLevels;
        if (config.mipLevelCount != UINT32_MAX)
        {
            maxMip = (std::min)(startMip + 1 + config.mipLevelCount, mipLevels);
        }

        // Transition entire resource to UNORDERED_ACCESS for compute shader.
        // Both source and destination mips are accessed via RWTexture2D (UAV),
        // avoiding the SRV state conflict that occurs with per-subresource barriers.
        texture->TransitionResourceTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        // Set compute root signature and PSO (once before the loop)
        cmdList->SetComputeRootSignature(rootSig);
        cmdList->SetPipelineState(computePSO);

        // Per-mip dispatch loop
        // UAV indices are persistent on D12Texture (created once, reused every frame).
        for (uint32_t mip = startMip + 1; mip < maxMip; ++mip)
        {
            uint32_t dstWidth = (std::max)(1u, texture->GetWidth() >> mip);
            uint32_t dstHeight = (std::max)(1u, texture->GetHeight() >> mip);

            // 1. Get persistent per-mip UAV indices from texture
            uint32_t srcUavIndex = texture->GetMipUavIndex(mip - 1);
            uint32_t dstUavIndex = texture->GetMipUavIndex(mip);

            // 2. Upload MipGenUniforms (PerDispatch auto-advances)
            MipGenUniforms uniforms = {};
            uniforms.srcTextureIndex      = srcUavIndex;  // Persistent per-mip UAV index
            uniforms.dstMipUavIndex       = dstUavIndex;
            uniforms.srcMipLevel          = mip - 1;      // For reference, not used by shader
            uniforms.samplerBindlessIndex = 0;             // Unused with RWTexture2D approach
            uniforms.dstWidth             = dstWidth;
            uniforms.dstHeight            = dstHeight;
            uniformMgr->UploadBuffer<MipGenUniforms>(uniforms);

            // 3. Bind CBV for this dispatch
            cmdList->SetComputeRootConstantBufferView(
                BindlessRootSignature::ROOT_CBV_MIPGEN,
                uniformMgr->GetEngineBufferGPUAddress(11));

            // 4. Dispatch compute shader
            uint32_t groupsX = (dstWidth + 7) / 8;
            uint32_t groupsY = (dstHeight + 7) / 8;
            cmdList->Dispatch(groupsX, groupsY, 1);

            // 5. UAV barrier (ensure writes complete before next mip reads)
            D3D12RenderSystem::UAVBarrier(cmdList, resource);
        }

        // Transition back to shader resource for sampling
        texture->TransitionResourceTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
} // namespace enigma::graphic
