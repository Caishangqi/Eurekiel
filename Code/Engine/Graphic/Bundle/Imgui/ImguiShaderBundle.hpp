#pragma once

// ============================================================================
// ImguiShaderBundle.hpp - Main panel composing Info and Selector modules
// ============================================================================

namespace enigma::graphic
{
    class ShaderBundleSubsystem;

    // Static ImGui class for ShaderBundle debug window
    class ImguiShaderBundle
    {
    public:
        ImguiShaderBundle()                                    = delete;
        ImguiShaderBundle(const ImguiShaderBundle&)            = delete;
        ImguiShaderBundle& operator=(const ImguiShaderBundle&) = delete;

        // Display complete ShaderBundle debug window
        // Registered via g_theImGui->RegisterWindow() in ShaderBundleSubsystem::Startup()
        static void Show(ShaderBundleSubsystem* subsystem);
    };
} // namespace enigma::graphic
