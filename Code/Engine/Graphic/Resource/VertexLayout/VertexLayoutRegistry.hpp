#pragma once

// ============================================================================
// VertexLayoutRegistry.hpp - Static registry for VertexLayout management
// 
// Design Notes:
// - Static class pattern (like D3D12RenderSystem)
// - No instantiation allowed: constructor deleted
// - Manages lifecycle of all VertexLayout instances
// - Called by RendererSubsystem::Startup/Shutdown
// ============================================================================

#include "VertexLayout.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace enigma::graphic
{
    /**
     * @brief Static registry for VertexLayout management
     * 
     * Core responsibilities:
     * 1. Manage lifecycle of all VertexLayout instances
     * 2. Provide registration and query APIs
     * 3. Set default layout for fallback
     * 
     * Pattern: Static class (like D3D12RenderSystem)
     * - All methods are static
     * - No instantiation allowed
     * - Static data members defined in .cpp file
     * 
     * Usage:
     *   // During engine startup
     *   VertexLayoutRegistry::Initialize();
     *   
     *   // Query layout by name
     *   const VertexLayout* layout = VertexLayoutRegistry::GetLayout("Vertex_PCUTBN");
     *   
     *   // Register custom layout (e.g., from Game RenderPass)
     *   VertexLayoutRegistry::RegisterLayout(std::make_unique<TerrainVertexLayout>());
     *   
     *   // During engine shutdown
     *   VertexLayoutRegistry::Shutdown();
     */
    class VertexLayoutRegistry
    {
    public:
        // ========================================================================
        // [REQUIRED] Delete constructor - Static class pattern
        // ========================================================================
        VertexLayoutRegistry()                                       = delete;
        ~VertexLayoutRegistry()                                      = delete;
        VertexLayoutRegistry(const VertexLayoutRegistry&)            = delete;
        VertexLayoutRegistry& operator=(const VertexLayoutRegistry&) = delete;

        // ========================================================================
        // Lifecycle Management - Called by RendererSubsystem
        // ========================================================================

        /**
         * @brief Initialize the registry
         * 
         * Called by RendererSubsystem::Startup().
         * Registers predefined layouts (Vertex_PCU, Vertex_PCUTBN).
         * Safe to call multiple times (logs warning on subsequent calls).
         */
        static void Initialize();

        /**
         * @brief Shutdown the registry
         * 
         * Called by RendererSubsystem::Shutdown().
         * Clears all registered layouts and resets state.
         */
        static void Shutdown();

        /**
         * @brief Check if registry is initialized
         * @return true if Initialize() has been called
         */
        static bool IsInitialized() { return s_initialized; }

        // ========================================================================
        // Registration API
        // ========================================================================

        /**
         * @brief Register a VertexLayout
         * @param layout Unique pointer to the layout (ownership transferred)
         * 
         * Behavior:
         * - Transfers ownership to registry
         * - Skips duplicate registration with warning (no exception)
         * - Null layout is silently ignored
         */
        static void RegisterLayout(std::unique_ptr<VertexLayout> layout);

        // ========================================================================
        // Query API
        // ========================================================================

        /**
         * @brief Get layout by name
         * @param name Layout name to search for
         * @return Pointer to layout, or nullptr if not found (no exception)
         */
        [[nodiscard]] static const VertexLayout* GetLayout(const std::string& name);

        /**
         * @brief Get all registered layouts
         * @return Vector of pointers to all layouts
         */
        [[nodiscard]] static std::vector<const VertexLayout*> GetAllLayouts();

        /**
         * @brief Get default layout for fallback
         * @return Pointer to default layout (Vertex_PCUTBN after Initialize)
         */
        [[nodiscard]] static const VertexLayout* GetDefault() { return s_defaultLayout; }

    private:
        // ========================================================================
        // Private Helper
        // ========================================================================

        /**
         * @brief Register predefined engine layouts
         * 
         * Called by Initialize().
         * Registers: Vertex_PCUTBN (default), Vertex_PCU
         * [NOTE] Terrain and Cloud layouts are registered by Game RenderPass
         */
        static void RegisterPredefinedLayouts();

        // ========================================================================
        // Static Data Members (defined in .cpp)
        // ========================================================================

        static std::unordered_map<std::string, std::unique_ptr<VertexLayout>> s_layouts;
        static const VertexLayout*                                            s_defaultLayout;
        static bool                                                           s_initialized;
    };
} // namespace enigma::graphic
