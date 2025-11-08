#include "Engine/Graphic/Target/RenderTargetBinder.hpp"

#include "Engine/Core/Logger/Logger.hpp"
#include "Engine/Graphic/Target/RenderTargetManager.hpp"
#include "Engine/Graphic/Target/DepthTextureManager.hpp"
#include "Engine/Graphic/Target/ShadowColorManager.hpp"
#include "Engine/Graphic/Target/ShadowTargetManager.hpp"
#include "Engine/Graphic/Target/D12RenderTarget.hpp"
#include "Engine/Graphic/Target/D12DepthTexture.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"

using namespace enigma::core;

namespace enigma::graphic
{
    // ============================================================================
    // 构造函数
    // ============================================================================

    RenderTargetBinder::RenderTargetBinder(
        RenderTargetManager* rtManager,
        DepthTextureManager* depthManager,
        ShadowColorManager*  shadowColorManager,
        ShadowTargetManager* shadowTargetManager
    )
        : m_rtManager(rtManager)
          , m_depthManager(depthManager)
          , m_shadowColorManager(shadowColorManager)
          , m_shadowTargetManager(shadowTargetManager)
          , m_totalBindCalls(0)
          , m_cacheHitCount(0)
          , m_actualBindCalls(0)
    {
        // 验证所有Manager指针
        if (!m_rtManager || !m_depthManager || !m_shadowColorManager || !m_shadowTargetManager)
        {
            LogError("RenderTargetBinder", "One or more Manager pointers are null!");
        }

        LogInfo("RenderTargetBinder", "Created with all Managers aggregated");
    }

    // ============================================================================
    // 统一绑定接口
    // ============================================================================

    void RenderTargetBinder::BindRenderTargets(
        const std::vector<RTType>&     rtTypes,
        const std::vector<int>&        indices,
        RTType                         depthType,
        int                            depthIndex,
        bool                           useAlt,
        LoadAction                     loadAction,
        const std::vector<ClearValue>& clearValues,
        const ClearValue&              depthClearValue
    )
    {
        if (rtTypes.size() != indices.size())
        {
            LogError("RenderTargetBinder", "rtTypes.size() (%zu) != indices.size() (%zu)", rtTypes.size(), indices.size());
            return;
        }

        // 清空pendingState
        m_pendingState.Reset();

        // 设置LoadAction
        m_pendingState.loadAction = loadAction;

        // 填充RTV句柄和ClearValue
        m_pendingState.rtvHandles.reserve(rtTypes.size());
        m_pendingState.clearValues.reserve(rtTypes.size());

        for (size_t i = 0; i < rtTypes.size(); ++i)
        {
            RTType type  = rtTypes[i];
            int    index = indices[i];

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetRTVHandle(type, index, useAlt);
            if (rtvHandle.ptr == 0)
            {
                LOG_WARN_F("RenderTargetBinder", "Failed to get RTV handle for type=%d, index=%d",
                           static_cast<int>(type), index);
                continue;
            }

            m_pendingState.rtvHandles.push_back(rtvHandle);

            // 如果提供了clearValues且数量匹配，使用提供的值；否则使用默认黑色
            if (!clearValues.empty() && i < clearValues.size())
            {
                m_pendingState.clearValues.push_back(clearValues[i]);
            }
            else
            {
                m_pendingState.clearValues.push_back(ClearValue::Color(Rgba8::BLACK));
            }
        }

        // 设置DSV句柄和ClearValue
        m_pendingState.dsvHandle       = GetDSVHandle(depthType, depthIndex);
        m_pendingState.depthClearValue = depthClearValue;

        // 计算Hash
        m_pendingState.stateHash = m_pendingState.ComputeHash();

        ++m_totalBindCalls;

        LogDebug("RenderTargetBinder", "BindRenderTargets: %zu RTVs, LoadAction=%d, Hash=0x%08X",
                 m_pendingState.rtvHandles.size(), static_cast<int>(loadAction), m_pendingState.stateHash);
    }

    void RenderTargetBinder::BindSingleRenderTarget(
        RTType            rtType,
        int               rtIndex,
        RTType            depthType,
        int               depthIndex,
        bool              useAlt,
        LoadAction        loadAction,
        const ClearValue& clearValue,
        const ClearValue& depthClearValue
    )
    {
        std::vector<RTType>     rtTypes     = {rtType};
        std::vector<int>        indices     = {rtIndex};
        std::vector<ClearValue> clearValues = {clearValue};
        BindRenderTargets(rtTypes, indices, depthType, depthIndex, useAlt, loadAction, clearValues, depthClearValue);
    }

    // ============================================================================
    // 便捷绑定API
    // ============================================================================

    void RenderTargetBinder::BindGBuffer()
    {
        // GBuffer典型布局：4个ColorTex + 1个DepthTex
        std::vector<RTType> rtTypes = {
            RTType::ColorTex,
            RTType::ColorTex,
            RTType::ColorTex,
            RTType::ColorTex
        };
        std::vector<int> indices = {0, 1, 2, 3};

        BindRenderTargets(rtTypes, indices, RTType::DepthTex, 0, false);

        LogDebug("RenderTargetBinder", "BindGBuffer: 4 ColorTex + 1 DepthTex");
    }

    void RenderTargetBinder::BindShadowPass(int shadowIndex, int depthIndex)
    {
        BindSingleRenderTarget(RTType::ShadowColor, shadowIndex, RTType::DepthTex, depthIndex, false);
        LogDebug("RenderTargetBinder", "BindShadowPass: ShadowColor[%d] + DepthTex[%d]",
                 shadowIndex, depthIndex);
    }

    void RenderTargetBinder::BindCompositePass(int colorIndex)
    {
        BindSingleRenderTarget(RTType::ColorTex, colorIndex, RTType::DepthTex, 0, false);

        LogDebug("RenderTargetBinder", "BindCompositePass: ColorTex[%d]", colorIndex);
    }

    void RenderTargetBinder::ClearBindings()
    {
        m_pendingState.Reset();
        m_currentState.Reset();

        LogDebug("RenderTargetBinder", "ClearBindings: All bindings cleared");
    }

    // ============================================================================
    // 状态管理接口
    // ============================================================================

    void RenderTargetBinder::FlushBindings(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList)
        {
            LogError("RenderTargetBinder", "FlushBindings: cmdList is null");
            return;
        }

        // 计算pendingState的Hash（防止外部修改）
        uint32_t newHash = m_pendingState.ComputeHash();

        // 状态缓存：Hash比较
        if (newHash == m_currentState.stateHash && newHash != 0)
        {
            ++m_cacheHitCount;
            LogDebug("RenderTargetBinder", "FlushBindings: Cache HIT (Hash=0x%08X), skipping OMSetRenderTargets",
                     newHash);
            return; // 早期退出优化
        }

        // [FIX] 资源状态转换 - 在OMSetRenderTargets之前执行
        // 教学要点:
        // - DirectX 12要求资源状态必须匹配使用场景
        // - RTV需要RENDER_TARGET状态，DSV需要DEPTH_WRITE状态
        // - 必须在OMSetRenderTargets之前转换状态
        PerformResourceStateTransitions(cmdList);

        // 状态变化，调用D3D12 API
        UINT numRTVs = static_cast<UINT>(m_pendingState.rtvHandles.size());

        if (numRTVs > 0)
        {
            // 有RTV绑定
            cmdList->OMSetRenderTargets(
                numRTVs,
                m_pendingState.rtvHandles.data(),
                FALSE, // RTsSingleHandleToDescriptorRange
                m_pendingState.dsvHandle.ptr != 0 ? &m_pendingState.dsvHandle : nullptr
            );
        }
        else
        {
            // 无RTV绑定（清除所有RT）
            cmdList->OMSetRenderTargets(
                0,
                nullptr,
                FALSE,
                nullptr
            );
        }

        // 更新currentState
        m_currentState = m_pendingState;
        ++m_actualBindCalls;

        LogDebug("RenderTargetBinder", "FlushBindings: OMSetRenderTargets called (%u RTVs, Hash=0x%08X)",
                 numRTVs, newHash);

        // 执行Clear操作（如果LoadAction为Clear）
        // 教学要点: 
        // - Clear操作必须在OMSetRenderTargets之后执行
        // - 这确保清空的是正确绑定的渲染目标
        PerformClearOperations(cmdList);
    }

    void RenderTargetBinder::ForceFlushBindings(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList)
        {
            LogError("RenderTargetBinder", "ForceFlushBindings: cmdList is null");
            return;
        }

        // 强制调用OMSetRenderTargets（跳过Hash检查）
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
            cmdList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
        }

        // 更新currentState
        m_currentState = m_pendingState;
        ++m_actualBindCalls;

        LogDebug("RenderTargetBinder", "ForceFlushBindings: OMSetRenderTargets called (%u RTVs, forced)",
                 numRTVs);

        // 执行Clear操作（如果LoadAction为Clear）
        PerformClearOperations(cmdList);
    }

    bool RenderTargetBinder::HasPendingChanges() const
    {
        uint32_t pendingHash = m_pendingState.ComputeHash();
        return pendingHash != m_currentState.stateHash;
    }

    // ============================================================================
    // 内部辅助方法
    // ============================================================================

    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetBinder::GetRTVHandle(RTType type, int index, bool useAlt) const
    {
        switch (type)
        {
        case RTType::ColorTex:
            if (m_rtManager)
            {
                return useAlt ? m_rtManager->GetAltRTV(index) : m_rtManager->GetMainRTV(index);
            }
            break;

        case RTType::ShadowColor:
            if (m_shadowColorManager)
            {
                return useAlt ? m_shadowColorManager->GetAltRTV(index) : m_shadowColorManager->GetMainRTV(index);
            }
            break;

        case RTType::DepthTex:
            // DepthTex不应该作为RTV使用
            LOG_WARN("RenderTargetBinder", "DepthTex cannot be used as RTV");
            break;

        case RTType::ShadowTex:
            // ShadowTex是只读纹理，不能作为RTV使用
            LOG_WARN("RenderTargetBinder", "ShadowTex cannot be used as RTV (read-only texture)");
            break;

        default:
            LogError("RenderTargetBinder", "Unknown RTType: %d", static_cast<int>(type));
            break;
        }

        return D3D12_CPU_DESCRIPTOR_HANDLE{0};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetBinder::GetDSVHandle(RTType type, int index) const
    {
        if (type == RTType::DepthTex && m_depthManager)
        {
            return m_depthManager->GetDSV(index);
        }

        // 其他类型不能作为DSV使用
        if (type != RTType::DepthTex)
        {
            LOG_WARN_F("RenderTargetBinder", "Only DepthTex can be used as DSV, got type=%d",
                       static_cast<int>(type));
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

        // 根据LoadAction决定是否执行Clear操作
        if (m_pendingState.loadAction != LoadAction::Clear)
        {
            // LoadAction::Load - 保留现有内容，不做任何操作
            // LoadAction::DontCare - 性能优化，不关心内容，不做任何操作
            return;
        }

        // LoadAction::Clear - 清空所有RTV和DSV

        // 清空所有RTV
        for (size_t i = 0; i < m_pendingState.rtvHandles.size(); ++i)
        {
            const auto& rtvHandle = m_pendingState.rtvHandles[i];

            // 获取清空值的float数组
            float clearColor[4];
            if (i < m_pendingState.clearValues.size())
            {
                m_pendingState.clearValues[i].GetColorAsFloats(clearColor);
            }
            else
            {
                // 如果没有提供清空值，使用默认黑色
                ClearValue::Color(Rgba8::BLACK).GetColorAsFloats(clearColor);
            }

            // 调用DirectX 12 API清空RTV
            // 教学要点: 
            // - 虽然项目有CommandListManager封装，但这里直接使用DX12 API也是可以接受的
            // - ClearRenderTargetView是底层操作，通常不需要额外封装
            cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

            LogDebug("RenderTargetBinder", "Cleared RTV[%zu] to color (%f, %f, %f, %f)",
                     i, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        }

        // 清空DSV（如果存在）
        if (m_pendingState.dsvHandle.ptr != 0)
        {
            // 构造清空标志
            D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;

            // 调用DirectX 12 API清空DSV
            // 教学要点:
            // - ClearDepthStencilView同样是底层操作
            // - depth和stencil值从depthClearValue获取
            cmdList->ClearDepthStencilView(
                m_pendingState.dsvHandle,
                clearFlags,
                m_pendingState.depthClearValue.depthStencil.depth,
                m_pendingState.depthClearValue.depthStencil.stencil,
                0,
                nullptr
            );

            LogDebug("RenderTargetBinder", "Cleared DSV to depth=%f, stencil=%u",
                     m_pendingState.depthClearValue.depthStencil.depth,
                     m_pendingState.depthClearValue.depthStencil.stencil);
        }
    }

    void RenderTargetBinder::PerformResourceStateTransitions(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList)
        {
            LogError("RenderTargetBinder", "PerformResourceStateTransitions: cmdList is null");
            return;
        }

        // [FIX] Bug #538 资源状态转换修复
        // 教学要点:
        // - DirectX 12要求资源状态必须匹配使用场景
        // - RTV需要RENDER_TARGET状态，DSV需要DEPTH_WRITE状态
        // - 使用D3D12RenderSystem::TransitionResource()统一管理状态转换

        // 转换所有RTV资源状态: PIXEL_SHADER_RESOURCE -> RENDER_TARGET
        for (size_t i = 0; i < m_pendingState.rtvHandles.size(); ++i)
        {
            const auto& rtvHandle = m_pendingState.rtvHandles[i];

            // 从RTV句柄获取底层资源
            ID3D12Resource* resource = GetResourceFromRTVHandle(rtvHandle);
            if (!resource)
            {
                LOG_WARN_F("RenderTargetBinder", "Failed to get resource from RTV[%zu]", i);
                continue;
            }

            // 使用D3D12RenderSystem的统一状态转换API
            D3D12RenderSystem::TransitionResource(
                cmdList,
                resource,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                "RenderTargetBinder::RTV"
            );
        }

        // 转换DSV资源状态: PIXEL_SHADER_RESOURCE -> DEPTH_WRITE
        if (m_pendingState.dsvHandle.ptr != 0)
        {
            ID3D12Resource* depthResource = GetResourceFromDSVHandle(m_pendingState.dsvHandle);
            if (depthResource)
            {
                // 使用D3D12RenderSystem的统一状态转换API
                D3D12RenderSystem::TransitionResource(
                    cmdList,
                    depthResource,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_DEPTH_WRITE,
                    "RenderTargetBinder::DSV"
                );
            }
            else
            {
                LOG_WARN("RenderTargetBinder", "Failed to get resource from DSV");
            }
        }

        LogDebug("RenderTargetBinder", "Performed resource state transitions for %zu RTVs + %d DSV",
                 m_pendingState.rtvHandles.size(), m_pendingState.dsvHandle.ptr != 0 ? 1 : 0);
    }

    ID3D12Resource* RenderTargetBinder::GetResourceFromRTVHandle(const D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle) const
    {
        // [TODO] 实现从RTV句柄获取底层资源的逻辑
        // 教学要点:
        // - 需要遍历所有Manager，根据RTV句柄匹配对应的资源
        // - 这是一个查找操作，可能需要优化（例如使用缓存）

        // 尝试从RenderTargetManager查找
        if (m_rtManager)
        {
            for (int i = 0; i < 16; ++i)
            {
                auto mainRTV = m_rtManager->GetMainRTV(i);
                auto altRTV  = m_rtManager->GetAltRTV(i);

                if (mainRTV.ptr == rtvHandle.ptr || altRTV.ptr == rtvHandle.ptr)
                {
                    auto rt = m_rtManager->GetRenderTarget(i);
                    if (rt)
                    {
                        return rt->GetResource();
                    }
                }
            }
        }

        // 尝试从ShadowColorManager查找
        if (m_shadowColorManager)
        {
            for (int i = 0; i < 8; ++i)
            {
                auto mainRTV = m_shadowColorManager->GetMainRTV(i);
                auto altRTV  = m_shadowColorManager->GetAltRTV(i);

                if (mainRTV.ptr == rtvHandle.ptr || altRTV.ptr == rtvHandle.ptr)
                {
                    auto shadowColor = m_shadowColorManager->GetShadowColor(i);
                    if (shadowColor)
                    {
                        return shadowColor->GetResource();
                    }
                }
            }
        }

        return nullptr;
    }

    ID3D12Resource* RenderTargetBinder::GetResourceFromDSVHandle(const D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle) const
    {
        // [TODO] 实现从DSV句柄获取底层资源的逻辑
        // 教学要点:
        // - 只需要查找DepthTextureManager

        if (m_depthManager)
        {
            for (int i = 0; i < 3; ++i)
            {
                auto dsv = m_depthManager->GetDSV(i);

                if (dsv.ptr == dsvHandle.ptr)
                {
                    auto depthTex = m_depthManager->GetDepthTexture(i);
                    if (depthTex)
                    {
                        return depthTex->GetResource();
                    }
                }
            }
        }

        return nullptr;
    }
} // namespace enigma::graphic
