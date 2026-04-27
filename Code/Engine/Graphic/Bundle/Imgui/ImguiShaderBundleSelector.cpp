// ============================================================================
// ImguiShaderBundleSelector.cpp - Static ImGui module for Bundle selection
// ============================================================================

#include "ImguiShaderBundleSelector.hpp"

#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace enigma::graphic
{
    void ImguiShaderBundleSelector::Show(ShaderBundleSubsystem* subsystem)
    {
        if (!subsystem)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] ShaderBundleSubsystem is null");
            return;
        }

        // Static selection index for ImGui immediate mode
        static int s_selectedIndex = 0;

        // Get discovered bundles
        std::vector<ShaderBundleMeta> discoveredBundles = subsystem->ListDiscoveredShaderBundles();

        // Refresh button
        if (ImGui::Button("Refresh"))
        {
            subsystem->RefreshDiscoveredShaderBundles();
            s_selectedIndex = 0;
        }

        ImGui::SameLine();
        ImGui::Text("Found: %zu bundle(s)", discoveredBundles.size());

        ImGui::Separator();

        // Handle empty bundle list
        if (discoveredBundles.empty())
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No user bundles discovered.");
            ImGui::TextWrapped("Place shader bundles in .enigma/shaderpacks/");
            return;
        }

        // Build combo items string
        std::string comboItems;
        for (const auto& meta : discoveredBundles)
        {
            comboItems += meta.name;
            comboItems += '\0';
        }
        comboItems += '\0';

        // Clamp selection index
        if (s_selectedIndex >= static_cast<int>(discoveredBundles.size()))
        {
            s_selectedIndex = 0;
        }

        // Bundle selection dropdown
        ImGui::Text("Select Bundle:");
        if (ImGui::Combo("##BundleSelector", &s_selectedIndex, comboItems.c_str()))
        {
            // Selection changed - no auto-load
        }

        // Show selected bundle path
        if (s_selectedIndex >= 0 && s_selectedIndex < static_cast<int>(discoveredBundles.size()))
        {
            const auto& selectedMeta = discoveredBundles[s_selectedIndex];
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Path: %s", selectedMeta.path.string().c_str());
        }

        ImGui::Separator();

        // Check if user bundle is loaded
        bool hasUserBundleLoaded = false;
        auto currentBundle       = subsystem->GetCurrentShaderBundle();
        if (currentBundle)
        {
            hasUserBundleLoaded = !currentBundle->GetMeta().isEngineBundle;
        }

        bool selectedBundleIsCurrent = false;
        if (s_selectedIndex >= 0 && s_selectedIndex < static_cast<int>(discoveredBundles.size()))
        {
            selectedBundleIsCurrent = subsystem->IsCurrentShaderBundle(discoveredBundles[s_selectedIndex]);
        }

        if (selectedBundleIsCurrent)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Load Selected"))
        {
            if (s_selectedIndex >= 0 && s_selectedIndex < static_cast<int>(discoveredBundles.size()))
            {
                const auto& selectedMeta = discoveredBundles[s_selectedIndex];
                // Reload coordinator serializes accepted requests and ignores busy ones.
                subsystem->RequestLoadShaderBundle(selectedMeta);
            }
        }

        if (selectedBundleIsCurrent)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        // Unload button (disabled if engine bundle is current)
        if (!hasUserBundleLoaded)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Unload"))
        {
            // Reload coordinator serializes accepted requests and ignores busy ones.
            subsystem->RequestUnloadShaderBundle();
        }

        if (!hasUserBundleLoaded)
        {
            ImGui::EndDisabled();
        }

        if (selectedBundleIsCurrent || !hasUserBundleLoaded)
        {
            ImGui::SameLine();
            ImGui::TextColored(
                ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                selectedBundleIsCurrent ? "(Current bundle active)" : "(Engine bundle active)");
        }
    }
} // namespace enigma::graphic
