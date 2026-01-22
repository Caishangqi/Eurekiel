#pragma once
#include "FluidType.hpp"

namespace enigma::voxel
{
    /**
     * @brief Simplified FluidState for liquid detection
     *
     * [MINECRAFT REF] FluidState.java
     * File: net/minecraft/world/level/material/FluidState.java
     *
     * [DEVIATION] Simplified version - does not inherit from StateHolder.
     * Current implementation only stores FluidType for basic liquid detection.
     *
     * ============================================================
     * MINECRAFT FLUID ARCHITECTURE (For Future Reference)
     * ============================================================
     *
     * Minecraft's full Fluid system:
     * ```
     * Fluid (similar to Block)
     * +-- EmptyFluid (air)
     * +-- FlowingFluid (flowing fluid base class)
     *     +-- WaterFluid.Source / WaterFluid.Flowing
     *     +-- LavaFluid.Source / LavaFluid.Flowing
     *
     * FluidState extends StateHolder<Fluid, FluidState>
     * - owner = Fluid (NOT Block!)
     * - has LEVEL property (0-8)
     * - has flow direction, height calculation, etc.
     * ```
     *
     * ============================================================
     * REFACTORING IMPACT ANALYSIS
     * ============================================================
     *
     * If full Fluid system implementation is needed in the future:
     *
     * | Module          | Current Design           | Minecraft Design                    | Effort |
     * |-----------------|--------------------------|-------------------------------------|--------|
     * | Chunk Storage   | BlockState only          | BlockState + FluidState separately  | HIGH   |
     * | World Generator | Place water Block        | Place Block + set FluidState        | MEDIUM |
     * | Light Engine    | Query via BlockState     | Must consider FluidState too        | MEDIUM |
     * | Render System   | LiquidBlock rendering    | Dedicated FluidRenderer             | MEDIUM |
     * | Tick System     | Block tick only          | Block tick + Fluid tick             | HIGH   |
     *
     * ============================================================
     * CURRENT SIMPLIFIED DESIGN (Recommended to Keep)
     * ============================================================
     *
     * Advantages of current design:
     * 1. LiquidBlock already has LEVEL property (0-15) in BlockState
     * 2. FluidState is sufficient for type detection (face culling, underwater check)
     * 3. No Chunk refactoring required
     * 4. No generator refactoring required
     *
     * Current flow:
     * ```
     * BlockState (water, level=0)
     *   +-- GetFluidState() -> FluidState(WATER)  // Cached type info
     * ```
     *
     * ============================================================
     * CORE FUNCTIONALITY
     * ============================================================
     *
     * - IsEmpty() - Check if empty (replaces deprecated liquid())
     * - GetType() - Get fluid type (EMPTY, WATER, LAVA)
     * - IsWater() / IsLava() - Type checking helpers
     *
     * Usage:
     * ```cpp
     * // [OLD - Deprecated in Minecraft]
     * if (blockState->IsLiquid()) { ... }
     *
     * // [NEW - Recommended]
     * if (!blockState->GetFluidState().IsEmpty()) { ... }
     * ```
     *
     * [YAML SUPPORT]
     * Fluid type is determined by block's base_class in YAML:
     * ```yaml
     * base_class: LiquidBlock
     * fluid_type: water  # or "lava"
     * ```
     *
     * [TODO - Future Phases]
     * - Phase 2: Fluid height support (GetHeight, GetOwnHeight, GetAmount)
     * - Phase 3: Flow direction support (GetFlow)
     * - Phase 4: Full StateHolder inheritance with Fluid owner class
     */
    class FluidState
    {
    private:
        enigma::voxel::FluidType m_type = enigma::voxel::FluidType::EMPTY;

    public:
        // ============================================================
        // Constructors
        // ============================================================

        FluidState() = default;

        explicit FluidState(enigma::voxel::FluidType type)
            : m_type(type)
        {
        }

        // ============================================================
        // Core Methods
        // [MINECRAFT REF] FluidState.java core API
        // ============================================================

        /**
         * @brief Check if this is an empty fluid state (no fluid)
         *
         * [MINECRAFT REF] FluidState.isEmpty()
         * This is the recommended way to check if a block contains fluid,
         * replacing the deprecated liquid() method.
         *
         * @return true if no fluid (air, solid blocks), false if water/lava
         */
        bool IsEmpty() const { return m_type == enigma::voxel::FluidType::EMPTY; }

        /**
         * @brief Get the fluid type
         *
         * [MINECRAFT REF] FluidState.getType()
         *
         * @return FluidType (EMPTY, WATER, or LAVA)
         */
        enigma::voxel::FluidType GetType() const { return m_type; }

        // ============================================================
        // Type Checking Helpers
        // ============================================================

        /**
         * @brief Check if this is water
         */
        bool IsWater() const { return m_type == enigma::voxel::FluidType::WATER; }

        /**
         * @brief Check if this is lava
         */
        bool IsLava() const { return m_type == enigma::voxel::FluidType::LAVA; }

        /**
         * @brief Check if fluid type matches another
         *
         * [MINECRAFT REF] Fluid.isSame()
         * Used by LiquidBlock::SkipRendering() for same-type face culling
         */
        bool IsSame(enigma::voxel::FluidType other) const { return m_type == other; }

        bool IsSame(const FluidState& other) const { return m_type == other.m_type; }

        // ============================================================
        // Static Factory Methods
        // ============================================================

        /**
         * @brief Create an empty fluid state
         *
         * [MINECRAFT REF] Fluids.EMPTY.defaultFluidState()
         */
        static FluidState Empty() { return FluidState(enigma::voxel::FluidType::EMPTY); }

        /**
         * @brief Create a water fluid state
         *
         * [MINECRAFT REF] Fluids.WATER.defaultFluidState()
         */
        static FluidState Water() { return FluidState(enigma::voxel::FluidType::WATER); }

        /**
         * @brief Create a lava fluid state
         *
         * [MINECRAFT REF] Fluids.LAVA.defaultFluidState()
         */
        static FluidState Lava() { return FluidState(enigma::voxel::FluidType::LAVA); }

        // ============================================================
        // Future Extensions (Not Implemented)
        // [MINECRAFT REF] Full FluidState API for reference
        // ============================================================

        // TODO: Phase 2 - Fluid height support
        // float GetHeight(World* world, const BlockPos& pos) const;
        // float GetOwnHeight() const;
        // int GetAmount() const;  // 0-8, where 0 = source

        // TODO: Phase 3 - Flow direction support
        // Vec3 GetFlow(World* world, const BlockPos& pos) const;

        // TODO: Phase 4 - Advanced rendering
        // bool ShouldRenderBackwardUpFace(World* world, const BlockPos& pos) const;
    };
}
