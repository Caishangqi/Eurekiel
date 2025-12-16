#pragma once
//-----------------------------------------------------------------------------------------------
// BundleException.hpp
//
// [NEW] Centralized exception hierarchy for ShaderBundle module
//
// Summary of exception types and their mapping to error macros:
//
// | Exception Type              | Error Macro          | Description                           |
// |-----------------------------|----------------------|---------------------------------------|
// | ShaderBundleException       | ERROR_AND_DIE        | Base class, generic bundle error      |
// | ShaderNotFoundException     | ERROR_AND_DIE        | Shader missing, cannot continue       |
// | BundleNotFoundException     | ERROR_AND_DIE        | Bundle directory not found            |
// | InvalidBundleJsonException  | ERROR_RECOVERABLE    | JSON parse error, may use defaults    |
// | CompilationFailedException  | ERROR_AND_DIE        | Shader compilation failed             |
//
// Usage Pattern (Exception + Error Macro two-phase):
//   1. Throw Phase: Throw specific exception type from business code
//   2. Catch Phase: In catch block, apply corresponding error macro
//
// Example:
//   try {
//       auto* program = bundle->GetProgram("gbuffers_clouds");
//   } catch (const ShaderNotFoundException& e) {
//       ERROR_AND_DIE(e.what());  // Fatal - cannot render without shader
//   } catch (const InvalidBundleJsonException& e) {
//       ERROR_RECOVERABLE(e.what());  // May continue with defaults
//   }
//
// Teaching Points:
//   - Exception types provide semantic information about error nature
//   - Error macros (ERROR_AND_DIE, ERROR_RECOVERABLE) handle user notification
//   - This separation allows different error handling strategies at call sites
//   - Use GUARANTEE_* macros for simple condition checks without exceptions
//
//-----------------------------------------------------------------------------------------------

#include <exception>
#include <string>

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // ShaderBundleException
    // Base exception class for all ShaderBundle module errors
    //
    // [NEW] Inherits from std::exception for standard C++ exception handling
    // Default mapping: ERROR_AND_DIE (fatal error)
    //-------------------------------------------------------------------------------------------
    class ShaderBundleException : public std::exception
    {
    public:
        explicit ShaderBundleException(const std::string& message)
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
    // ShaderNotFoundException
    // Thrown when a requested shader program cannot be found in any fallback level
    //
    // Typical scenarios:
    //   - Shader file does not exist in bundle directory
    //   - Shader name not registered in program cache
    //   - All fallback levels exhausted without finding shader
    //
    // Mapping: ERROR_AND_DIE (rendering cannot proceed without required shader)
    //-------------------------------------------------------------------------------------------
    class ShaderNotFoundException : public ShaderBundleException
    {
    public:
        using ShaderBundleException::ShaderBundleException;
    };

    //-------------------------------------------------------------------------------------------
    // BundleNotFoundException
    // Thrown when a ShaderBundle directory does not exist or is inaccessible
    //
    // Typical scenarios:
    //   - Specified bundle path does not exist
    //   - Bundle directory missing required structure (shaders/bundle.json)
    //   - File permission issues
    //
    // Mapping: ERROR_AND_DIE (bundle is required for operation)
    //-------------------------------------------------------------------------------------------
    class BundleNotFoundException : public ShaderBundleException
    {
    public:
        using ShaderBundleException::ShaderBundleException;
    };

    //-------------------------------------------------------------------------------------------
    // InvalidBundleJsonException
    // Thrown when bundle.json or fallback_rule.json has parsing errors
    //
    // Typical scenarios:
    //   - Malformed JSON syntax
    //   - Missing required fields (name, etc.)
    //   - Invalid field types
    //
    // Mapping: ERROR_RECOVERABLE (may continue with default values)
    //          or ERROR_AND_DIE (if critical fields missing)
    //-------------------------------------------------------------------------------------------
    class InvalidBundleJsonException : public ShaderBundleException
    {
    public:
        using ShaderBundleException::ShaderBundleException;
    };

    //-------------------------------------------------------------------------------------------
    // CompilationFailedException
    // Thrown when shader compilation fails
    //
    // Typical scenarios:
    //   - HLSL syntax errors
    //   - Missing shader entry point
    //   - Incompatible shader model
    //   - DXC compiler errors
    //
    // Mapping: ERROR_AND_DIE (cannot use shader without successful compilation)
    //-------------------------------------------------------------------------------------------
    class CompilationFailedException : public ShaderBundleException
    {
    public:
        using ShaderBundleException::ShaderBundleException;
    };
} // namespace enigma::graphic
