/**
 * @file RenderTargetBinder.cpp
 * @brief [REFACTOR] Implementation of Provider-based RenderTargetBinder
 */

#include "Engine/Graphic/Target/RenderTargetBinder.hpp"

#include "Engine/Core/Logger/Logger.hpp"
#include "Engine/Graphic/Target/IRenderTargetProvider.hpp"
#include "Engine/Graphic/Target/ShadowTextureProvider.hpp"
#include "Engine/Graphic/Target/DepthTextureProvider.hpp"

using namespace enigma::core;

namespace enigma::graphic
{
    // ============================================================================
    // Constructor
    // ============================================================================

    RenderTargetBinder::RenderTargetBinder(
        IRenderTargetProvider* colorProvider,
        IRenderTargetProvider* depthProvider,
        IRenderTargetProvider* shadowColorProvider,
        IRenderTargetProvider* shadowTexProvider
    )
        : m_colorProvider(colorProvider)
          , m_depthProvider(depthProvider)
          , m_shadowColorProvider(shadowColorProvider)
          , m_shadowTexProvider(shadowTexProvider)
          , m_cachedDSVHandle{0}
          , m_hasDepthBinding(false)
          , m_totalBindCalls(0)
          , m_cacheHitCount(0)
          , m_actualBindCalls(0)
          , m_currentDepthFormat(DXGI_FORMAT_UNKNOWN)
    {
        // Validate all provider pointers
        if (!m_colorProvider || !m_depthProvider || !m_shadowColorProvider || !m_shadowTexProvider)
        {
            LogError("RenderTargetBinder", "One or more Provider pointers are null!");
        }

        // Initialize format cache
        for (int i = 0; i < 8; ++i)
        {
            m_currentRTFormats[i] = DXGI_FORMAT_UNKNOWN;
        }

        LogInfo("RenderTargetBinder", "Created with all Providers aggregated");
    }

    // ============================================================================
    // Unified Binding Interface
    // ============================================================================

    void RenderTargetBinder::BindRenderTargets(
        const std::vector<std::pair<RenderTargetType, int>>& targets
    )
    {
        // ========== Validate depth binding constraints ==========
        int                                           shadowTexCount = 0;
        int                                           depthTexCount  = 0;
        std::pair<RenderTargetType, int>              depthTarget    = {RenderTargetType::DepthTex, -1}; // -1 = not specified
        std::vector<std::pair<RenderTargetType, int>> colorTargets;

        for (const auto& target : targets)
        {
            switch (target.first)
            {
            case RenderTargetType::ShadowTex:
                shadowTexCount++;
                depthTarget = target;
                break;
            case RenderTargetType::DepthTex:
                depthTexCount++;
                depthTarget = target;
                break;
            case RenderTargetType::ColorTex:
            case RenderTargetType::ShadowColor:
                colorTargets.push_back(target);
                break;
            }
        }

        // Validate: cannot bind both ShadowTex and DepthTex
        if (shadowTexCount > 0 && depthTexCount > 0)
        {
            throw InvalidBindingException(InvalidBindingException::Reason::DualDepthBinding);
        }

        // Validate: cannot bind multiple ShadowTex
        if (shadowTexCount > 1)
        {
            throw InvalidBindingException(InvalidBindingException::Reason::MultipleShadowTex);
        }

        // Validate: cannot bind multiple DepthTex
        if (depthTexCount > 1)
        {
            throw InvalidBindingException(InvalidBindingException::Reason::MultipleDepthTex);
        }

        // ========== Execute binding ==========
        // Reset pending state
        m_pendingState.Reset();

        // Collect RTV handles
        m_pendingState.rtvHandles.reserve(colorTargets.size());
        for (const auto& colorTarget : colorTargets)
        {
            auto* provider = GetProvider(colorTarget.first);
            if (provider)
            {
                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = provider->GetMainRTV(colorTarget.second);
                if (rtvHandle.ptr != 0)
                {
                    m_pendingState.rtvHandles.push_back(rtvHandle);
                    m_pendingState.clearValues.push_back(ClearValue::Color(Rgba8::BLACK));
                }
                else
                {
                    LOG_WARN_F("RenderTargetBinder", "Failed to get RTV for type=%d, index=%d",
                               static_cast<int>(colorTarget.first), colorTarget.second);
                }
            }
        }

        // Get DSV handle (if depth target exists)
        m_hasDepthBinding = (depthTarget.second >= 0);
        if (m_hasDepthBinding)
        {
            m_pendingState.dsvHandle = GetDSVHandle(depthTarget.first, depthTarget.second);
        }
        else
        {
            m_pendingState.dsvHandle = D3D12_CPU_DESCRIPTOR_HANDLE{0};
        }

        // Compute hash
        m_pendingState.stateHash = m_pendingState.ComputeHash();

        // Cache for FlushBindings
        m_cachedRTVHandles = m_pendingState.rtvHandles;
        m_cachedDSVHandle  = m_pendingState.dsvHandle;

        // Update format cache
        for (int i = 0; i < 8; ++i)
        {
            m_currentRTFormats[i] = DXGI_FORMAT_UNKNOWN;
        }
        m_currentDepthFormat = DXGI_FORMAT_UNKNOWN;

        // [FIX] Fill format cache from providers for PSO creation
        // Color targets: fill m_currentRTFormats[] in order
        int rtIndex = 0;
        for (const auto& colorTarget : colorTargets)
        {
            if (rtIndex >= 8) break; // Max 8 RTVs

            auto* provider = GetProvider(colorTarget.first);
            if (provider)
            {
                try
                {
                    m_currentRTFormats[rtIndex] = provider->GetFormat(colorTarget.second);
                }
                catch (const std::exception& e)
                {
                    LOG_WARN_F("RenderTargetBinder", "Failed to get format for RT[%d]: %s", rtIndex, e.what());
                    m_currentRTFormats[rtIndex] = DXGI_FORMAT_UNKNOWN;
                }
            }
            ++rtIndex;
        }

        // Depth target: fill m_currentDepthFormat
        if (m_hasDepthBinding && depthTarget.second >= 0)
        {
            auto* provider = GetProvider(depthTarget.first);
            if (provider)
            {
                try
                {
                    m_currentDepthFormat = provider->GetFormat(depthTarget.second);
                }
                catch (const std::exception& e)
                {
                    LOG_WARN_F("RenderTargetBinder", "Failed to get depth format: %s", e.what());
                    m_currentDepthFormat = DXGI_FORMAT_UNKNOWN;
                }
            }
        }

        ++m_totalBindCalls;
    }

    IRenderTargetProvider* RenderTargetBinder::GetProvider(RenderTargetType rtType)
    {
        switch (rtType)
        {
        case RenderTargetType::ColorTex:
            return m_colorProvider;
        case RenderTargetType::DepthTex:
            return m_depthProvider;
        case RenderTargetType::ShadowColor:
            return m_shadowColorProvider;
        case RenderTargetType::ShadowTex:
            return m_shadowTexProvider;
        default:
            LogError("RenderTargetBinder", "Unknown RenderTargetType: %d", static_cast<int>(rtType));
            throw std::invalid_argument("Unknown RenderTargetType");
        }
    }

    void RenderTargetBinder::ClearBindings()
    {
        m_pendingState.Reset();
        m_currentState.Reset();
        m_cachedRTVHandles.clear();
        m_cachedDSVHandle = D3D12_CPU_DESCRIPTOR_HANDLE{0};
        m_hasDepthBinding = false;

        // Clear format cache
        for (int i = 0; i < 8; ++i)
        {
            m_currentRTFormats[i] = DXGI_FORMAT_UNKNOWN;
        }
        m_currentDepthFormat = DXGI_FORMAT_UNKNOWN;
    }

    // ============================================================================
    // State Management Interface
    // ============================================================================

    void RenderTargetBinder::FlushBindings(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList)
        {
            LogError("RenderTargetBinder", "FlushBindings: cmdList is null");
            return;
        }

        // Compute pending state hash (prevent external modification)
        uint32_t newHash = m_pendingState.ComputeHash();

        // State cache: hash comparison
        if (newHash == m_currentState.stateHash && newHash != 0)
        {
            ++m_cacheHitCount;
            return; // Early exit optimization
        }

        // State changed, call D3D12 API
        UINT numRTVs = static_cast<UINT>(m_pendingState.rtvHandles.size());

        if (numRTVs > 0)
        {
            // Has RTV bindings
            cmdList->OMSetRenderTargets(
                numRTVs,
                m_pendingState.rtvHandles.data(),
                FALSE, // RTsSingleHandleToDescriptorRange
                m_pendingState.dsvHandle.ptr != 0 ? &m_pendingState.dsvHandle : nullptr
            );
        }
        else
        {
            // No RTV bindings (clear all RTs)
            cmdList->OMSetRenderTargets(
                0,
                nullptr,
                FALSE,
                m_pendingState.dsvHandle.ptr != 0 ? &m_pendingState.dsvHandle : nullptr
            );
        }

        // Update current state
        m_currentState = m_pendingState;
        ++m_actualBindCalls;
        // Perform clear operations (if LoadAction is Clear)
        PerformClearOperations(cmdList);
    }

    void RenderTargetBinder::ForceFlushBindings(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList)
        {
            LogError("RenderTargetBinder", "ForceFlushBindings: cmdList is null");
            return;
        }

        // Force call OMSetRenderTargets (skip hash check)
        UINT numRTVs = static_cast<UINT>(m_pendingState.rtvHandles.size());

        if (numRTVs > 0)
        {
            cmdList->OMSetRenderTargets(
                numRTVs,
                m_pendingState.rtvHandles.data(),
                FALSE,
                m_pendingState.dsvHandle.ptr != 0 ? &m_pendingState.dsvHandle : nullptr
            );
        }
        else
        {
            cmdList->OMSetRenderTargets(
                0,
                nullptr,
                FALSE,
                m_pendingState.dsvHandle.ptr != 0 ? &m_pendingState.dsvHandle : nullptr
            );
        }

        // Update current state
        m_currentState = m_pendingState;
        ++m_actualBindCalls;
        // Perform clear operations (if LoadAction is Clear)
        PerformClearOperations(cmdList);
    }

    bool RenderTargetBinder::HasPendingChanges() const
    {
        uint32_t pendingHash = m_pendingState.ComputeHash();
        return pendingHash != m_currentState.stateHash;
    }

    void RenderTargetBinder::GetCurrentRTFormats(DXGI_FORMAT outFormats[8]) const
    {
        for (int i = 0; i < 8; ++i)
        {
            outFormats[i] = m_currentRTFormats[i];
        }
    }

    DXGI_FORMAT RenderTargetBinder::GetCurrentDepthFormat() const
    {
        return m_currentDepthFormat;
    }

    // ============================================================================
    // Internal Implementation
    // ============================================================================

    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetBinder::GetDSVHandle(RenderTargetType rtType, int index) const
    {
        if (rtType == RenderTargetType::DepthTex && m_depthProvider)
        {
            // DepthTextureProvider supports DSV
            if (m_depthProvider->SupportsDSV())
            {
                auto* depthProvider = static_cast<DepthTextureProvider*>(m_depthProvider);
                return depthProvider->GetDSV(index);
            }
        }
        else if (rtType == RenderTargetType::ShadowTex && m_shadowTexProvider)
        {
            // ShadowTextureProvider supports DSV
            if (m_shadowTexProvider->SupportsDSV())
            {
                auto* shadowTexProvider = static_cast<ShadowTextureProvider*>(m_shadowTexProvider);
                return shadowTexProvider->GetDSV(index);
            }
        }

        // Other types cannot be used as DSV
        if (rtType != RenderTargetType::DepthTex && rtType != RenderTargetType::ShadowTex)
        {
            LogWarn("RenderTargetBinder", "Only DepthTex/ShadowTex can be used as DSV, got type=%d",
                    static_cast<int>(rtType));
        }

        return D3D12_CPU_DESCRIPTOR_HANDLE{0};
    }

    void RenderTargetBinder::PerformClearOperations(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList)
        {
            LogError("RenderTargetBinder", "PerformClearOperations: cmdList is null");
            return;
        }

        // Early exit for LoadAction::Load (most common case)
        if (m_pendingState.loadAction != LoadAction::Clear)
        {
            return;
        }

        // LoadAction::Clear - clear all RTVs and DSV

        // Clear all RTVs
        for (size_t i = 0; i < m_pendingState.rtvHandles.size(); ++i)
        {
            const auto& rtvHandle = m_pendingState.rtvHandles[i];

            float clearColor[4];
            if (i < m_pendingState.clearValues.size())
            {
                m_pendingState.clearValues[i].GetColorAsFloats(clearColor);
            }
            else
            {
                ClearValue::Color(Rgba8::BLACK).GetColorAsFloats(clearColor);
            }

            cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        }

        // Clear DSV (if exists)
        if (m_pendingState.dsvHandle.ptr != 0)
        {
            D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;

            cmdList->ClearDepthStencilView(
                m_pendingState.dsvHandle,
                clearFlags,
                m_pendingState.depthClearValue.depthStencil.depth,
                m_pendingState.depthClearValue.depthStencil.stencil,
                0,
                nullptr
            );
        }
    }

    uint32_t RenderTargetBinder::ComputeStateHash() const
    {
        return m_pendingState.ComputeHash();
    }
} // namespace enigma::graphic
