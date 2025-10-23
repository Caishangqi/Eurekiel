#pragma once
#include "Engine/Core/ImGui/IImGuiRenderContext.hpp"
#include "Engine/Renderer/IRenderer.hpp"

namespace enigma::graphic
{
    class RendererSubsystem; // Forward declaration
}

/**
 * @brief Adapter for RendererSubsystem to IImGuiRenderContext
 *
 * Wraps RendererSubsystem* and implements IImGuiRenderContext interface
 * by delegating to RendererSubsystem's getter methods.
 *
 * Design Pattern: Adapter Pattern
 * - Adapts RendererSubsystem (new architecture) to IImGuiRenderContext interface
 * - Delegates all calls to underlying RendererSubsystem
 * - Non-owning pointer design (lifetime managed by caller)
 *
 * Used by: Thesis project (deferred rendering with ShaderPack support)
 *
 * @note This adapter assumes RendererSubsystem has the following getter methods:
 *       - GetD3D12Device()
 *       - GetCurrentCommandList()
 *       - GetSRVHeap()
 *       - GetRTVFormat()
 *       - GetFramesInFlight()
 *       - GetCommandQueue()
 *       - IsInitialized()
 *       These methods will be added to RendererSubsystem in Task 7.
 */
class RendererSubsystemImGuiContext : public IImGuiRenderContext
{
public:
    /**
     * @brief Constructs adapter with non-owning pointer to RendererSubsystem
     * @param rendererSubsystem Pointer to RendererSubsystem (must remain valid during adapter lifetime)
     */
    explicit RendererSubsystemImGuiContext(enigma::graphic::RendererSubsystem* rendererSubsystem);

    /**
     * @brief Destructor (default, no cleanup needed for non-owning pointer)
     */
    ~RendererSubsystemImGuiContext() override = default;

    // Prevent copying (adapter should not be copied due to pointer semantics)
    RendererSubsystemImGuiContext(const RendererSubsystemImGuiContext&)            = delete;
    RendererSubsystemImGuiContext& operator=(const RendererSubsystemImGuiContext&) = delete;

    // Allow moving (safe with non-owning pointer)
    RendererSubsystemImGuiContext(RendererSubsystemImGuiContext&&) noexcept            = default;
    RendererSubsystemImGuiContext& operator=(RendererSubsystemImGuiContext&&) noexcept = default;

    // IImGuiRenderContext interface implementation
    void*                                        GetDevice() const override;
    void*                                        GetCommandList() const override;
    void*                                        GetSRVHeap() const override;
    DXGI_FORMAT                                  GetRTVFormat() const override;
    uint32_t                                     GetNumFramesInFlight() const override;
    std::unique_ptr<enigma::core::IImGuiBackend> CreateBackend() override; // Factory method
    bool                                         IsReady() const override;
    void*                                        GetCommandQueue() const override;

private:
    enigma::graphic::RendererSubsystem* m_rendererSubsystem; ///< Non-owning pointer to RendererSubsystem
};
