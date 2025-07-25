#include "DescriptorAllocator.hpp"

#include "Engine/Renderer/GraphicsError.hpp"

DescriptorAllocator::DescriptorAllocator(ID3D12Device* device, DescriptorHeapType type, UINT numDescriptors, bool shaderVisible) : m_numDescriptors(numDescriptors), m_currentOffset(0),
                                                                                                                                   m_shaderVisible(shaderVisible)
{
    //Convert heap type
    switch (type)
    {
    case DescriptorHeapType::CBV_SRV_UAV:
        m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        break;
    case DescriptorHeapType::RTV:
        m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        break;
    case DescriptorHeapType::DSV:
        m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        break;
    case DescriptorHeapType::SAMPLER:
        m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        break;
    }

    // Create a descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type                       = m_heapType;
    heapDesc.NumDescriptors             = numDescriptors;
    heapDesc.Flags                      = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask                   = 0;

    device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap)) >> chk;
    // Get the descriptor size
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(m_heapType);
}

DescriptorAllocator::~DescriptorAllocator()
{
    if (m_heap)
    {
        m_heap.Reset();
    }
}

DescriptorHandle DescriptorAllocator::Allocate()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    DescriptorHandle handle;

    // If there is a free one, use it first
    if (!m_shaderVisible && !m_freeList.empty())
    {
        handle.heapIndex = m_freeList.front();
        m_freeList.pop();
    }
    else if (m_currentOffset < m_numDescriptors)
    {
        handle.heapIndex = m_currentOffset++;
    }
    else
    {
        // Heap is full, return an invalid handle
        ERROR_AND_DIE("Descriptor heap is full")
        return handle;
    }

    // Calculation handle
    handle.cpu = GetCPUHandle(handle.heapIndex);
    if (m_shaderVisible)
    {
        handle.gpu = GetGPUHandle(handle.heapIndex);
    }

    return handle;
}

DescriptorHandle DescriptorAllocator::AllocateRange(UINT count)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    DescriptorHandle handle;

    if (m_currentOffset + count > m_numDescriptors)
    {
        ERROR_RECOVERABLE("Not enough space in descriptor heap for range allocation");
        return handle;
    }

    handle.heapIndex = m_currentOffset;
    m_currentOffset += count;

    handle.cpu = GetCPUHandle(handle.heapIndex);
    if (m_shaderVisible)
    {
        handle.gpu = GetGPUHandle(handle.heapIndex);
    }

    return handle;
}

void DescriptorAllocator::Free(const DescriptorHandle& handle)
{
    if (!m_shaderVisible && handle.IsValid())
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_freeList.push(handle.heapIndex);
    }
}

void DescriptorAllocator::FreeRange(const DescriptorHandle& handle, UINT count)
{
    if (!m_shaderVisible && handle.IsValid())
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (UINT i = 0; i < count; ++i)
        {
            m_freeList.push(handle.heapIndex + i);
        }
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetCPUHandle(UINT index) const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index), m_descriptorSize);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetGPUHandle(UINT index) const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(index), m_descriptorSize);
}

void DescriptorAllocator::Reset()
{
    m_currentOffset = 0;
    // Clear the free list
    std::queue<UINT> empty;
    std::swap(m_freeList, empty);
}

TieredDescriptorHandler::TieredDescriptorHandler(ID3D12Device* device) : m_device(device), m_currentFrameIndex(0)
{
    m_frameHeaps              = {0};
    m_cbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

TieredDescriptorHandler::~TieredDescriptorHandler()
{
    // 清理持久化的CPU heap
    if (m_persistentCBVSRVUAV)
    {
        m_persistentCBVSRVUAV.reset();
    }

    // 清理每帧的GPU heaps
    for (auto& frameHeap : m_frameHeaps)
    {
        if (frameHeap.allocator)
        {
            frameHeap.allocator.reset();
        }
    }
}

void TieredDescriptorHandler::Startup(UINT maxTexturesPerFrame, UINT maxCBVsPerFrame)
{
    UNUSED(maxTexturesPerFrame)
    UNUSED(maxCBVsPerFrame)
    // Create a persistent CPU heap
    m_persistentCBVSRVUAV = std::make_unique<DescriptorAllocator>(m_device, DescriptorHeapType::CBV_SRV_UAV, kMaxPersistentDescriptors, false);

    // Create GPU heap for each frame
    for (UINT i = 0; i < kFrameCount; ++i)
    {
        m_frameHeaps[i].allocator        = std::make_unique<DescriptorAllocator>(m_device, DescriptorHeapType::CBV_SRV_UAV, kMaxFrameDescriptors, true);
        m_frameHeaps[i].frameStartOffset = 0;
    }
}

void TieredDescriptorHandler::BeginFrame(UINT frameIndex)
{
    m_currentFrameIndex = frameIndex % kFrameCount;

    // Reset the allocator for the current frame
    auto& frameHeap = m_frameHeaps[m_currentFrameIndex];
    frameHeap.allocator->Reset();
    frameHeap.frameStartOffset = 0;
}

void TieredDescriptorHandler::EndFrame()
{
}

DescriptorHandle TieredDescriptorHandler::CreatePersistentCBV(const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc)
{
    DescriptorHandle handle = m_persistentCBVSRVUAV->Allocate();
    if (handle.IsValid())
    {
        m_device->CreateConstantBufferView(&desc, handle.cpu);
    }
    return handle;
}

DescriptorHandle TieredDescriptorHandler::CreatePersistentSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
{
    DescriptorHandle handle = m_persistentCBVSRVUAV->Allocate();
    if (handle.IsValid())
    {
        m_device->CreateShaderResourceView(resource, &desc, handle.cpu);
    }
    return handle;
}

DescriptorHandle TieredDescriptorHandler::CreatePersistentUAV(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc)
{
    DescriptorHandle handle = m_persistentCBVSRVUAV->Allocate();
    if (handle.IsValid())
    {
        m_device->CreateUnorderedAccessView(resource, nullptr, &desc, handle.cpu);
    }
    return handle;
}

void TieredDescriptorHandler::ReleasePersistentDescriptor(const DescriptorHandle& handle)
{
    m_persistentCBVSRVUAV->Free(handle);
}

TieredDescriptorHandler::FrameDescriptorTable TieredDescriptorHandler::AllocateFrameDescriptorTable(UINT numDescriptors)
{
    FrameDescriptorTable table;

    auto& frameHeap      = m_frameHeaps[m_currentFrameIndex];
    table.baseHandle     = frameHeap.allocator->AllocateRange(numDescriptors);
    table.numDescriptors = numDescriptors;

    return table;
}

DescriptorHandle TieredDescriptorHandler::CreateFrameCBV(const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc)
{
    // Get the descriptor heap allocator for the current frame
    auto& frameAllocator = m_frameHeaps[m_currentFrameIndex].allocator;

    // Allocate a descriptor
    DescriptorHandle handle = frameAllocator->Allocate();
    if (handle.IsValid())
    {
        // Create CBV on GPU heap
        m_device->CreateConstantBufferView(&desc, handle.cpu);
    }
    return handle;
}

DescriptorHandle TieredDescriptorHandler::CreateFrameSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
{
    // Get the descriptor heap allocator for the current frame
    auto& frameAllocator = m_frameHeaps[m_currentFrameIndex].allocator;

    // Allocate a descriptor
    DescriptorHandle handle = frameAllocator->Allocate();
    if (handle.IsValid())
    {
        // Create SRV on GPU heap
        m_device->CreateShaderResourceView(resource, &desc, handle.cpu);
    }
    return handle;
}

void TieredDescriptorHandler::CopyDescriptors(const FrameDescriptorTable& dest, const DescriptorHandle* srcHandles, UINT count)
{
    if (count > dest.numDescriptors)
    {
        ERROR_RECOVERABLE("Trying to copy more descriptors than allocated in table");
        return;
    }

    for (UINT i = 0; i < count; ++i)
    {
        if (srcHandles[i].IsValid())
        {
            // Check if the source descriptor is from the GPU heap
            if (!srcHandles[i].IsShaderVisible())
            {
                ERROR_RECOVERABLE("Attempting to copy from CPU-only descriptor")
                continue; // Skip this descriptor
            }
            D3D12_CPU_DESCRIPTOR_HANDLE destCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(dest.baseHandle.cpu, static_cast<INT>(i), m_cbvSrvUavDescriptorSize);
            m_device->CopyDescriptorsSimple(1, destCPU, srcHandles[i].cpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
    }
}

void TieredDescriptorHandler::CopyDescriptorsRange(const FrameDescriptorTable& dest, UINT destOffset, const DescriptorHandle& src, UINT srcOffset, UINT count)
{
    if (destOffset + count > dest.numDescriptors)
    {
        ERROR_RECOVERABLE("Descriptor copy would exceed table bounds")
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE destCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(dest.baseHandle.cpu, static_cast<INT>(destOffset), m_cbvSrvUavDescriptorSize);
    D3D12_CPU_DESCRIPTOR_HANDLE srcCPU  = CD3DX12_CPU_DESCRIPTOR_HANDLE(src.cpu, static_cast<INT>(srcOffset), m_cbvSrvUavDescriptorSize);

    m_device->CopyDescriptorsSimple(count, destCPU, srcCPU, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void TieredDescriptorHandler::BindToCommandList(ID3D12GraphicsCommandList* cmdList)
{
    auto                  heap    = m_frameHeaps[m_currentFrameIndex].allocator->GetHeap();
    ID3D12DescriptorHeap* heaps[] = {heap};
    cmdList->SetDescriptorHeaps(1, heaps);
}

ID3D12DescriptorHeap* TieredDescriptorHandler::GetCurrentFrameHeap() const
{
    return m_frameHeaps[m_currentFrameIndex].allocator->GetHeap();
}
