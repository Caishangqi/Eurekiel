#pragma once

// ============================================================================
// VertexLayoutException.hpp - Exception hierarchy for VertexLayout system
// 
// Provides type-safe exception classes for vertex layout related errors:
// - VertexLayoutException: Base exception class
// - VertexLayoutNotFoundException: Layout name not found in registry
// - VertexLayoutMismatchException: Stride or format mismatch
// - VertexLayoutRegistrationException: Registration failed
// ============================================================================

#include <exception>
#include <string>

namespace enigma::graphic
{
    // ========================================================================
    // VertexLayoutException - Base exception class
    // ========================================================================

    /**
     * @brief Base exception class for VertexLayout system errors
     * 
     * Inherits from std::exception for standard C++ exception handling.
     * All VertexLayout-related exceptions derive from this class.
     */
    class VertexLayoutException : public std::exception
    {
    public:
        explicit VertexLayoutException(const std::string& message)
            : m_message(message)
        {
        }

        const char* what() const noexcept override
        {
            return m_message.c_str();
        }

    protected:
        std::string m_message;
    };

    // ========================================================================
    // VertexLayoutNotFoundException - Layout not found in registry
    // ========================================================================

    /**
     * @brief Exception thrown when a requested layout is not found in registry
     * 
     * Example:
     *   throw VertexLayoutNotFoundException("Terrain");
     *   // Message: "VertexLayout not found: 'Terrain'"
     */
    class VertexLayoutNotFoundException : public VertexLayoutException
    {
    public:
        explicit VertexLayoutNotFoundException(const std::string& layoutName)
            : VertexLayoutException("VertexLayout not found: '" + layoutName + "'")
              , m_layoutName(layoutName)
        {
        }

        const std::string& GetLayoutName() const { return m_layoutName; }

    private:
        std::string m_layoutName;
    };

    // ========================================================================
    // VertexLayoutMismatchException - Stride or format mismatch
    // ========================================================================

    /**
     * @brief Exception thrown when vertex buffer stride doesn't match layout stride
     * 
     * Example:
     *   throw VertexLayoutMismatchException("Vertex_PCUTBN", "TerrainVertex");
     *   // Message: "VertexLayout mismatch: expected 'Vertex_PCUTBN', got 'TerrainVertex'"
     */
    class VertexLayoutMismatchException : public VertexLayoutException
    {
    public:
        VertexLayoutMismatchException(const std::string& expected, const std::string& actual)
            : VertexLayoutException("VertexLayout mismatch: expected '" + expected + "', got '" + actual + "'")
              , m_expected(expected)
              , m_actual(actual)
        {
        }

        const std::string& GetExpected() const { return m_expected; }
        const std::string& GetActual() const { return m_actual; }

    private:
        std::string m_expected;
        std::string m_actual;
    };

    // ========================================================================
    // VertexLayoutRegistrationException - Registration failed
    // ========================================================================

    /**
     * @brief Exception thrown when layout registration fails
     * 
     * Example:
     *   throw VertexLayoutRegistrationException("Layout 'Custom' already exists");
     *   // Message: "VertexLayout registration failed: Layout 'Custom' already exists"
     */
    class VertexLayoutRegistrationException : public VertexLayoutException
    {
    public:
        explicit VertexLayoutRegistrationException(const std::string& reason)
            : VertexLayoutException("VertexLayout registration failed: " + reason)
              , m_reason(reason)
        {
        }

        const std::string& GetReason() const { return m_reason; }

    private:
        std::string m_reason;
    };
} // namespace enigma::graphic
