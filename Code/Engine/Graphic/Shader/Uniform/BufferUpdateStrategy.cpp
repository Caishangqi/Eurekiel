#include "BufferUpdateStrategy.hpp"
#include "UniformCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"

namespace enigma::graphic
{
    void PerPassUpdateStrategy::SetFrameIndex()
    {
        const uint32_t frameIndex = D3D12RenderSystem::GetFrameIndex();
        if (frameIndex >= MAX_FRAMES_IN_FLIGHT)
        {
            ERROR_AND_DIE(Stringf("PerPassUpdateStrategy: invalid frame index %u (max=%u)",
                frameIndex, MAX_FRAMES_IN_FLIGHT));
        }

        m_frameBaseOffset = static_cast<size_t>(frameIndex) * MAX_PASS_SCOPES_PER_FRAME;
    }

    size_t PerPassUpdateStrategy::GetCurrentWriteIndex() const
    {
        return GetCurrentIndex();
    }

    void PerPassUpdateStrategy::OnPassScopeAdvance()
    {
        const size_t nextPassScopeIndex = m_passScopeIndex + 1;
        if (nextPassScopeIndex >= MAX_PASS_SCOPES_PER_FRAME)
        {
            ERROR_AND_DIE(Stringf("PerPass ring overflow: frame=%u scope=%zu capacity=%u",
                D3D12RenderSystem::GetFrameIndex(), nextPassScopeIndex, MAX_PASS_SCOPES_PER_FRAME));
        }

        m_passScopeIndex = nextPassScopeIndex;
    }

    size_t PerPassUpdateStrategy::GetCurrentIndex() const
    {
        const size_t currentIndex = m_frameBaseOffset + m_passScopeIndex;
        const size_t ringBufferCount = GetRingBufferCount(0);
        if (currentIndex >= ringBufferCount)
        {
            ERROR_AND_DIE(Stringf("PerPass index out of range: frame=%u base=%zu scope=%zu count=%zu",
                D3D12RenderSystem::GetFrameIndex(), m_frameBaseOffset, m_passScopeIndex, ringBufferCount));
        }

        return currentIndex;
    }

    void PerFrameUpdateStrategy::SetFrameIndex()
    {
        m_frameIndex = D3D12RenderSystem::GetFrameIndex();
    }

    void PerObjectUpdateStrategy::SetFrameIndex()
    {
        m_frameBaseOffset = D3D12RenderSystem::GetFrameIndex() * m_maxCount;
    }

    std::unique_ptr<BufferUpdateStrategy> BufferUpdateStrategy::Create(UpdateFrequency freq, size_t maxCount)
    {
        switch (freq)
        {
        case UpdateFrequency::Static:
            return std::make_unique<StaticUpdateStrategy>();
        case UpdateFrequency::PerFrame:
            return std::make_unique<PerFrameUpdateStrategy>();
        case UpdateFrequency::PerPass:
            return std::make_unique<PerPassUpdateStrategy>();
        case UpdateFrequency::PerObject:
            return std::make_unique<PerObjectUpdateStrategy>(maxCount);
        case UpdateFrequency::PerDispatch:
            return std::make_unique<PerDispatchUpdateStrategy>(maxCount > 0 ? maxCount : 16);
        default:
            ERROR_AND_DIE("BufferUpdateStrategy::Create: Unknown UpdateFrequency");
        }
    }
} // namespace enigma::graphic
