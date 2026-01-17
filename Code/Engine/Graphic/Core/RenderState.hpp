/**
 * @file RenderState.hpp
 * @brief Rendering state aggregation header file - contains all rendering state configurations
 *
 * Teaching focus:
 * 1. Modular design - split large files into independent functional modules
 * 2. Aggregated header file mode - provides a unified inclusion entry
 * 3. Backward compatibility - existing code does not need to modify the include path
 *
 * File structure:
 * - RenderState/BlendState.hpp - Blend State (BlendMode)
 * - RenderState/DepthState.hpp - Depth state (DepthMode, DepthConfig)
 * - RenderState/RasterizeState.hpp - Rasterization state (RasterizationConfig)
 * - RenderState/StencilState.hpp - Template state (StencilTestDetail)
 */

#pragma once

// ========================================
// Modular Render State Includes
// ========================================

#include "RenderState/BlendState.hpp"
#include "RenderState/DepthState.hpp"
#include "RenderState/RasterizeState.hpp"
#include "RenderState/StencilState.hpp"
