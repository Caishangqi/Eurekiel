#include "SamplerProviderException.hpp"

// ============================================================================
// SamplerProviderException.cpp - [NEW] Exception implementations
// Part of Dynamic Sampler System
// ============================================================================

namespace enigma::graphic
{
    // ========== Base Exception ==========

    SamplerProviderException::SamplerProviderException(const std::string& message)
        : std::runtime_error(message)
    {
    }

    // ========== Derived Exceptions ==========

    InvalidSamplerIndexException::InvalidSamplerIndexException(
        const std::string& providerName, uint32_t index, uint32_t maxIndex)
        : SamplerProviderException(
            providerName + ":: Invalid sampler index " + std::to_string(index) +
            ", valid range [0, " + std::to_string(maxIndex - 1) + "]")
    {
    }

    SamplerHeapAllocationException::SamplerHeapAllocationException(const std::string& details)
        : SamplerProviderException("SamplerHeap:: Allocation failed - " + details)
    {
    }

    InvalidSamplerConfigException::InvalidSamplerConfigException(const std::string& reason)
        : SamplerProviderException("SamplerConfig:: Invalid configuration - " + reason)
    {
    }
} // namespace enigma::graphic
