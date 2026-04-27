#pragma once

#include "Engine/Graphic/Bundle/ShaderBundleCommon.hpp"
#include "Engine/Graphic/Integration/RendererFrontendReloadScope.hpp"
#include "Engine/Graphic/Reload/RenderPipelineReloadTypes.hpp"

#include <memory>
#include <optional>
#include <string>

namespace enigma::graphic
{
    class ShaderBundle;
    class ShaderBundleSubsystem;

    class ShaderBundleReloadAdapter final
    {
    public:
        enum class PreparedShaderBundleAction
        {
            Load,
            UnloadToEngine
        };

        struct PreparedShaderBundle
        {
            PreparedShaderBundleAction      action = PreparedShaderBundleAction::Load;
            std::shared_ptr<ShaderBundle>   bundle;
            std::optional<ShaderBundleMeta> meta;
            std::string                     debugName;

            bool IsValid() const noexcept
            {
                return bundle != nullptr;
            }
        };

        explicit ShaderBundleReloadAdapter(ShaderBundleSubsystem& subsystem) noexcept;

        PreparedShaderBundle PrepareLoad(const ShaderBundleMeta& meta) const;
        PreparedShaderBundle PrepareUnloadToEngine() const;

        RendererFrontendReloadScope BuildFrontendReloadScope(const PreparedShaderBundle& prepared) const;
        void ApplyPreparedBundle(PreparedShaderBundle&& prepared);
        void RollbackToStableBundle(const StableShaderBundleSnapshot& snapshot);
        StableShaderBundleSnapshot CaptureStableSnapshot() const;
        void SaveCommittedRequest(const RenderPipelineReloadRequest& request);

    private:
        ShaderBundleSubsystem* m_subsystem = nullptr;
    };
} // namespace enigma::graphic
