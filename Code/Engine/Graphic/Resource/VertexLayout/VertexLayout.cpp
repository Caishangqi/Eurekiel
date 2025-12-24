#include "VertexLayout.hpp"
#include "VertexLayoutCommon.hpp"

#include <functional>

#include "Engine/Core/Logger/LoggerAPI.hpp"

// ============================================================================
// VertexLayout.cpp - Implementation of VertexLayout base class
// ============================================================================

namespace enigma::graphic
{
    // ========================================================================
    // Constructor
    // ========================================================================

    VertexLayout::VertexLayout(const char* name, size_t stride)
        : m_name(name)
          , m_stride(stride)
          , m_hash(0)
    {
    }

    // ========================================================================
    // Hash Calculation
    // ========================================================================

    void VertexLayout::CalculateHash(const D3D12_INPUT_ELEMENT_DESC* elements, uint32_t count)
    {
        // Hash algorithm: combine semantic name, index, format, and offset
        // Uses XOR shift combining for good distribution

        size_t                 hash = 0;
        std::hash<std::string> stringHasher;
        std::hash<uint32_t>    intHasher;

        for (uint32_t i = 0; i < count; ++i)
        {
            const auto& elem = elements[i];

            // Hash semantic name (string hash)
            size_t nameHash = stringHasher(elem.SemanticName);

            // Hash semantic index
            size_t indexHash = intHasher(elem.SemanticIndex);

            // Hash format (DXGI_FORMAT enum)
            size_t formatHash = intHasher(static_cast<uint32_t>(elem.Format));

            // Hash offset
            size_t offsetHash = intHasher(elem.AlignedByteOffset);

            // Combine using XOR shift pattern
            // Each element contributes to the overall hash
            hash ^= nameHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= indexHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= formatHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= offsetHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }

        // Include stride in hash for additional uniqueness
        hash ^= intHasher(static_cast<uint32_t>(m_stride)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

        m_hash = hash;

        LogInfo(LogVertexLayout, "VertexLayout:: '%s' hash calculated: 0x%zx (stride=%zu, elements=%u)",
                m_name.c_str(), m_hash, m_stride, count);
    }
} // namespace enigma::graphic
