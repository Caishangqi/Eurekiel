#pragma once
//-----------------------------------------------------------------------------------------------
// LightException.hpp
//
// [NEW] Centralized exception hierarchy for VoxelLight module
//
// Summary of exception types and their mapping to error macros:
//
// | Exception Type              | Error Macro          | Description                           |
// |-----------------------------|----------------------|---------------------------------------|
// | LightEngineException        | ERROR_AND_DIE        | Base class, generic light error       |
// | InvalidBlockIteratorException| ERROR_RECOVERABLE   | Invalid iterator, may skip processing |
// | ChunkNotLoadedException     | ERROR_RECOVERABLE    | Chunk not loaded, defer processing    |
// | LightPropagationException   | ERROR_AND_DIE        | Critical propagation failure          |
//
// Usage Pattern (Exception + Error Macro two-phase):
//   1. Throw Phase: Throw specific exception type from business code
//   2. Catch Phase: In catch block, apply corresponding error macro
//
//-----------------------------------------------------------------------------------------------

#include <exception>
#include <string>

namespace enigma::voxel
{
    //-------------------------------------------------------------------------------------------
    // LightEngineException
    // Base exception class for all VoxelLight module errors
    //
    // Default mapping: ERROR_AND_DIE (fatal error)
    //-------------------------------------------------------------------------------------------
    class LightEngineException : public std::exception
    {
    public:
        explicit LightEngineException(const std::string& message);

        const char* what() const noexcept override;

    private:
        std::string m_message;
    };

    //-------------------------------------------------------------------------------------------
    // InvalidBlockIteratorException
    // Thrown when a BlockIterator is invalid or points to unloaded chunk
    //
    // Mapping: ERROR_RECOVERABLE (skip processing, continue with next block)
    //-------------------------------------------------------------------------------------------
    class InvalidBlockIteratorException : public LightEngineException
    {
    public:
        using LightEngineException::LightEngineException;
    };

    //-------------------------------------------------------------------------------------------
    // ChunkNotLoadedException
    // Thrown when attempting to access light data from an unloaded chunk
    //
    // Mapping: ERROR_RECOVERABLE (defer processing until chunk loads)
    //-------------------------------------------------------------------------------------------
    class ChunkNotLoadedException : public LightEngineException
    {
    public:
        using LightEngineException::LightEngineException;
    };

    //-------------------------------------------------------------------------------------------
    // LightPropagationException
    // Thrown when light propagation algorithm encounters critical failure
    //
    // Mapping: ERROR_AND_DIE (cannot continue rendering)
    //-------------------------------------------------------------------------------------------
    class LightPropagationException : public LightEngineException
    {
    public:
        using LightEngineException::LightEngineException;
    };
} // namespace enigma::voxel
