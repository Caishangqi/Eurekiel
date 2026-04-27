#include "Engine/Graphic/Bundle/Integration/ShaderBundleReloadAdapter.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Bundle/BundleException.hpp"
#include "Engine/Graphic/Bundle/Directive/PackRenderTargetDirectives.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Bundle/MaterialIdMapper.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp"

#include <algorithm>
#include <functional>
#include <utility>

namespace enigma::graphic
{
    namespace
    {
        void AddCustomImageSlot(std::vector<int>& slots, int slot)
        {
            if (slot < 0)
            {
                return;
            }

            if (std::find(slots.begin(), slots.end(), slot) == slots.end())
            {
                slots.push_back(slot);
            }
        }

        void CollectBundleCustomImageSlots(const ShaderBundle* bundle, std::vector<int>& slots)
        {
            if (bundle == nullptr)
            {
                return;
            }

            const CustomTextureData& textureData = bundle->GetCustomTextureData();
            for (const StageTextureBinding& binding : textureData.stageBindings)
            {
                AddCustomImageSlot(slots, binding.textureSlot);
            }
            for (const CustomTextureBinding& binding : textureData.customBindings)
            {
                AddCustomImageSlot(slots, binding.textureSlot);
            }
        }

        void AppendRenderTargetConfigs(
            std::vector<IndexedRenderTargetConfig>& output,
            int maxIndex,
            const std::function<bool(int)>& hasConfig,
            const std::function<RenderTargetConfig(int)>& getConfig)
        {
            for (int index = 0; index <= maxIndex; ++index)
            {
                if (!hasConfig(index))
                {
                    continue;
                }

                IndexedRenderTargetConfig indexedConfig;
                indexedConfig.index  = index;
                indexedConfig.config = getConfig(index);
                output.push_back(indexedConfig);
            }
        }
    }

    ShaderBundleReloadAdapter::ShaderBundleReloadAdapter(ShaderBundleSubsystem& subsystem) noexcept
        : m_subsystem(&subsystem)
    {
    }

    ShaderBundleReloadAdapter::PreparedShaderBundle ShaderBundleReloadAdapter::PrepareLoad(const ShaderBundleMeta& meta) const
    {
        if (m_subsystem == nullptr)
        {
            throw ShaderBundleException("ShaderBundleReloadAdapter has no subsystem");
        }

        auto pathAliases = m_subsystem->m_config.GetPathAliasMap();

        PreparedShaderBundle prepared;
        prepared.action    = PreparedShaderBundleAction::Load;
        prepared.meta      = meta;
        prepared.debugName = meta.name;
        prepared.bundle    = std::make_shared<ShaderBundle>(meta, m_subsystem->m_engineBundle, pathAliases);

        auto* mapper = prepared.bundle->GetMaterialIdMapper();
        const auto blockPropertiesPath = meta.path / "shaders" / "block.properties";
        if (!mapper->Load(blockPropertiesPath))
        {
            LogWarn(LogShaderBundle,
                    "ShaderBundleReloadAdapter:: MaterialIdMapper load failed or empty for '%s' (non-blocking)",
                    meta.name.c_str());
        }

        return prepared;
    }

    ShaderBundleReloadAdapter::PreparedShaderBundle ShaderBundleReloadAdapter::PrepareUnloadToEngine() const
    {
        if (m_subsystem == nullptr || m_subsystem->m_engineBundle == nullptr)
        {
            throw ShaderBundleException("ShaderBundleReloadAdapter cannot unload because engine bundle is unavailable");
        }

        PreparedShaderBundle prepared;
        prepared.action    = PreparedShaderBundleAction::UnloadToEngine;
        prepared.bundle    = m_subsystem->m_engineBundle;
        prepared.meta      = m_subsystem->m_engineBundle->GetMeta();
        prepared.debugName = m_subsystem->m_engineBundle->GetName();

        auto* mapper = prepared.bundle->GetMaterialIdMapper();
        const auto blockPropertiesPath = prepared.bundle->GetMeta().path / "shaders" / "block.properties";
        if (!mapper->Load(blockPropertiesPath))
        {
            LogWarn(LogShaderBundle,
                    "ShaderBundleReloadAdapter:: Engine MaterialIdMapper load failed or empty (non-blocking)");
        }

        return prepared;
    }

    RendererFrontendReloadScope ShaderBundleReloadAdapter::BuildFrontendReloadScope(const PreparedShaderBundle& prepared) const
    {
        if (!prepared.IsValid())
        {
            throw ShaderBundleException("ShaderBundleReloadAdapter cannot build frontend scope from invalid prepared bundle");
        }

        RendererFrontendReloadScope scope;
        CollectBundleCustomImageSlots(
            m_subsystem != nullptr ? m_subsystem->m_currentBundle.get() : nullptr,
            scope.clearCustomImageSlots);

        if (prepared.action == PreparedShaderBundleAction::UnloadToEngine)
        {
            scope.resetRenderTargetsToEngineDefaults = true;
            return scope;
        }

        const PackRenderTargetDirectives* rtDirectives = prepared.bundle->GetRTDirectives();
        if (rtDirectives != nullptr)
        {
            AppendRenderTargetConfigs(
                scope.colorTexConfigs,
                rtDirectives->GetMaxColorTexIndex(),
                [rtDirectives](int index) { return rtDirectives->HasColorTexConfig(index); },
                [rtDirectives](int index) { return rtDirectives->GetColorTexConfig(index); });
            AppendRenderTargetConfigs(
                scope.depthTexConfigs,
                rtDirectives->GetMaxDepthTexIndex(),
                [rtDirectives](int index) { return rtDirectives->HasDepthTexConfig(index); },
                [rtDirectives](int index) { return rtDirectives->GetDepthTexConfig(index); });
            AppendRenderTargetConfigs(
                scope.shadowColorConfigs,
                rtDirectives->GetMaxShadowColorIndex(),
                [rtDirectives](int index) { return rtDirectives->HasShadowColorConfig(index); },
                [rtDirectives](int index) { return rtDirectives->GetShadowColorConfig(index); });
            AppendRenderTargetConfigs(
                scope.shadowTexConfigs,
                rtDirectives->GetMaxShadowTexIndex(),
                [rtDirectives](int index) { return rtDirectives->HasShadowTexConfig(index); },
                [rtDirectives](int index) { return rtDirectives->GetShadowTexConfig(index); });
        }

        const std::optional<int> shadowResolution = prepared.bundle->GetConstInt("shadowMapResolution");
        if (shadowResolution.has_value() && shadowResolution.value() > 0 && shadowResolution.value() <= 16384)
        {
            scope.shadowMapResolution = shadowResolution.value();
        }

        return scope;
    }

    void ShaderBundleReloadAdapter::ApplyPreparedBundle(PreparedShaderBundle&& prepared)
    {
        if (m_subsystem == nullptr || !prepared.IsValid())
        {
            throw ShaderBundleException("ShaderBundleReloadAdapter cannot apply invalid prepared bundle");
        }

        if (m_subsystem->m_onBuildVertexHandle != 0)
        {
            TerrainVertexLayout::OnBuildVertexLayout.Remove(m_subsystem->m_onBuildVertexHandle);
            m_subsystem->m_onBuildVertexHandle = 0;
        }

        auto* mapper = prepared.bundle->GetMaterialIdMapper();
        m_subsystem->m_onBuildVertexHandle = TerrainVertexLayout::OnBuildVertexLayout.Add(
            mapper,
            &MaterialIdMapper::OnBuildVertex);

        m_subsystem->m_currentBundle = std::move(prepared.bundle);
    }

    void ShaderBundleReloadAdapter::RollbackToStableBundle(const StableShaderBundleSnapshot& snapshot)
    {
        if (m_subsystem == nullptr || snapshot.bundle == nullptr)
        {
            throw ShaderBundleException("ShaderBundleReloadAdapter cannot rollback without a stable bundle snapshot");
        }

        PreparedShaderBundle prepared;
        prepared.action    = snapshot.wasEngineBundle ? PreparedShaderBundleAction::UnloadToEngine : PreparedShaderBundleAction::Load;
        prepared.bundle    = snapshot.bundle;
        prepared.meta      = snapshot.meta;
        prepared.debugName = snapshot.debugName;

        ApplyPreparedBundle(std::move(prepared));
    }

    StableShaderBundleSnapshot ShaderBundleReloadAdapter::CaptureStableSnapshot() const
    {
        StableShaderBundleSnapshot snapshot;
        if (m_subsystem == nullptr || m_subsystem->m_currentBundle == nullptr)
        {
            return snapshot;
        }

        snapshot.bundle          = m_subsystem->m_currentBundle;
        snapshot.meta            = m_subsystem->m_currentBundle->GetMeta();
        snapshot.wasEngineBundle = m_subsystem->m_currentBundle == m_subsystem->m_engineBundle ||
                                   m_subsystem->m_currentBundle->IsEngineBundle();
        snapshot.debugName       = m_subsystem->m_currentBundle->GetName();
        return snapshot;
    }

    void ShaderBundleReloadAdapter::SaveCommittedRequest(const RenderPipelineReloadRequest& request)
    {
        if (m_subsystem == nullptr)
        {
            throw ShaderBundleException("ShaderBundleReloadAdapter cannot save committed request without subsystem");
        }

        if (request.IsUnloadRequest())
        {
            m_subsystem->m_config.SetCurrentLoadedBundle("");
            m_subsystem->m_config.SaveToYaml(ShaderBundleSubsystem::CONFIG_FILE_PATH);
            return;
        }

        if (!request.shaderBundleMeta.has_value())
        {
            throw ShaderBundleException("ShaderBundleReloadAdapter cannot save load request without bundle metadata");
        }

        m_subsystem->m_config.SetCurrentLoadedBundle(request.shaderBundleMeta->name);
        m_subsystem->m_config.SaveToYaml(ShaderBundleSubsystem::CONFIG_FILE_PATH);
    }
} // namespace enigma::graphic
