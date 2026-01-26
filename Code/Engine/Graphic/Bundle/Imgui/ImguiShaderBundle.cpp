// ============================================================================
// ImguiShaderBundle.cpp - Main panel composing Info and Selector modules
// ============================================================================

#include "ImguiShaderBundle.hpp"
#include "ImguiShaderBundleInfo.hpp"
#include "ImguiShaderBundleSelector.hpp"

#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace enigma::graphic
{
    void ImguiShaderBundle::Show(ShaderBundleSubsystem* subsystem)
    {
        if (!subsystem)
            return;

        if (ImGui::Begin("Shader Bundle"))
        {
            // Section 1: Current Bundle Info
            if (ImGui::CollapsingHeader("Current Bundle Info", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImguiShaderBundleInfo::Show(subsystem);
            }

            // Section 2: Bundle Selector
            if (ImGui::CollapsingHeader("Bundle Selector", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImguiShaderBundleSelector::Show(subsystem);
            }
        }
        ImGui::End();
    }
} // namespace enigma::graphic
