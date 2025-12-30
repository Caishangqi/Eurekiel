#pragma once
#include "Engine/Core/LogCategory/LogCategory.hpp"
//-----------------------------------------------------------------------------------------------
// LightEngineCommon.hpp
//
// [NEW] Common declarations for VoxelLight module
// Contains log category declarations for consistent logging across light engine components
//
// Usage:
//   LogInfo(LogVoxelLight, "VoxelLightEngine:: Message with format: %s", arg);
//   LogDebug(LogTimeProvider, "WorldTimeProvider:: Tick updated to %d", tick);
//
//-----------------------------------------------------------------------------------------------

namespace enigma::voxel
{
    //-------------------------------------------------------------------------------------------
    // Log Category Declarations
    //
    // [NEW] VoxelLight module log categories for consistent logging
    // Definition in LightEngineCommon.cpp
    //-------------------------------------------------------------------------------------------
    DECLARE_LOG_CATEGORY_EXTERN(LogVoxelLight);
} // namespace enigma::voxel
