#pragma once

// ============================================================================
// VertexLayout.hpp - Abstract base class for vertex layout definitions
// 
// Design Notes:
// - All concrete Layout classes inherit from this base class
// - Instances are created and managed by VertexLayoutRegistry
// - InputElements array is defined by subclasses
// - Hash is calculated at construction for PSO cache key comparison
// ============================================================================

#include <d3d12.h>
#include <string>
#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief Abstract base class for vertex layouts
     * 
     * Core responsibilities:
     * 1. Define vertex input element descriptions for D3D12 InputLayout
     * 2. Provide stride and hash for PSO cache key building
     * 3. Enable comparison operators for layout identity checking
     * 
     * Subclass Implementation:
     * - Override GetInputElements() to return static element array
     * - Override GetInputElementCount() to return element count
     * - Call CalculateHash() in constructor after setting up elements
     * 
     * Example usage:
     *   class MyVertexLayout : public VertexLayout
     *   {
     *   public:
     *       MyVertexLayout() : VertexLayout("MyVertex", sizeof(MyVertex))
     *       {
     *           CalculateHash(s_elements, 3);
     *       }
     *       const D3D12_INPUT_ELEMENT_DESC* GetInputElements() const override
     *       { return s_elements; }
     *       uint32_t GetInputElementCount() const override { return 3; }
     *   private:
     *       static const D3D12_INPUT_ELEMENT_DESC s_elements[3];
     *   };
     */
    class VertexLayout
    {
    public:
        virtual ~VertexLayout() = default;

        // ========================================================================
        // Core Interface - Pure Virtual (subclass must implement)
        // ========================================================================

        /**
         * @brief Get D3D12 input element description array
         * @return Pointer to static element array
         */
        [[nodiscard]] virtual const D3D12_INPUT_ELEMENT_DESC* GetInputElements() const = 0;

        /**
         * @brief Get number of input elements
         * @return Element count
         */
        [[nodiscard]] virtual uint32_t GetInputElementCount() const = 0;

        // ========================================================================
        // Core Interface - Concrete (implemented in base class)
        // ========================================================================

        /**
         * @brief Get vertex stride in bytes
         * @return Stride value
         */
        [[nodiscard]] size_t GetStride() const { return m_stride; }

        /**
         * @brief Get layout hash for PSO cache key comparison
         * @return Hash value
         * 
         * Hash is calculated from semantic names, formats, and offsets.
         * Two layouts with identical elements will have the same hash.
         */
        [[nodiscard]] size_t GetLayoutHash() const { return m_hash; }

        /**
         * @brief Get layout name for debugging
         * @return Layout name string
         */
        [[nodiscard]] const char* GetLayoutName() const { return m_name.c_str(); }

        // ========================================================================
        // Comparison Operators - Based on hash
        // ========================================================================

        bool operator==(const VertexLayout& other) const { return m_hash == other.m_hash; }
        bool operator!=(const VertexLayout& other) const { return m_hash != other.m_hash; }

    protected:
        /**
         * @brief Protected constructor - only subclasses can instantiate
         * @param name Layout name for debugging
         * @param stride Vertex stride in bytes
         */
        VertexLayout(const char* name, size_t stride);

        /**
         * @brief Calculate hash from input elements
         * @param elements Pointer to element array
         * @param count Number of elements
         * 
         * Call this in subclass constructor after elements are defined.
         * Hash combines: semantic name, semantic index, format, offset
         */
        void CalculateHash(const D3D12_INPUT_ELEMENT_DESC* elements, uint32_t count);

    protected:
        std::string m_name; // Layout name for debugging
        size_t      m_stride; // Vertex stride in bytes
        size_t      m_hash; // Computed hash for comparison
    };
} // namespace enigma::graphic
