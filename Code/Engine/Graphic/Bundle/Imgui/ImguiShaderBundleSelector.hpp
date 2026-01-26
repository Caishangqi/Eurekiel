#pragma once

// ============================================================================
// ImguiShaderBundleSelector.hpp - Static ImGui module for Bundle selection
// ============================================================================

namespace enigma::graphic
{
    class ShaderBundleSubsystem;

    // Static ImGui class for Bundle selection dropdown and operations
    class ImguiShaderBundleSelector
    {
    public:
        ImguiShaderBundleSelector()                                            = delete;
        ImguiShaderBundleSelector(const ImguiShaderBundleSelector&)            = delete;
        ImguiShaderBundleSelector& operator=(const ImguiShaderBundleSelector&) = delete;

        // Display Bundle selector panel (dropdown, Load/Unload/Refresh buttons)
        static void Show(ShaderBundleSubsystem* subsystem);
    };
} // namespace enigma::graphic
