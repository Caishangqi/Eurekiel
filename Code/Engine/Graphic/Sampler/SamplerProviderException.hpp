#pragma once

// ============================================================================
// SamplerProviderException.hpp - [NEW] Exception classes for Sampler Provider module
// Part of Dynamic Sampler System
// ============================================================================

#include <stdexcept>
#include <string>
#include <cstdint>

namespace enigma::graphic
{
    // ========== Base Exception ==========

    /// Base exception for all Sampler Provider errors
    class SamplerProviderException : public std::runtime_error
    {
    public:
        explicit SamplerProviderException(const std::string& message);
    };

    // ========== Derived Exceptions ==========

    /// Sampler index out of bounds (recoverable)
    class InvalidSamplerIndexException : public SamplerProviderException
    {
    public:
        InvalidSamplerIndexException(const std::string& providerName, uint32_t index, uint32_t maxIndex);
    };

    /// Sampler heap allocation failed (fatal)
    class SamplerHeapAllocationException : public SamplerProviderException
    {
    public:
        explicit SamplerHeapAllocationException(const std::string& details);
    };

    /// Sampler configuration invalid (recoverable)
    class InvalidSamplerConfigException : public SamplerProviderException
    {
    public:
        explicit InvalidSamplerConfigException(const std::string& reason);
    };
} // namespace enigma::graphic
