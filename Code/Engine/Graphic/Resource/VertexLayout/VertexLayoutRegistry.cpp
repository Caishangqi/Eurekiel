#include "VertexLayoutRegistry.hpp"
#include "VertexLayoutCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

// Concrete layout headers
#include "Layouts/Vertex_PCULayout.hpp"
#include "Layouts/Vertex_PCUTBNLayout.hpp"

namespace enigma::graphic
{
    // ============================================================================
    // Static Member Definitions
    // ============================================================================

    std::unordered_map<std::string, std::unique_ptr<VertexLayout>> VertexLayoutRegistry::s_layouts;
    const VertexLayout*                                            VertexLayoutRegistry::s_defaultLayout = nullptr;
    bool                                                           VertexLayoutRegistry::s_initialized   = false;

    // ============================================================================
    // Lifecycle Management
    // ============================================================================

    void VertexLayoutRegistry::Initialize()
    {
        if (s_initialized)
        {
            LogWarn(LogVertexLayout, "VertexLayoutRegistry already initialized, skipping");
            return;
        }

        RegisterPredefinedLayouts();

        s_initialized = true;
        LogInfo(LogVertexLayout, "VertexLayoutRegistry initialized with %zu layouts", s_layouts.size());
    }

    void VertexLayoutRegistry::Shutdown()
    {
        if (!s_initialized)
        {
            LogWarn(LogVertexLayout, "VertexLayoutRegistry not initialized, skipping shutdown");
            return;
        }

        // Clear all layouts
        s_layouts.clear();
        s_defaultLayout = nullptr;
        s_initialized   = false;

        LogInfo(LogVertexLayout, "VertexLayoutRegistry shutdown complete");
    }

    // ============================================================================
    // Registration API
    // ============================================================================

    void VertexLayoutRegistry::RegisterLayout(std::unique_ptr<VertexLayout> layout)
    {
        if (!layout)
        {
            return; // Silently ignore null layout
        }

        std::string name = layout->GetLayoutName();

        // Check for duplicate registration
        auto it = s_layouts.find(name);
        if (it != s_layouts.end())
        {
            LogWarn(LogVertexLayout, "Layout '%s' already registered, skipping", name.c_str());
            return;
        }

        // Transfer ownership to registry
        s_layouts[name] = std::move(layout);
        LogInfo(LogVertexLayout, "Layout '%s' registered successfully", name.c_str());
    }

    // ============================================================================
    // Query API
    // ============================================================================

    const VertexLayout* VertexLayoutRegistry::GetLayout(const std::string& name)
    {
        auto it = s_layouts.find(name);
        if (it == s_layouts.end())
        {
            return nullptr; // No exception, return nullptr
        }
        return it->second.get();
    }

    std::vector<const VertexLayout*> VertexLayoutRegistry::GetAllLayouts()
    {
        std::vector<const VertexLayout*> result;
        result.reserve(s_layouts.size());

        for (const auto& pair : s_layouts)
        {
            result.push_back(pair.second.get());
        }

        return result;
    }

    // ============================================================================
    // Private Helper
    // ============================================================================

    void VertexLayoutRegistry::RegisterPredefinedLayouts()
    {
        // Create Vertex_PCUTBN layout (default)
        auto pcutbnLayout = std::make_unique<Vertex_PCUTBNLayout>();
        s_defaultLayout   = pcutbnLayout.get();
        RegisterLayout(std::move(pcutbnLayout));

        // Create Vertex_PCU layout
        RegisterLayout(std::make_unique<Vertex_PCULayout>());

        // [NOTE] Terrain and Cloud layouts are registered by Game RenderPass
        LogInfo(LogVertexLayout, "RegisterPredefinedLayouts: Vertex_PCUTBN (default) and Vertex_PCU registered");
    }
} // namespace enigma::graphic
