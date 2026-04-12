#include "D12IndexBuffer.hpp"
#include <cassert>
#include <sstream>

using namespace enigma::graphic;

namespace
{
    BufferCreateInfo MakeDynamicIndexBufferCreateInfo(size_t size, const void* initialData, const char* debugName)
    {
        BufferCreateInfo info;
        info.size         = size;
        info.usage        = BufferUsage::IndexBuffer;
        info.memoryAccess = MemoryAccess::CPUWritable;
        info.initialData  = initialData;
        info.debugName    = debugName;
        info.usagePolicy  = BufferUsagePolicy::DynamicUpload;
        return info;
    }

    BufferCreateInfo NormalizeIndexBufferCreateInfo(const BufferCreateInfo& createInfo)
    {
        BufferCreateInfo normalized = createInfo;
        normalized.usage = BufferUsage::IndexBuffer;

        switch (normalized.usagePolicy)
        {
        case BufferUsagePolicy::DynamicUpload:
            normalized.memoryAccess = MemoryAccess::CPUWritable;
            break;
        case BufferUsagePolicy::GpuOnlyCopyReady:
            if (normalized.memoryAccess != MemoryAccess::GPUOnly)
            {
                ERROR_RECOVERABLE("D12IndexBuffer: GpuOnlyCopyReady forces GPUOnly memory access");
            }
            normalized.memoryAccess = MemoryAccess::GPUOnly;
            break;
        default:
            normalized.memoryAccess = MemoryAccess::CPUWritable;
            break;
        }

        return normalized;
    }
}

D12IndexBuffer::D12IndexBuffer(size_t size, const void* initialData, const char* debugName)
    : D12IndexBuffer(MakeDynamicIndexBufferCreateInfo(size, initialData, debugName))
{
}

D12IndexBuffer::D12IndexBuffer(const BufferCreateInfo& createInfo)
    : D12Buffer(NormalizeIndexBufferCreateInfo(createInfo))
      , m_view{}
{
    assert(GetSize() % INDEX_SIZE == 0 && "Buffer size must be multiple of INDEX_SIZE (4 bytes)");
    UpdateView();

    if (SupportsPersistentMapping())
    {
        MapPersistent();
    }
}

void D12IndexBuffer::UpdateView()
{
    auto* resource = GetResource();
    if (!resource)
    {
        m_view = {};
        return;
    }

    m_view.BufferLocation = resource->GetGPUVirtualAddress();
    m_view.SizeInBytes    = static_cast<UINT>(GetSize());
    m_view.Format         = DXGI_FORMAT_R32_UINT;
}

void D12IndexBuffer::SetDebugName(const std::string& name)
{
    D12Buffer::SetDebugName(name);
}

std::string D12IndexBuffer::GetDebugInfo() const
{
    std::ostringstream oss;
    oss << "IndexBuffer [" << GetDebugName() << "]\n";
    oss << "  Size: " << GetSize() << " bytes\n";
    oss << "  Format: Uint32 (fixed)\\n";
    oss << "  Index Count: " << GetIndexCount() << "\n";
    oss << "  GPU Address: 0x" << std::hex << m_view.BufferLocation << std::dec << "\n";
    oss << "  Valid: " << (IsValid() ? "Yes" : "No");
    return oss.str();
}

void D12IndexBuffer::ReleaseResource()
{
    m_view = {};
    D12Buffer::ReleaseResource();
}
