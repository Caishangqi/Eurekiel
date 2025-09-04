#include "GraphicsError.hpp"

#include "API/DX12Renderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"

CheckerToken chk;

HResultGrabber::HResultGrabber(unsigned int hr) noexcept : hr(hr)
{
}
