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
/**
 * @brief A helper structure to encapsulate and manage HRESULT values in Windows API operations.
 *
 * The HResultGrabber is used to handle and store the result of Windows API calls that return HRESULT.
 * It provides a utility for tracking failure codes and facilitates integration with error-checking mechanisms.
 *
 * This structure is particularly useful in scenarios where HRESULT values need to be checked for success or failure.
 */
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
