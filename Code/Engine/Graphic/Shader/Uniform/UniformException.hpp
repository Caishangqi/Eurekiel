#pragma once
//-----------------------------------------------------------------------------------------------
// UniformException.hpp
//
// [NEW] Exception hierarchy for Uniform module
//
// This header provides:
//   - UniformException base class inheriting from std::exception
//   - SlotConflictException for duplicate slot registration
//   - InvalidSpaceException for invalid BufferSpace parameter
//   - BufferNotRegisteredException for upload to unregistered buffer
//
// Usage:
//   try {
//       if (slotOccupied) throw SlotConflictException("Slot already in use");
//   } catch (const UniformException& e) {
//       LogError(LogUniform, e.what());
//   }
//
// Teaching Points:
//   - std::exception inheritance for standard C++ exception handling
//   - Constructor inheritance via using-declaration (C++11)
//   - Exception hierarchy enables catch-by-base or catch-by-derived
//
//-----------------------------------------------------------------------------------------------

#include <exception>
#include <string>

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // UniformException
    // Base exception class for all Uniform module errors
    //
    // [NEW] Inherits from std::exception for standard C++ exception handling
    // Default mapping: ERROR_AND_DIE (fatal error)
    //-------------------------------------------------------------------------------------------
    class UniformException : public std::exception
    {
    public:
        explicit UniformException(const std::string& message)
            : m_message(message)
        {
        }

        const char* what() const noexcept override
        {
            return m_message.c_str();
        }

    private:
        std::string m_message;
    };

    //-------------------------------------------------------------------------------------------
    // SlotConflictException
    // Thrown when registering buffer to an already occupied slot
    //
    // Typical scenarios:
    //   - RegisterBuffer called with slot that already has a buffer
    //   - Duplicate buffer registration attempt
    //
    // Mapping: ERROR_AND_DIE (fatal configuration error)
    //-------------------------------------------------------------------------------------------
    class SlotConflictException : public UniformException
    {
    public:
        using UniformException::UniformException;
    };

    //-------------------------------------------------------------------------------------------
    // InvalidSpaceException
    // Thrown when BufferSpace parameter validation fails
    //
    // Typical scenarios:
    //   - Slot exceeds ENGINE_BUFFER_MAX_SLOT for BufferSpace::Engine
    //   - Slot exceeds CUSTOM_BUFFER_MAX_SLOT for BufferSpace::Custom
    //   - Invalid BufferSpace enum value
    //
    // Mapping: ERROR_AND_DIE (fatal configuration error)
    //-------------------------------------------------------------------------------------------
    class InvalidSpaceException : public UniformException
    {
    public:
        using UniformException::UniformException;
    };

    //-------------------------------------------------------------------------------------------
    // BufferNotRegisteredException
    // Thrown when uploading data to a buffer that was not registered
    //
    // Typical scenarios:
    //   - UploadBuffer called before RegisterBuffer
    //   - Buffer was unregistered or destroyed
    //
    // Mapping: ERROR_RECOVERABLE (skip upload, rendering may continue)
    //-------------------------------------------------------------------------------------------
    class BufferNotRegisteredException : public UniformException
    {
    public:
        using UniformException::UniformException;
    };
} // namespace enigma::graphic
