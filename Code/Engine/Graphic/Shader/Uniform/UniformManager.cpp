// Windows header preprocessing
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "UniformManager.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

#include <cstring>

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Resource/BindlessRootSignature.hpp"
#include "Engine/Graphic/Resource/GlobalDescriptorHeapManager.hpp"
#include "Engine/Graphic/Integration/RendererEvents.hpp"

namespace enigma::graphic
{
    UniformManager::UniformManager()
    {
        // Step 1: Get GlobalDescriptorHeapManager
        auto* heapMgr = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
        if (!heapMgr)
        {
            LogError(LogUniform, "GlobalDescriptorHeapManager not available");
            ERROR_AND_DIE("UniformManager: GlobalDescriptorHeapManager not available");
        }

        // Step 2: Get Descriptor increment size
        auto* device = D3D12RenderSystem::GetDevice();
        if (device)
        {
            m_cbvSrvUavDescriptorIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        // Step 3: Pre-allocate Frame-Isolated Ring Descriptor Pool
        // Each in-flight frame gets its own descriptor region to prevent CPU/GPU race conditions.
        // Layout: [Frame0: Draw0..Draw1023] [Frame1: Draw0..Draw1023], each draw has MAX_CUSTOM_BUFFERS slots
        const uint32_t totalDescriptors = MAX_FRAMES_IN_FLIGHT * BindlessRootSignature::MAX_RING_FRAMES * BindlessRootSignature::MAX_CUSTOM_BUFFERS;
        auto           allocations      = heapMgr->BatchAllocateCustomCbv(totalDescriptors);
        if (allocations.size() != totalDescriptors)
        {
            LogError(LogUniform, "Failed to allocate Ring Descriptor Pool: expected %u, got %zu",
                     totalDescriptors, allocations.size());
            ERROR_AND_DIE(Stringf("UniformManager: expected %u descriptors, got %zu",
                totalDescriptors, allocations.size()));
        }

        // Step 4: Verify descriptor continuity (required by Descriptor Table)
        for (size_t i = 1; i < allocations.size(); i++)
        {
            if (allocations[i].heapIndex != allocations[i - 1].heapIndex + 1)
            {
                LogError(LogUniform, "Ring Descriptor Pool not contiguous at index %zu", i);
                ERROR_AND_DIE(Stringf("UniformManager: descriptors not contiguous at index %zu", i));
            }
        }

        // Step 5: Store base GPU handle for SetGraphicsRootDescriptorTable
        m_customBufferDescriptorTableBaseGPUHandle = allocations[0].gpuHandle;

        LogInfo(LogUniform, "UniformManager: Ring Descriptor Pool Base GPU Handle=0x%llX, incrementSize=%u",
                m_customBufferDescriptorTableBaseGPUHandle.ptr, m_cbvSrvUavDescriptorIncrementSize);

        // Step 6: Store all allocations
        m_customBufferDescriptorPool.resize(totalDescriptors);
        for (size_t i = 0; i < allocations.size(); i++)
        {
            m_customBufferDescriptorPool[i] = allocations[i];
        }

        m_initialized = true;

        // Subscribe to OnBeginFrame to auto-update frame index on all strategies
        m_beginFrameHandle = RendererEvents::OnBeginFrame.Add(this, &UniformManager::SetFrameIndex);

        LogInfo(LogUniform, "UniformManager: %u Ring Descriptors allocated (Frames=%u * Draws=%u * Slots=%u)",
                totalDescriptors, MAX_FRAMES_IN_FLIGHT, BindlessRootSignature::MAX_RING_FRAMES, BindlessRootSignature::MAX_CUSTOM_BUFFERS);
    }

    UniformManager::~UniformManager()
    {
        // Unsubscribe from OnBeginFrame delegate
        if (m_beginFrameHandle != 0)
        {
            RendererEvents::OnBeginFrame.Remove(m_beginFrameHandle);
            m_beginFrameHandle = 0;
        }
    }

    void UniformManager::SetFrameIndex()
    {
        const uint32_t currentFrameIndex = D3D12RenderSystem::GetFrameIndex();

        for (auto& [typeId, state] : m_bufferStates)
        {
            if (state.strategy)
            {
                state.strategy->SetFrameIndex();
            }
        }

        ResetCustomDescriptorPageState(currentFrameIndex);
    }

    void UniformManager::IncrementDrawCount()
    {
        m_currentDrawCount++;
        // Strategy pattern: delegate to each buffer's strategy
        for (auto& [typeId, state] : m_bufferStates)
        {
            if (state.strategy)
            {
                state.strategy->OnDrawCountIncrement();
            }
        }
    }

    void UniformManager::ResetDrawCount()
    {
        m_currentDrawCount = 0;
        // Strategy pattern: delegate frame reset to each buffer's strategy
        for (auto& [typeId, state] : m_bufferStates)
        {
            if (state.strategy)
            {
                state.strategy->OnFrameReset();
            }
        }
    }

    void UniformManager::ResetPassScopesForFrame()
    {
        const uint32_t currentFrameIndex = D3D12RenderSystem::GetFrameIndex();

        m_passScopeState.frameIndex     = currentFrameIndex;
        m_passScopeState.passScopeIndex = 0;
        m_passScopeState.hasActiveScope = false;
        m_passScopeState.hasFrameReset  = true;

        for (auto& [typeId, state] : m_bufferStates)
        {
            if (state.frequency != UpdateFrequency::PerPass || !state.strategy)
            {
                continue;
            }

            state.strategy->OnFrameReset();
        }
    }

    void UniformManager::AdvancePassScope()
    {
        const uint32_t currentFrameIndex = D3D12RenderSystem::GetFrameIndex();
        if (!m_passScopeState.hasFrameReset || m_passScopeState.frameIndex != currentFrameIndex)
        {
            ERROR_RECOVERABLE(Stringf("UniformManager::AdvancePassScope called before ResetPassScopesForFrame for frame %u; resetting pass scope state",
                currentFrameIndex));
            ResetPassScopesForFrame();
        }

        if (!m_passScopeState.hasActiveScope)
        {
            m_passScopeState.hasActiveScope = true;
            return;
        }

        for (auto& [typeId, state] : m_bufferStates)
        {
            if (state.frequency != UpdateFrequency::PerPass || !state.strategy)
            {
                continue;
            }

            state.strategy->OnPassScopeAdvance();
        }

        ++m_passScopeState.passScopeIndex;
    }

    bool UniformManager::IsSlotRegistered(uint32_t slot, uint32_t space) const
    {
        return m_slotToTypeMap.find(SlotSpaceKey{slot, space}) != m_slotToTypeMap.end();
    }

    const std::vector<uint32_t>& UniformManager::GetSlotsByFrequency(UpdateFrequency frequency) const
    {
        static const std::vector<uint32_t> emptyVector;

        auto it = m_frequencyToSlotsMap.find(frequency);
        if (it != m_frequencyToSlotsMap.end())
        {
            return it->second;
        }

        return emptyVector;
    }

    PerObjectBufferState* UniformManager::GetBufferStateBySlot(uint32_t rootSlot, uint32_t space) const
    {
        // Find (slot, space) combination
        auto slotIt = m_slotToTypeMap.find(SlotSpaceKey{rootSlot, space});
        if (slotIt == m_slotToTypeMap.end())
        {
            return nullptr;
        }

        // [REFACTORED] Get TypeId and find in unified m_bufferStates
        const std::type_index& typeId   = slotIt->second;
        auto                   bufferIt = m_bufferStates.find(typeId);
        if (bufferIt != m_bufferStates.end())
        {
            return const_cast<PerObjectBufferState*>(&bufferIt->second);
        }

        // TypeId registered but buffer not created (abnormal)
        LogWarn(LogUniform, "Slot %u (space=%u) registered but buffer not found: %s", rootSlot, space, typeId.name());
        ERROR_RECOVERABLE(Stringf("Slot %u (space=%u) registered but buffer not found", rootSlot, space));
        return nullptr;
    }

    void UniformManager::UpdateRingBufferOffsets(UpdateFrequency frequency)
    {
        // Strategy pattern: iterate buffers, filter by frequency tag, delegate to strategy
        for (auto& [typeId, state] : m_bufferStates)
        {
            // Filter by frequency tag, skip if not matching or buffer invalid
            if (state.frequency != frequency || !state.buffer)
            {
                continue;
            }

            // Strategy provides the current write index
            size_t currentIndex = state.GetCurrentWriteIndex();

            // Delayed Fill: copy last value if index not updated
            if (state.lastUpdatedIndex != currentIndex)
            {
                void* destPtr = state.GetDataAt(currentIndex);
                std::memcpy(destPtr, state.lastUpdatedValue.data(), state.elementSize);
            }

            UpdateRootCBVOffset(state.slot, currentIndex);
        }
    }

    // ========================================================================
    // [REFACTORED] RegisterBufferInternal - Unified buffer registration
    // Routes to Engine (Root CBV) or Custom (Descriptor Table) based on space
    // ========================================================================
    bool UniformManager::RegisterBufferInternal(
        uint32_t        slotId,
        std::type_index typeId,
        size_t          bufferSize,
        UpdateFrequency freq,
        size_t          maxDraws,
        BufferSpace     space)
    {
        uint32_t spaceValue = static_cast<uint32_t>(space);

        // Validate space-specific constraints
        if (space == BufferSpace::Engine)
        {
            if (!BufferHelper::IsEngineReservedSlot(slotId))
            {
                throw UniformBufferException("Engine Buffer slot must be 0-14", slotId, spaceValue);
            }
        }
        else if (space == BufferSpace::Custom)
        {
            if (slotId >= BindlessRootSignature::MAX_CUSTOM_BUFFERS)
            {
                throw UniformBufferException("Custom Buffer slot exceeds MAX_CUSTOM_BUFFERS", slotId, spaceValue);
            }
            if (BufferHelper::IsEngineReservedSlot(slotId))
            {
                LogWarn(LogUniform, "Custom Buffer slot %u is in engine range (0-14), ensure shader uses register(b%u, space1)", slotId, slotId);
            }
        }

        // Check slot conflict
        if (IsSlotRegistered(slotId, spaceValue))
        {
            throw UniformBufferException("Slot already registered", slotId, spaceValue);
        }

        // Calculate 256-byte aligned size
        size_t alignedSize = BufferHelper::CalculateAlignedSize(bufferSize);

        // Strategy pattern: create strategy and query ring buffer count
        auto strategy = BufferUpdateStrategy::Create(freq, maxDraws);
        size_t ringBufferCount = strategy->GetRingBufferCount(maxDraws);

        size_t totalSize = alignedSize * ringBufferCount;

        // Create GPU Buffer using D12Buffer
        BufferCreateInfo createInfo;
        createInfo.size          = totalSize;
        createInfo.usage         = BufferUsage::ConstantBuffer;
        createInfo.memoryAccess  = MemoryAccess::CPUToGPU;
        createInfo.initialData   = nullptr;
        std::string debugNameStr = (space == BufferSpace::Engine)
                                       ? Stringf("EngineBuffer_Slot%u", slotId)
                                       : Stringf("CustomBuffer_Slot%u_Space1", slotId);
        createInfo.debugName = debugNameStr.c_str();

        auto gpuBuffer = D3D12RenderSystem::CreateBuffer(createInfo);
        if (!gpuBuffer)
        {
            throw UniformBufferException("Failed to create GPU buffer", slotId, spaceValue);
        }

        // Persistent mapping
        void* mappedData = gpuBuffer->MapPersistent();
        if (!mappedData)
        {
            throw UniformBufferException("Failed to map GPU buffer", slotId, spaceValue);
        }

        // Custom Buffer needs descriptor allocation and CBV creation
        if (space == BufferSpace::Custom)
        {
            if (!AllocateCustomBufferDescriptor(slotId))
            {
                throw DescriptorHeapException("Failed to allocate descriptor for Custom Buffer");
            }

            // Create CBV for ALL frames x ALL draws (frame-isolated Ring Descriptor Table)
            // Each in-flight frame gets its own descriptor region, preventing CPU/GPU race.
            // CBVs are deterministic and never need per-frame updates.
            auto* device = D3D12RenderSystem::GetDevice();
            if (!device)
            {
                throw DescriptorHeapException("D3D12 device not available for CBV creation");
            }

            D3D12_GPU_VIRTUAL_ADDRESS baseGpuAddress = gpuBuffer->GetGPUVirtualAddress();

            bool   isPerObject      = (freq == UpdateFrequency::PerObject);
            bool   isPerFrame       = (freq == UpdateFrequency::PerFrame);
            bool   isPerPass        = (freq == UpdateFrequency::PerPass);
            size_t perFrameDraws    = isPerObject ? (ringBufferCount / MAX_FRAMES_IN_FLIGHT) : 0;
            size_t perFramePasses   = isPerPass ? (ringBufferCount / MAX_FRAMES_IN_FLIGHT) : 0;

            for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
            {
                for (uint32_t draw = 0; draw < BindlessRootSignature::MAX_RING_FRAMES; ++draw)
                {
                    uint32_t globalRingPage  = frame * BindlessRootSignature::MAX_RING_FRAMES + draw;
                    uint32_t descriptorIndex = globalRingPage * BindlessRootSignature::MAX_CUSTOM_BUFFERS + slotId;

                    if (descriptorIndex >= m_customBufferDescriptorPool.size())
                    {
                        LogError(LogUniform, "Descriptor index %u out of range (pool size=%zu)",
                                 descriptorIndex, m_customBufferDescriptorPool.size());
                        continue;
                    }

                    size_t sliceIndex;
                    if (isPerObject)
                    {
                        // Frame F, Draw D -> buffer slot (F * perFrameDraws + D) % totalSlots
                        sliceIndex = (frame * perFrameDraws + draw) % ringBufferCount;
                    }
                    else if (isPerFrame)
                    {
                        // Frame F -> buffer slot F (same data for all draws within a frame)
                        sliceIndex = frame;
                    }
                    else if (isPerPass)
                    {
                        // PerPass pages are keyed by pass-scope index within the current frame partition.
                        const size_t passScopeIndex = (perFramePasses > 0 && draw < perFramePasses) ? draw : 0;
                        sliceIndex = frame * perFramePasses + passScopeIndex;
                    }
                    else
                    {
                        // Static/PerDispatch: always slot 0
                        sliceIndex = 0;
                    }

                    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle    = m_customBufferDescriptorPool[descriptorIndex].cpuHandle;
                    D3D12_GPU_VIRTUAL_ADDRESS   sliceAddress = baseGpuAddress + sliceIndex * alignedSize;

                    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                    cbvDesc.BufferLocation                  = sliceAddress;
                    cbvDesc.SizeInBytes                     = static_cast<UINT>(alignedSize);

                    device->CreateConstantBufferView(&cbvDesc, cpuHandle);
                }
            }

            LogInfo(LogUniform, "Created %u CBVs for Custom Buffer Slot=%u (Frames=%u * Draws=%u), Base GPU=0x%llX",
                    MAX_FRAMES_IN_FLIGHT * BindlessRootSignature::MAX_RING_FRAMES, slotId,
                    MAX_FRAMES_IN_FLIGHT, BindlessRootSignature::MAX_RING_FRAMES, baseGpuAddress);
        }

        // Create unified UniformBufferState with strategy
        UniformBufferState state;
        state.buffer      = std::move(gpuBuffer);
        state.strategy    = std::move(strategy);
        state.elementSize = alignedSize;
        state.maxCount    = ringBufferCount;
        state.frequency   = freq;
        state.slot        = slotId;
        state.space       = space;
        state.lastUpdatedValue.resize(alignedSize, 0);
        state.lastUpdatedIndex = SIZE_MAX;

        m_bufferStates[typeId] = std::move(state);
        m_frequencyToSlotsMap[freq].push_back(slotId);

        // [PERF] Populate engine buffer cache for O(1) lookup in GetEngineBufferGPUAddress
        if (space == BufferSpace::Engine && slotId < ENGINE_SLOT_COUNT)
        {
            m_engineBufferCache[slotId] = &m_bufferStates[typeId];
        }

        const char* spaceStr = (space == BufferSpace::Engine) ? "Engine" : "Custom";
        LogInfo(LogUniform, "%s Buffer registered: Slot=%u, Space=%u, Size=%zu, Freq=%d, Count=%zu",
                spaceStr, slotId, spaceValue, alignedSize, static_cast<int>(freq), ringBufferCount);
        return true;
    }

    // [REMOVED] RegisterCustomBuffer - merged into RegisterBufferInternal

    // ========================================================================
    // [REFACTORED] UploadBufferInternal - Unified buffer upload
    // Routes based on space stored in UniformBufferState
    // ========================================================================
    bool UniformManager::UploadBufferInternal(const std::type_index& typeId, const void* data, size_t size)
    {
        // Find buffer state by TypeId
        auto it = m_bufferStates.find(typeId);
        if (it == m_bufferStates.end())
        {
            LogError(LogUniform, "Buffer not registered: %s", typeId.name());
            return false;
        }

        UniformBufferState& state = it->second;

        if (size > state.elementSize)
        {
            LogWarn(LogUniform, "Data size (%zu) exceeds element size (%zu)", size, state.elementSize);
        }

        // Strategy pattern: get current write index
        size_t currentIndex = state.GetCurrentWriteIndex();
        void*  destPtr      = state.GetDataAt(currentIndex);

        if (!destPtr)
        {
            LogError(LogUniform, "Failed to get mapped data for buffer slot %u", state.slot);
            return false;
        }

        std::memcpy(destPtr, data, size);

        // Update Delayed Fill cache
        std::memcpy(state.lastUpdatedValue.data(), data, size);
        state.lastUpdatedIndex = currentIndex;

        // Strategy pattern: post-upload hook (PerDispatch auto-advances here)
        if (state.strategy)
        {
            state.strategy->OnPostUpload();
        }

        return true;
    }

    // ========================================================================
    // UpdateRootCBVOffset - Placeholder (actual binding in RendererSubsystem)
    // ========================================================================
    void UniformManager::UpdateRootCBVOffset(uint32_t slotId, size_t currentIndex)
    {
        UNUSED(slotId)
        UNUSED(currentIndex)
    }

    void UniformManager::ResetCustomDescriptorPageState(uint32_t frameIndex)
    {
        m_customDescriptorPageState.frameIndex       = frameIndex;
        m_customDescriptorPageState.activePageIndex  = 0;
        m_customDescriptorPageState.hasCommittedPage = false;
        m_customDescriptorPageState.committedReadIndices.fill(0);
        m_customDescriptorPageState.committedReadIndicesValid.reset();
    }

    void UniformManager::CaptureCommittedCustomBufferReadState()
    {
        m_customDescriptorPageState.committedReadIndices.fill(0);
        m_customDescriptorPageState.committedReadIndicesValid.reset();

        // CAUTION: The current descriptor-page model snapshots every registered
        // Custom-space buffer. Requirement 7 is still incomplete because page
        // reuse is not yet keyed by the active program's real custom-slot usage.
        for (const auto& entry : m_bufferStates)
        {
            const UniformBufferState& state = entry.second;
            if (state.space != BufferSpace::Custom || !state.buffer)
            {
                continue;
            }

            if (state.slot >= BindlessRootSignature::MAX_CUSTOM_BUFFERS)
            {
                ERROR_RECOVERABLE(Stringf("UniformManager::CaptureCommittedCustomBufferReadState encountered invalid custom slot %u",
                    state.slot));
                continue;
            }

            m_customDescriptorPageState.committedReadIndices[state.slot] = state.GetCurrentReadIndex();
            m_customDescriptorPageState.committedReadIndicesValid.set(state.slot);
        }
    }

    bool UniformManager::IsCurrentCustomBufferStateCommitted() const
    {
        if (!m_customDescriptorPageState.hasCommittedPage)
        {
            return false;
        }

        // TODO(perf): Compare only the custom slots consumed by the active draw.
        // The current global comparison allows unrelated sky-only PerObject
        // buffers to force descriptor-page churn in shadow/terrain passes.
        for (const auto& entry : m_bufferStates)
        {
            const UniformBufferState& state = entry.second;
            if (state.space != BufferSpace::Custom || !state.buffer)
            {
                continue;
            }

            if (state.slot >= BindlessRootSignature::MAX_CUSTOM_BUFFERS)
            {
                return false;
            }

            if (!m_customDescriptorPageState.committedReadIndicesValid.test(state.slot))
            {
                return false;
            }

            if (m_customDescriptorPageState.committedReadIndices[state.slot] != state.GetCurrentReadIndex())
            {
                return false;
            }
        }

        return true;
    }

    uint32_t UniformManager::GetCustomBufferDescriptorPoolIndex(uint32_t slotId, uint32_t pageIndex) const
    {
        const uint32_t frameIndex      = D3D12RenderSystem::GetFrameIndex();
        const uint32_t globalPageIndex = frameIndex * CUSTOM_DESCRIPTOR_PAGES_PER_FRAME + pageIndex;
        return globalPageIndex * BindlessRootSignature::MAX_CUSTOM_BUFFERS + slotId;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE UniformManager::GetCustomBufferDescriptorTableGPUHandleForPage(uint32_t pageIndex) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = m_customBufferDescriptorTableBaseGPUHandle;
        handle.ptr += static_cast<uint64_t>(GetCustomBufferDescriptorPoolIndex(0, pageIndex)) * m_cbvSrvUavDescriptorIncrementSize;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE UniformManager::PrepareCustomBufferDescriptorTableForCurrentState()
    {
        const uint32_t currentFrameIndex = D3D12RenderSystem::GetFrameIndex();
        if (m_customDescriptorPageState.frameIndex != currentFrameIndex)
        {
            ResetCustomDescriptorPageState(currentFrameIndex);
        }

        if (IsCurrentCustomBufferStateCommitted())
        {
            return GetCustomBufferDescriptorTableGPUHandleForPage(m_customDescriptorPageState.activePageIndex);
        }

        if (m_customDescriptorPageState.hasCommittedPage)
        {
            const uint32_t nextPageIndex = m_customDescriptorPageState.activePageIndex + 1;
            if (nextPageIndex >= CUSTOM_DESCRIPTOR_PAGES_PER_FRAME)
            {
                ERROR_AND_DIE(Stringf("Custom descriptor page overflow: frame=%u transitions=%u capacity=%u",
                    currentFrameIndex,
                    nextPageIndex,
                    CUSTOM_DESCRIPTOR_PAGES_PER_FRAME));
            }

            m_customDescriptorPageState.activePageIndex = nextPageIndex;
        }

        CommitCustomBufferDescriptorPageForPage(m_customDescriptorPageState.activePageIndex);
        CaptureCommittedCustomBufferReadState();
        m_customDescriptorPageState.hasCommittedPage = true;

        return GetCustomBufferDescriptorTableGPUHandleForPage(m_customDescriptorPageState.activePageIndex);
    }

    // ========================================================================
    // UpdateDescriptorTableOffset
    // ========================================================================
    void UniformManager::UpdateDescriptorTableOffset(const UniformBufferState& state, size_t currentIndex, uint32_t pageIndex)
    {
        if (!state.buffer)
        {
            return;
        }

        D3D12_GPU_VIRTUAL_ADDRESS bufferAddress = state.GetGPUVirtualAddress();
        bufferAddress                           += currentIndex * state.elementSize;

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation                  = bufferAddress;
        cbvDesc.SizeInBytes                     = static_cast<UINT>(state.elementSize);

        const uint32_t descriptorIndex = GetCustomBufferDescriptorPoolIndex(state.slot, pageIndex);

        if (descriptorIndex >= m_customBufferDescriptorPool.size())
        {
            LogError(LogUniform, "Custom Buffer descriptor index %u out of range (pool size=%zu)",
                     descriptorIndex, m_customBufferDescriptorPool.size());
            return;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_customBufferDescriptorPool[descriptorIndex].cpuHandle;

        auto* device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            LogError(LogUniform, "D3D12 device not available");
            return;
        }

        device->CreateConstantBufferView(&cbvDesc, cpuHandle);
    }

    void UniformManager::CommitCustomBufferDescriptorPageForPage(uint32_t pageIndex)
    {
        // TODO(perf): Commit only the active program's custom-slot subset once
        // that usage mask exists. Rebuilding every Custom-space slot keeps page
        // selection broader than the draw's true dependency set.
        for (auto& entry : m_bufferStates)
        {
            const UniformBufferState& state = entry.second;

            if (state.space != BufferSpace::Custom || !state.buffer)
            {
                continue;
            }

            UpdateDescriptorTableOffset(state, state.GetCurrentReadIndex(), pageIndex);
        }
    }

    // ========================================================================
    // AllocateCustomBufferDescriptor
    // ========================================================================
    bool UniformManager::AllocateCustomBufferDescriptor(uint32_t slotId)
    {
        if (m_customBufferDescriptors.find(slotId) != m_customBufferDescriptors.end())
        {
            LogWarn(LogUniform, "Descriptor already allocated for slot %u", slotId);
            return true;
        }

        if (!m_initialized || m_customBufferDescriptorPool.empty())
        {
            LogError(LogUniform, "UniformManager not initialized or pool empty");
            return false;
        }

        uint32_t descriptorIndex = slotId;
        if (descriptorIndex >= BindlessRootSignature::MAX_CUSTOM_BUFFERS)
        {
            LogError(LogUniform, "Slot %u exceeds MAX_CUSTOM_BUFFERS (%u)", slotId, BindlessRootSignature::MAX_CUSTOM_BUFFERS);
            return false;
        }

        auto allocation = m_customBufferDescriptorPool[descriptorIndex];
        if (!allocation.isValid)
        {
            LogError(LogUniform, "Failed to get descriptor for slot %u (index %u)", slotId, descriptorIndex);
            return false;
        }

        CustomBufferDescriptor desc;
        desc.allocation = allocation;
        desc.slotId     = slotId;
        desc.isValid    = true;

        m_customBufferDescriptors[slotId] = desc;

        LogInfo(LogUniform, "Allocated descriptor: Slot=%u -> Index=%u", slotId, descriptorIndex);
        return true;
    }

    // ========================================================================
    // FreeCustomBufferDescriptor
    // ========================================================================
    void UniformManager::FreeCustomBufferDescriptor(uint32_t slotId)
    {
        auto it = m_customBufferDescriptors.find(slotId);
        if (it != m_customBufferDescriptors.end())
        {
            m_customBufferDescriptors.erase(it);
            LogInfo(LogUniform, "Freed descriptor for slot %u", slotId);
        }
    }

    // ========================================================================
    // GetCustomBufferCPUHandle
    // ========================================================================
    D3D12_CPU_DESCRIPTOR_HANDLE UniformManager::GetCustomBufferCPUHandle(uint32_t slotId) const
    {
        auto it = m_customBufferDescriptors.find(slotId);
        if (it != m_customBufferDescriptors.end())
        {
            return it->second.allocation.cpuHandle;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE nullHandle = {};
        nullHandle.ptr                         = 0;
        return nullHandle;
    }

    // ========================================================================
    // GetEngineBufferGPUAddress
    // ========================================================================
    D3D12_GPU_VIRTUAL_ADDRESS UniformManager::GetEngineBufferGPUAddress(uint32_t slotId) const
    {
        // [PERF] Direct array lookup instead of two hash map lookups
        if (slotId >= ENGINE_SLOT_COUNT)
        {
            return 0;
        }

        auto* state = m_engineBufferCache[slotId];
        if (!state || !state->buffer)
        {
            return 0;
        }

        // Strategy pattern: use read index for GPU address binding
        size_t currentIndex = state->GetCurrentReadIndex();

        D3D12_GPU_VIRTUAL_ADDRESS baseAddress    = state->GetGPUVirtualAddress();
        D3D12_GPU_VIRTUAL_ADDRESS currentAddress = baseAddress + currentIndex * state->elementSize;

        return currentAddress;
    }
} // namespace enigma::graphic
