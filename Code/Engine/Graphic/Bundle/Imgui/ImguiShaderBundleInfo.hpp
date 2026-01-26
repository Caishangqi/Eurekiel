#pragma once

// ============================================================================
// ImguiShaderBundleInfo.hpp - [NEW] Static ImGui module for Bundle info display
// ============================================================================

namespace enigma::graphic
{
    class ShaderBundleSubsystem;

    // [NEW] Static ImGui class for displaying current Bundle metadata
    class ImguiShaderBundleInfo
    {
    public:
        ImguiShaderBundleInfo()                                        = delete;
        ImguiShaderBundleInfo(const ImguiShaderBundleInfo&)            = delete;
        ImguiShaderBundleInfo& operator=(const ImguiShaderBundleInfo&) = delete;

        // Display current Bundle info panel (name, author, description, isEngineBundle)
        static void Show(ShaderBundleSubsystem* subsystem);
    };
} // namespace enigma::graphic
