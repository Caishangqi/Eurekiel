//-----------------------------------------------------------------------------------------------
// ShaderBundleCommon.cpp
//
// [NEW] Implementation file for ShaderBundle common types and declarations
//
// This file contains:
//   - Log category definition for LogShaderBundle
//
// Teaching Points:
//   - DEFINE_LOG_CATEGORY must be in exactly one .cpp file
//   - The macro generates a unique ID using __COUNTER__
//   - Log category can be used across all translation units after extern declaration
//
//-----------------------------------------------------------------------------------------------

#include "ShaderBundleCommon.hpp"

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // Log Category Definition
    //
    // [NEW] Define the LogShaderBundle log category
    // This creates the actual storage for the category declared in the header
    // Default log level is TRACE (most verbose)
    //-------------------------------------------------------------------------------------------
    DEFINE_LOG_CATEGORY(LogShaderBundle);
} // namespace enigma::graphic
