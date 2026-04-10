#include "MipmapGenerator.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Shader/Program/ShaderProgram.hpp"
#include "Engine/Graphic/Shader/PSO/PSOManager.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Shader/Compiler/DXCCompiler.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Integration/RendererEvents.hpp"
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
    namespace
    {
        const char* GetQueueTypeName(CommandQueueType queueType)
        {
            switch (queueType)
            {
            case CommandQueueType::Graphics:
                return "Graphics";
            case CommandQueueType::Compute:
                return "Compute";
            case CommandQueueType::Copy:
                return "Copy";
            }

            return "Unknown";
        }

        const char* GetFallbackReasonName(QueueFallbackReason reason)
        {
            switch (reason)
            {
            case QueueFallbackReason::None:
                return "None";
            case QueueFallbackReason::GraphicsOnlyMode:
                return "GraphicsOnlyMode";
            case QueueFallbackReason::RouteNotValidated:
                return "RouteNotValidated";
            case QueueFallbackReason::UnsupportedWorkload:
                return "UnsupportedWorkload";
            case QueueFallbackReason::RequiresGraphicsStateTransition:
                return "RequiresGraphicsStateTransition";
            case QueueFallbackReason::DedicatedQueueUnavailable:
                return "DedicatedQueueUnavailable";
            case QueueFallbackReason::QueueTypeUnavailable:
                return "QueueTypeUnavailable";
            case QueueFallbackReason::ResourceStateNotSupported:
                return "ResourceStateNotSupported";
            }

            return "Unknown";
        }

        bool IsUnsupportedForComputeRoute(D3D12_RESOURCE_STATES state)
        {
            constexpr D3D12_RESOURCE_STATES kUnsupportedMask =
                D3D12_RESOURCE_STATE_DEPTH_WRITE |
                D3D12_RESOURCE_STATE_DEPTH_READ |
                D3D12_RESOURCE_STATE_RESOLVE_SOURCE |
                D3D12_RESOURCE_STATE_RESOLVE_DEST;

            return (state & kUnsupportedMask) != 0;
        }

        bool NeedsGraphicsPreambleForCompute(D3D12_RESOURCE_STATES state)
        {
            constexpr D3D12_RESOURCE_STATES kDirectComputeSafeMask =
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
                D3D12_RESOURCE_STATE_COPY_SOURCE |
                D3D12_RESOURCE_STATE_COPY_DEST |
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            if (state == D3D12_RESOURCE_STATE_COMMON)
            {
                return false;
            }

            return (state & ~kDirectComputeSafeMask) != 0;
        }

        void BindDescriptorHeapsIfNeeded(ID3D12GraphicsCommandList* commandList)
        {
            auto* heapMgr = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
            if (heapMgr)
            {
                heapMgr->SetDescriptorHeaps(commandList);
            }
        }

        bool WaitAndRecycleSubmission(CommandListManager* commandListManager, const QueueSubmissionToken& token)
        {
            if (!commandListManager->WaitForSubmission(token))
            {
                return false;
            }

            commandListManager->UpdateCompletedCommandLists();
            return true;
        }
    }

    // Static member definitions
    std::map<MipFilterMode, std::unique_ptr<ShaderProgram>> MipmapGenerator::s_shaderCache;
    bool MipmapGenerator::s_initialized = false;

    // ========================================================================
    // Event-Driven Initialization
    // ========================================================================

    void MipmapGenerator::Initialize()
    {
        RendererEvents::OnPipelineReady.Add([]() { onPipelineReady(); });
    }

    void MipmapGenerator::onPipelineReady()
    {
        if (s_initialized)
            return;

        registerUniformBuffer();
        compileAllVariants();
        s_initialized = true;

        LogInfo("MipmapGenerator", "Initialized: %zu filter variants compiled",
                s_shaderCache.size());
    }

    void MipmapGenerator::compileAllVariants()
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
        options.includePaths.push_back(L".enigma/assets/engine/shaders/");
        options.includePaths.push_back(L".enigma/assets/engine/shaders/core/");

        // Iterate all filter modes using MIP_FILTER_SHADER_PATHS from MipmapCommon.hpp
        for (int i = 0; i < static_cast<int>(MipFilterMode::COUNT); ++i)
        {
            MipFilterMode mode = static_cast<MipFilterMode>(i);
            const wchar_t* path = MIP_FILTER_SHADER_PATHS[i];

            DXCCompiler::CompileResult result = compiler.CompileShaderFromFile(path, options);
            if (!result.success)
            {
                LogError("MipmapGenerator", "Compute shader compile error: %s",
                         result.errorMessage.c_str());
                ERROR_AND_DIE("MipmapGenerator: shader compilation failed");
            }

            CompiledShader compiled;
            compiled.stage      = ShaderStage::Compute;
            compiled.name       = "MipGen_" + std::to_string(i);
            compiled.entryPoint = "main";
            compiled.profile    = "cs_6_6";
            compiled.success    = true;
            compiled.bytecode   = std::move(result.bytecode);

            auto program = ShaderProgram::CreateCompute(std::move(compiled), ShaderType::Composite);
            if (!program || !program->IsValid())
            {
                ERROR_AND_DIE("MipmapGenerator: Failed to create compute ShaderProgram");
            }

            s_shaderCache[mode] = std::move(program);
        }
    }

    ShaderProgram* MipmapGenerator::GetFilterShader(MipFilterMode mode)
    {
        auto it = s_shaderCache.find(mode);
        if (it != s_shaderCache.end())
            return it->second.get();
        return nullptr;
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

        // Resolve compute program: custom shader takes priority, otherwise select by FilterMode
        ShaderProgram* computeProgram = config.computeShader
            ? config.computeShader
            : GetFilterShader(config.filterMode);

        if (!computeProgram)
        {
            ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: No shader available. Was Initialize() called?");
            return;
        }

        // Get Compute PSO
        auto* psoManager = g_theRendererSubsystem->GetPSOManager();
        ID3D12PipelineState* computePSO = psoManager->GetOrCreateComputePSO(computeProgram);
        if (!computePSO)
        {
            ERROR_AND_DIE("MipmapGenerator: Failed to get Compute PSO");
        }

        // Get required systems
        auto*                 uniformMgr         = g_theRendererSubsystem->GetUniformManager();
        ID3D12Resource*       resource           = texture->GetResource();
        auto*                 bindlessRootSig    = D3D12RenderSystem::GetBindlessRootSignature();
        ID3D12RootSignature*  rootSig            = bindlessRootSig ? bindlessRootSig->GetRootSignature() : nullptr;
        auto* cmdListManager = D3D12RenderSystem::GetCommandListManager();
        if (!uniformMgr || !cmdListManager || !rootSig)
        {
            ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Required DX12 runtime state is unavailable");
            return;
        }

        const bool                   hasActiveGraphicsCommandList = D3D12RenderSystem::GetCurrentCommandList() != nullptr;
        const D3D12_RESOURCE_STATES  initialState                = texture->GetCurrentState();
        const uint32_t               sourceSrvIndex              = texture->GetBindlessIndex();
        QueueRouteContext            routeContext                = {};
        routeContext.workload                                   = QueueWorkloadClass::MipmapGeneration;
        routeContext.prefersAsyncExecution                      = true;
        routeContext.allowGraphicsFallback                      = true;

        if (sourceSrvIndex == UINT32_MAX)
        {
            ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: texture SRV bindless index is invalid");
            return;
        }

        if (hasActiveGraphicsCommandList)
        {
            routeContext.forcedFallbackReason = QueueFallbackReason::RouteNotValidated;
        }
        else if (IsUnsupportedForComputeRoute(initialState))
        {
            routeContext.forcedFallbackReason = QueueFallbackReason::ResourceStateNotSupported;
        }

        const QueueRouteDecision routeDecision = D3D12RenderSystem::ResolveQueueRoute(routeContext);
        const bool               usesComputeRoute = routeDecision.activeQueue == CommandQueueType::Compute;

        if (routeDecision.UsesFallback())
        {
            LogDebug(LogRenderer,
                     "MipmapGenerator: '%s' routed to %s (fallback=%s)",
                     texture->GetDebugName().c_str(),
                     GetQueueTypeName(routeDecision.activeQueue),
                     GetFallbackReasonName(routeDecision.fallbackReason));
        }

        auto submitAndTrack = [&](ID3D12GraphicsCommandList* commandList) -> QueueSubmissionToken
        {
            QueueSubmissionToken submissionToken = cmdListManager->SubmitCommandList(commandList);
            if (submissionToken.IsValid())
            {
                D3D12RenderSystem::RecordQueueSubmission(submissionToken, QueueWorkloadClass::MipmapGeneration);
            }

            return submissionToken;
        };

        auto insertQueueWaitOrBlock = [&](CommandQueueType waitingQueue, const QueueSubmissionToken& producerToken) -> bool
        {
            if (cmdListManager->InsertQueueWait(waitingQueue, producerToken))
            {
                D3D12RenderSystem::RecordQueueWaitInsertion(producerToken.queueType, waitingQueue);
                return true;
            }

            LogWarn(LogRenderer,
                    "MipmapGenerator: Falling back to CPU wait before %s queue consumption of '%s'",
                    GetQueueTypeName(waitingQueue),
                    texture->GetDebugName().c_str());
            return WaitAndRecycleSubmission(cmdListManager, producerToken);
        };

        auto transitionResourceState = [&](ID3D12GraphicsCommandList* commandList,
                                           D3D12_RESOURCE_STATES      beforeState,
                                           D3D12_RESOURCE_STATES      afterState)
        {
            if (beforeState == afterState)
            {
                return;
            }

            D3D12RenderSystem::TransitionResource(commandList,
                                                  resource,
                                                  beforeState,
                                                  afterState,
                                                  texture->GetDebugName().c_str());
        };

        auto finalizeToShaderResource = [&](ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES beforeState)
        {
            transitionResourceState(commandList,
                                    beforeState,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            texture->SetCurrentState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        };

        // Calculate mip range
        uint32_t startMip = config.startMipLevel;
        uint32_t maxMip = mipLevels;
        if (config.mipLevelCount != UINT32_MAX)
        {
            maxMip = (std::min)(startMip + 1 + config.mipLevelCount, mipLevels);
        }

        if (usesComputeRoute)
        {
            D3D12_RESOURCE_STATES computeSourceState = initialState;

            if (NeedsGraphicsPreambleForCompute(initialState))
            {
                auto* preambleCommandList = cmdListManager->AcquireCommandList(CommandQueueType::Graphics, "MipGen_GraphicsPreamble");
                if (!preambleCommandList)
                {
                    ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Failed to acquire graphics preamble command list");
                    return;
                }

                transitionResourceState(preambleCommandList,
                                        initialState,
                                        D3D12_RESOURCE_STATE_COMMON);

                QueueSubmissionToken preambleToken = submitAndTrack(preambleCommandList);
                if (!preambleToken.IsValid())
                {
                    ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Failed to submit graphics preamble");
                    return;
                }

                if (!insertQueueWaitOrBlock(CommandQueueType::Compute, preambleToken))
                {
                    ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Failed to synchronize graphics preamble with compute queue");
                    return;
                }

                computeSourceState = D3D12_RESOURCE_STATE_COMMON;
            }

            auto* computeCommandList = cmdListManager->AcquireCommandList(CommandQueueType::Compute, "MipGen_Compute");
            if (!computeCommandList)
            {
                ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Failed to acquire compute command list");
                return;
            }

            BindDescriptorHeapsIfNeeded(computeCommandList);

            transitionResourceState(computeCommandList,
                                    computeSourceState,
                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            computeCommandList->SetComputeRootSignature(rootSig);
            computeCommandList->SetPipelineState(computePSO);

            for (uint32_t mip = startMip + 1; mip < maxMip; ++mip)
            {
                uint32_t dstWidth = (std::max)(1u, texture->GetWidth() >> mip);
                uint32_t dstHeight = (std::max)(1u, texture->GetHeight() >> mip);

                uint32_t dstUavIndex = texture->GetMipUavIndex(mip);

                MipGenUniforms uniforms = {};
                uniforms.srcTextureIndex      = sourceSrvIndex;
                uniforms.dstMipUavIndex       = dstUavIndex;
                uniforms.srcMipLevel          = mip - 1;
                uniforms.samplerBindlessIndex = 0;
                uniforms.dstWidth             = dstWidth;
                uniforms.dstHeight            = dstHeight;
                uniformMgr->UploadBuffer<MipGenUniforms>(uniforms);

                computeCommandList->SetComputeRootConstantBufferView(
                    BindlessRootSignature::ROOT_CBV_MIPGEN,
                    uniformMgr->GetEngineBufferGPUAddress(11));

                uint32_t groupsX = (dstWidth + 7) / 8;
                uint32_t groupsY = (dstHeight + 7) / 8;
                computeCommandList->Dispatch(groupsX, groupsY, 1);
                D3D12RenderSystem::UAVBarrier(computeCommandList, resource);
            }

            transitionResourceState(computeCommandList,
                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                    D3D12_RESOURCE_STATE_COMMON);

            QueueSubmissionToken computeToken = submitAndTrack(computeCommandList);
            if (!computeToken.IsValid())
            {
                ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Failed to submit compute workload");
                return;
            }

            if (!insertQueueWaitOrBlock(CommandQueueType::Graphics, computeToken))
            {
                ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Failed to synchronize compute mip generation with graphics queue");
                return;
            }

            auto* finalizeCommandList = cmdListManager->AcquireCommandList(CommandQueueType::Graphics, "MipGen_GraphicsFinalize");
            if (!finalizeCommandList)
            {
                ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Failed to acquire graphics finalize command list");
                return;
            }

            finalizeToShaderResource(finalizeCommandList, D3D12_RESOURCE_STATE_COMMON);

            QueueSubmissionToken finalizeToken = submitAndTrack(finalizeCommandList);
            if (!finalizeToken.IsValid())
            {
                ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Failed to submit graphics finalize workload");
                return;
            }

            if (!WaitAndRecycleSubmission(cmdListManager, finalizeToken))
            {
                ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Failed to wait for graphics finalize workload");
            }

            return;
        }

        // Use the active frame graphics list when the route is validated only as graphics.
        ID3D12GraphicsCommandList* graphicsCommandList = D3D12RenderSystem::GetCurrentCommandList();
        bool                       selfAcquiredGraphicsList = false;
        if (!graphicsCommandList)
        {
            graphicsCommandList = cmdListManager->AcquireCommandList(CommandQueueType::Graphics, "MipGen_Graphics");
            if (!graphicsCommandList)
            {
                ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Failed to acquire graphics command list");
                return;
            }

            selfAcquiredGraphicsList = true;
            BindDescriptorHeapsIfNeeded(graphicsCommandList);
        }

        transitionResourceState(graphicsCommandList,
                                initialState,
                                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        graphicsCommandList->SetComputeRootSignature(rootSig);
        graphicsCommandList->SetPipelineState(computePSO);

        // Per-mip dispatch loop
        // UAV indices are persistent on D12Texture (created once, reused every frame).
        for (uint32_t mip = startMip + 1; mip < maxMip; ++mip)
        {
            uint32_t dstWidth = (std::max)(1u, texture->GetWidth() >> mip);
            uint32_t dstHeight = (std::max)(1u, texture->GetHeight() >> mip);

            // Read from the texture SRV and write to the destination mip UAV.
            uint32_t dstUavIndex = texture->GetMipUavIndex(mip);

            // 2. Upload MipGenUniforms (PerDispatch auto-advances)
            MipGenUniforms uniforms = {};
            uniforms.srcTextureIndex      = sourceSrvIndex;
            uniforms.dstMipUavIndex       = dstUavIndex;
            uniforms.srcMipLevel          = mip - 1; // Explicit source mip for all built-in variants
            uniforms.samplerBindlessIndex = 0;       // Built-in box filter uses sampler0; Load-based variants ignore this field
            uniforms.dstWidth             = dstWidth;
            uniforms.dstHeight            = dstHeight;
            uniformMgr->UploadBuffer<MipGenUniforms>(uniforms);

            // 3. Bind CBV for this dispatch
            graphicsCommandList->SetComputeRootConstantBufferView(
                BindlessRootSignature::ROOT_CBV_MIPGEN,
                uniformMgr->GetEngineBufferGPUAddress(11));

            // 4. Dispatch compute shader
            uint32_t groupsX = (dstWidth + 7) / 8;
            uint32_t groupsY = (dstHeight + 7) / 8;
            graphicsCommandList->Dispatch(groupsX, groupsY, 1);

            // 5. UAV barrier (ensure writes complete before next mip reads)
            D3D12RenderSystem::UAVBarrier(graphicsCommandList, resource);
        }

        // Transition back to shader resource for sampling
        finalizeToShaderResource(graphicsCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        if (selfAcquiredGraphicsList)
        {
            QueueSubmissionToken submissionToken = submitAndTrack(graphicsCommandList);
            if (!submissionToken.IsValid())
            {
                LogError(LogRenderer, "MipmapGenerator: SubmitCommandList failed for '%s'", texture->GetDebugName().c_str());
                return;
            }

            if (!WaitAndRecycleSubmission(cmdListManager, submissionToken))
            {
                ERROR_RECOVERABLE("MipmapGenerator::GenerateMips: Failed to wait for graphics workload");
            }
        }
    }
} // namespace enigma::graphic
