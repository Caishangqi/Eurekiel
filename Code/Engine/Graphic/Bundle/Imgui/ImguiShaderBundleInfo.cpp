// ============================================================================
// ImguiShaderBundleInfo.cpp - [NEW] Static ImGui module for Bundle info display
// ============================================================================

#include "ImguiShaderBundleInfo.hpp"

#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace enigma::graphic
{
    void ImguiShaderBundleInfo::Show(ShaderBundleSubsystem* subsystem)
    {
        if (!subsystem)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] ShaderBundleSubsystem is null");
            return;
        }

        auto bundle = subsystem->GetCurrentShaderBundle();
        if (!bundle)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[WARNING] No active ShaderBundle");
            return;
        }

        const ShaderBundleMeta& meta = bundle->GetMeta();

        // Bundle type indicator
        if (meta.isEngineBundle)
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "[Engine Default]");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "[User Bundle]");
        }

        ImGui::Separator();

        // Metadata display
        ImGui::Text("Name:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", meta.name.c_str());

        if (!meta.author.empty())
        {
            ImGui::Text("Author:");
            ImGui::SameLine();
            ImGui::Text("%s", meta.author.c_str());
        }

        if (!meta.description.empty())
        {
            ImGui::Text("Description:");
            ImGui::TextWrapped("%s", meta.description.c_str());
        }

        ImGui::Separator();

        // UserDefinedBundle info
        size_t userBundleCount = bundle->GetUserBundleCount();
        ImGui::Text("UserDefinedBundles: %zu", userBundleCount);

        if (userBundleCount > 0)
        {
            std::string currentName = bundle->GetCurrentUserBundleName();
            ImGui::BulletText("Current: %s", currentName.empty() ? "(none)" : currentName.c_str());
        }

        // Fallback status
        ImGui::Text("Fallback Rules:");
        ImGui::SameLine();
        if (bundle->HasFallbackConfiguration())
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Enabled");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Not Configured");
        }
    }
} // namespace enigma::graphic
