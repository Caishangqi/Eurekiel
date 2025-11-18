#pragma once

#include "PSOStateCollector.hpp"

namespace enigma::graphic
{
    /**
     * @class RenderStateValidator
     * @brief Render State Validator - Validates Draw() state completeness
     * 
     * Pure static helper class following shrimp-rules.md §10
     * - No state: All methods are static
     * - No instantiation: Constructor deleted
     * - Single responsibility: Validate PSO state only
     */
    class RenderStateValidator
    {
    public:
        /**
         * @brief Validate Draw() state completeness
         * @param state Collected PSO state
         * @param outErrorMessage Optional error message output
         * @return true if state is valid, false otherwise
         */
        static bool ValidateDrawState(
            const PSOStateCollector::CollectedState& state,
            const char** outErrorMessage = nullptr
        );

    private:
        RenderStateValidator() = delete;  // [REQUIRED] Prevent instantiation
    };
}
