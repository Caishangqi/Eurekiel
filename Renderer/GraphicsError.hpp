#pragma once
#include <ostream>

#include "API/DX12Renderer.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"

struct CheckerToken
{
};

extern CheckerToken chk;

struct HResultGrabber
{
    HResultGrabber() = default;
    HResultGrabber(unsigned int hr) noexcept;
    long hr;
};

inline void operator>>(HResultGrabber grabber, CheckerToken token)
{
    UNUSED(token)
    if (FAILED(grabber.hr))
    {
        ERROR_AND_DIE(Stringf("HRESULT failed with error code %l", grabber.hr))
    }
}
