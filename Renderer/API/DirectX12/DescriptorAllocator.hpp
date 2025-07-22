#pragma once
#include <array>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <queue>
#include <mutex>

using Microsoft::WRL::ComPtr;

// Descriptor handle wrapper
struct DescriptorHandle
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpu       = D3D12_CPU_DESCRIPTOR_HANDLE();
    D3D12_GPU_DESCRIPTOR_HANDLE gpu       = D3D12_GPU_DESCRIPTOR_HANDLE();
    UINT                        heapIndex = UINT_MAX;

    bool IsValid() const { return heapIndex != UINT_MAX; }
    bool IsShaderVisible() const { return gpu.ptr != 0; }
};

// Descriptor heap type
enum class DescriptorHeapType
{
    CBV_SRV_UAV,
    RTV,
    DSV,
    SAMPLER
};

/**
 * @class DescriptorAllocator
 * @brief Handles the allocation and management of descriptors in a DirectX 12 Descriptor Heap.
 *
 * This class provides mechanisms to allocate and free both single and consecutive
 * ranges of descriptors in a descriptor heap. It also provides utility methods
 * to retrieve the underlying descriptor heap and individual descriptor handles.
 */
class DescriptorAllocator
{
public:
    DescriptorAllocator(ID3D12Device* device, DescriptorHeapType type, UINT numDescriptors, bool shaderVisible);
    ~DescriptorAllocator();
    // Allocate a single descriptor
    DescriptorHandle Allocate();
    // Allocate multiple consecutive descriptors
    DescriptorHandle AllocateRange(UINT count);
    // Free descriptor (for CPU heap)
    void Free(const DescriptorHandle& handle);
    void FreeRange(const DescriptorHandle& handle, UINT count);
    // Get the heap
    ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
    // Get the CPU handle (for writing to the GPU heap)
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index) const;

    void Reset(); // To Reset the Ring buffer

    UINT GetDescriptorSize() const { return m_descriptorSize; }
    UINT GetNumDescriptors() const { return m_numDescriptors; }
    UINT GetAllocatedCount() const { return m_currentOffset; }

private:
    ComPtr<ID3D12DescriptorHeap>                m_heap;
    [[maybe_unused]] D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
    UINT                                        m_descriptorSize;
    UINT                                        m_numDescriptors;
    UINT                                        m_currentOffset;
    bool                                        m_shaderVisible;

    /// CPU side free list
    std::queue<UINT> m_freeList;
    std::mutex       m_mutex; // Thread safety
};

/**
 * @class TieredDescriptorHandler
 * @brief Manages descriptor heaps for resource binding in DirectX 12 with support for both persistent and per-frame descriptors.
 *
 * This class provides a mechanism to manage GPU and CPU descriptor heaps as well as
 * handle transitions between these heaps across frames. It supports creation, release,
 * and binding of persistent descriptors and frame-specific descriptors, along with utilities
 * to copy descriptors between heaps.
 */
class TieredDescriptorHandler
{
public:
    TieredDescriptorHandler(ID3D12Device* device);
    ~TieredDescriptorHandler();

    // Initialization
    void Startup(UINT maxTexturesPerFrame = 512, UINT maxCBVsPerFrame = 256);

    // Frame Management
    void BeginFrame(UINT frameIndex);
    void EndFrame();

    // CPU heap management (persistent storage)
    DescriptorHandle CreatePersistentCBV(const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc);
    DescriptorHandle CreatePersistentSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc);
    DescriptorHandle CreatePersistentUAV(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc);
    void             ReleasePersistentDescriptor(const DescriptorHandle& handle);

    // GPU heap management (per frame)
    struct FrameDescriptorTable
    {
        DescriptorHandle baseHandle;
        UINT             numDescriptors;
    };

    // Allocate a descriptor table for the current frame
    FrameDescriptorTable AllocateFrameDescriptorTable(UINT numDescriptors);

    // Create descriptors directly in the GPU heap of the current frame (for temporary resources such as CBV)
    DescriptorHandle CreateFrameCBV(const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc);
    DescriptorHandle CreateFrameSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc);


    // Copy from CPU heap to GPU heap
    void CopyDescriptors(const FrameDescriptorTable& dest, const DescriptorHandle* srcHandles, UINT count);
    void CopyDescriptorsRange(const FrameDescriptorTable& dest, UINT destOffset, const DescriptorHandle& src, UINT srcOffset, UINT count);

    // Bind the heap to the command list
    void BindToCommandList(ID3D12GraphicsCommandList* cmdList);

    // Get the GPU heap of the current frame
    ID3D12DescriptorHeap* GetCurrentFrameHeap() const;

private:
    ID3D12Device* m_device;

    // CPU heaps persistent storage
    static constexpr UINT                kMaxPersistentDescriptors = 10000;
    std::unique_ptr<DescriptorAllocator> m_persistentCBVSRVUAV;

    // GPU heaps - per-frame ring buffer
    static constexpr UINT kFrameCount          = 4;
    static constexpr UINT kMaxFrameDescriptors = 65536; // Smaller to avoid exceeding the limit

    struct FrameHeap
    {
        std::unique_ptr<DescriptorAllocator> allocator;
        UINT                                 frameStartOffset;
    };

    std::array<FrameHeap, kFrameCount> m_frameHeaps;
    UINT                               m_currentFrameIndex;

    UINT m_cbvSrvUavDescriptorSize; //Descriptor size cache
};

// Simplified descriptor set manager (for batch binding)
class DescriptorSet
{
public:
    static constexpr UINT kMaxDescriptors = 16;

    DescriptorSet() { Reset(); }

    void Reset()
    {
        m_numDescriptors = 0;
        m_handles.fill(DescriptorHandle{});
    }

    void AddDescriptor(const DescriptorHandle& handle)
    {
        if (m_numDescriptors < kMaxDescriptors)
        {
            m_handles[m_numDescriptors++] = handle;
        }
    }

    const DescriptorHandle* GetHandles() const { return m_handles.data(); }
    UINT                    GetCount() const { return m_numDescriptors; }

private:
    std::array<DescriptorHandle, kMaxDescriptors> m_handles;
    UINT                                          m_numDescriptors;
};