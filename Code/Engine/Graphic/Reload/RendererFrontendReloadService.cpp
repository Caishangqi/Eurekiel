#include "Engine/Graphic/Reload/RendererFrontendReloadService.hpp"

#include "Engine/Core/StringUtils.hpp"
#include "Engine/Graphic/Integration/RendererFrontendReloadScope.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Reload/RenderPipelineReloadException.hpp"

#include <exception>

namespace enigma::graphic
{
    RendererFrontendReloadService::RendererFrontendReloadService(RendererSubsystem& renderer) noexcept
        : m_renderer(&renderer)
    {
    }

    void RendererFrontendReloadService::ApplyReloadScope(const RendererFrontendReloadScope& scope)
    {
        if (m_renderer == nullptr)
        {
            throw ReloadFrontendApplyException("RendererFrontendReloadService has no renderer");
        }

        try
        {
            m_renderer->ReloadFrontendState(scope);
        }
        catch (const RenderPipelineReloadException&)
        {
            throw;
        }
        catch (const std::exception& e)
        {
            throw ReloadFrontendApplyException(Stringf("Renderer frontend reload failed: %s", e.what()));
        }
    }
} // namespace enigma::graphic
