#include "D3D12RenderSystem.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

using namespace enigma::graphic;
using namespace enigma::core;

void D3D12RenderSystem::InitializeRenderer(bool enableDebugLayer, bool enableGPUValidation)
{
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Initializing D3D12 render system...");
    UNUSED(enableDebugLayer)
    UNUSED(enableGPUValidation)
}

void D3D12RenderSystem::ShutdownRenderer()
{
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Shutting down D3D12 render system...");
}
