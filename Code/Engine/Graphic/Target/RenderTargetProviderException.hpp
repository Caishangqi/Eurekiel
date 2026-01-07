#pragma once

// [NEW] Exception classes for RenderTarget Provider module
// Part of RenderTarget Manager Architecture Refactoring

#include <stdexcept>
#include <string>

namespace enigma::graphic
{
    // ========== Base Exception ==========

    /// Base exception for all RenderTarget Provider errors
    class RenderTargetProviderException : public std::runtime_error
    {
    public:
        explicit RenderTargetProviderException(const std::string& message);
    };

    // ========== Derived Exceptions ==========

    /// Index out of bounds (recoverable)
    class InvalidIndexException : public RenderTargetProviderException
    {
    public:
        InvalidIndexException(const std::string& providerName, int index, int maxIndex);
    };

    /// Resource not initialized (fatal)
    class ResourceNotReadyException : public RenderTargetProviderException
    {
    public:
        explicit ResourceNotReadyException(const std::string& resourceName);
    };

    /// Copy operation failed (recoverable)
    class CopyOperationFailedException : public RenderTargetProviderException
    {
    public:
        CopyOperationFailedException(const std::string& providerName, int srcIndex, int dstIndex);
    };

    /// Invalid RT binding configuration (recoverable)
    class InvalidBindingException : public RenderTargetProviderException
    {
    public:
        enum class Reason
        {
            DualDepthBinding, // Both ShadowTex and DepthTex bound
            MultipleShadowTex, // Two or more ShadowTex bound
            MultipleDepthTex // Two or more DepthTex bound
        };

        explicit InvalidBindingException(Reason reason);

        Reason GetReason() const { return m_reason; }

    private:
        static std::string GetMessage(Reason reason);

        Reason m_reason;
    };
} // namespace enigma::graphic
