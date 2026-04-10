#pragma once
#include "Engine/Graphic/Resource/CommandQueueTypes.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace enigma::graphic
{
    // Per-frame execution state.
    // Synchronization policy stays in D3D12RenderSystem.
    struct FrameContext
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> graphicsCommandAllocator;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> computeCommandAllocator;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> copyCommandAllocator;
        FrameQueueRetirementState                      retirement = {};

        ID3D12CommandAllocator* GetCommandAllocator(CommandQueueType queueType) const noexcept
        {
            switch (queueType)
            {
            case CommandQueueType::Graphics:
                return graphicsCommandAllocator.Get();
            case CommandQueueType::Compute:
                return computeCommandAllocator.Get();
            case CommandQueueType::Copy:
                return copyCommandAllocator.Get();
            }

            return nullptr;
        }

        void ClearRetirement() noexcept
        {
            retirement.Clear();
        }
    };
} // namespace enigma::graphic
