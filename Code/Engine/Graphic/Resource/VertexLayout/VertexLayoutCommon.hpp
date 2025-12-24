#pragma once

// ============================================================================
// VertexLayoutCommon.hpp - Common type definitions for VertexLayout system
// 
// This module provides common declarations including LogCategory for the
// entire VertexLayout system.
// ============================================================================

#include "Engine/Core/LogCategory/LogCategory.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // Log Category Declaration
    // ========================================================================

    /**
     * @brief Log category for VertexLayout system
     * 
     * Usage:
     *   LogInfo(LogVertexLayout, "Layout registered: %s", layoutName);
     *   LogWarn(LogVertexLayout, "Stride mismatch detected");
     *   LogError(LogVertexLayout, "Layout not found: %s", name);
     */
    DECLARE_LOG_CATEGORY_EXTERN(LogVertexLayout);
} // namespace enigma::graphic
