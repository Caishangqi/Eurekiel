#pragma once

#include <cstdint>
#include <memory>
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"

namespace enigma::graphic
{
    enum class UpdateFrequency : uint8_t;

    static constexpr uint32_t MAX_PASS_SCOPES_PER_FRAME = 20;

    // ============================================================================
    // BufferUpdateStrategy - Strategy pattern for uniform buffer ring indexing
    //
    // Encapsulates frequency-specific ring buffer behavior:
    // - How many ring buffer slots to allocate
    // - Current write/read index management
    // - Post-upload advancement (PerDispatch auto-advances)
    // - Draw count and frame reset handling
    //
    // Eliminates all frequency switch/case blocks in UniformManager.
    // Adding new frequencies requires only a new strategy class + factory entry.
    // ============================================================================

    class BufferUpdateStrategy
    {
    public:
        virtual ~BufferUpdateStrategy() = default;

        // How many ring buffer slots to allocate (called during RegisterBuffer)
        virtual size_t GetRingBufferCount(size_t maxDraws) const = 0;

        // Current index for writing data (used by UploadBufferInternal)
        virtual size_t GetCurrentWriteIndex() const = 0;

        // Current index for GPU address binding (used by GetEngineBufferGPUAddress)
        // For most strategies: same as write index
        // For PerDispatch: returns last-written index (write has already advanced)
        virtual size_t GetCurrentReadIndex() const = 0;

        // Called after each UploadBuffer() completes
        virtual void OnPostUpload() = 0;

        // Called when IncrementDrawCount() is invoked globally
        virtual void OnDrawCountIncrement() = 0;

        // Called when an explicit render pass scope advances.
        // Default no-op for strategies that are not pass-scoped.
        virtual void OnPassScopeAdvance() {}

        // Called at frame start (ResetDrawCount)
        virtual void OnFrameReset() = 0;

        // Called at BeginFrame to update frame partition index.
        // Queries D3D12RenderSystem::GetFrameIndex() internally.
        // Default no-op for strategies that don't need frame awareness.
        virtual void SetFrameIndex() {}

        // Factory: create strategy from UpdateFrequency enum
        static std::unique_ptr<BufferUpdateStrategy> Create(UpdateFrequency freq, size_t maxCount = 0);
    };

    // ============================================================================
    // Concrete Strategies
    // ============================================================================

    // --- StaticUpdateStrategy ---
    // Single slot, never changes. Used for engine constants that are set once.
    class StaticUpdateStrategy final : public BufferUpdateStrategy
    {
    public:
        size_t GetRingBufferCount(size_t) const override { return 1; }
        size_t GetCurrentWriteIndex() const override     { return 0; }
        size_t GetCurrentReadIndex() const override      { return 0; }
        void OnPostUpload() override                     {}
        void OnDrawCountIncrement() override             {}
        void OnFrameReset() override                     {}
    };

    // --- PerFrameUpdateStrategy ---
    // N slots (one per in-flight frame). Camera, time, viewport uniforms.
    // Write index driven by frameIndex, no longer reset each frame.
    class PerFrameUpdateStrategy final : public BufferUpdateStrategy
    {
    public:
        size_t GetRingBufferCount(size_t) const override { return MAX_FRAMES_IN_FLIGHT; }
        size_t GetCurrentWriteIndex() const override     { return m_frameIndex; }
        size_t GetCurrentReadIndex() const override      { return m_frameIndex; }
        void OnPostUpload() override                     {}
        void OnDrawCountIncrement() override             {}
        void OnFrameReset() override                     {}

        void SetFrameIndex() override;

    private:
        uint32_t m_frameIndex = 0;
    };

    // --- PerPassUpdateStrategy ---
    // Frame-partitioned slots for explicit render pass scope rotation.
    class PerPassUpdateStrategy final : public BufferUpdateStrategy
    {
    public:
        size_t GetRingBufferCount(size_t) const override { return static_cast<size_t>(MAX_PASS_SCOPES_PER_FRAME) * MAX_FRAMES_IN_FLIGHT; }
        size_t GetCurrentWriteIndex() const override;
        size_t GetCurrentReadIndex() const override      { return GetCurrentWriteIndex(); }
        void OnPostUpload() override                     {}
        void OnDrawCountIncrement() override             {}
        void OnPassScopeAdvance() override;
        void OnFrameReset() override                     { m_passScopeIndex = 0; }

        void SetFrameIndex() override;

    private:
        size_t GetCurrentIndex() const;

    private:
        size_t m_frameBaseOffset = 0;
        size_t m_passScopeIndex  = 0;
    };

    // --- PerObjectUpdateStrategy ---
    // Ring buffer advanced by global IncrementDrawCount(). Per-draw-call data.
    // With multi-frame: N * maxDraws slots, frame base offset applied.
    class PerObjectUpdateStrategy final : public BufferUpdateStrategy
    {
    public:
        explicit PerObjectUpdateStrategy(size_t maxCount) : m_maxCount(maxCount) {}

        size_t GetRingBufferCount(size_t maxDraws) const override
        {
            return maxDraws * MAX_FRAMES_IN_FLIGHT;
        }

        size_t GetCurrentWriteIndex() const override
        {
            size_t totalCount = m_maxCount * MAX_FRAMES_IN_FLIGHT;
            return (totalCount > 0) ? ((m_frameBaseOffset + m_ringIndex) % totalCount) : 0;
        }

        size_t GetCurrentReadIndex() const override { return GetCurrentWriteIndex(); }
        void OnPostUpload() override                {}
        void OnDrawCountIncrement() override        { m_ringIndex++; }
        void OnFrameReset() override                { m_ringIndex = 0; }

        void SetFrameIndex() override;

    private:
        size_t m_ringIndex       = 0;
        size_t m_maxCount;
        size_t m_frameBaseOffset = 0;
    };

    // --- PerDispatchUpdateStrategy ---
    // Self-contained ring buffer for compute dispatch loops.
    // Auto-advances on each UploadBuffer(), no global side effects.
    class PerDispatchUpdateStrategy final : public BufferUpdateStrategy
    {
    public:
        explicit PerDispatchUpdateStrategy(size_t maxCount) : m_maxCount(maxCount) {}

        size_t GetRingBufferCount(size_t) const override { return m_maxCount; }

        size_t GetCurrentWriteIndex() const override
        {
            return (m_maxCount > 0) ? (m_dispatchIndex % m_maxCount) : 0;
        }

        size_t GetCurrentReadIndex() const override
        {
            // Return last-written slot (UploadBuffer already advanced m_dispatchIndex)
            return (m_dispatchIndex > 0) ? ((m_dispatchIndex - 1) % m_maxCount) : 0;
        }

        void OnPostUpload() override         { m_dispatchIndex++; }
        void OnDrawCountIncrement() override  {} // no-op: self-contained
        void OnFrameReset() override          { m_dispatchIndex = 0; }

    private:
        size_t m_dispatchIndex = 0;
        size_t m_maxCount;
    };
} // namespace enigma::graphic
