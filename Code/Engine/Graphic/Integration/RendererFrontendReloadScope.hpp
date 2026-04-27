#pragma once

#include "Engine/Graphic/Target/RTTypes.hpp"

#include <optional>
#include <vector>

namespace enigma::graphic
{
    struct IndexedRenderTargetConfig
    {
        int                index = 0;
        RenderTargetConfig config;
    };

    struct RendererFrontendReloadScope
    {
        bool resetRenderTargetsToEngineDefaults = false;

        std::vector<IndexedRenderTargetConfig> colorTexConfigs;
        std::vector<IndexedRenderTargetConfig> depthTexConfigs;
        std::vector<IndexedRenderTargetConfig> shadowColorConfigs;
        std::vector<IndexedRenderTargetConfig> shadowTexConfigs;

        std::optional<int> shadowMapResolution;
        std::vector<int>   clearCustomImageSlots;

        bool HasExplicitRenderTargetConfigs() const noexcept
        {
            return !colorTexConfigs.empty() ||
                   !depthTexConfigs.empty() ||
                   !shadowColorConfigs.empty() ||
                   !shadowTexConfigs.empty();
        }

        bool HasRenderTargetConfigConflict() const noexcept
        {
            return resetRenderTargetsToEngineDefaults && HasExplicitRenderTargetConfigs();
        }
    };
} // namespace enigma::graphic
