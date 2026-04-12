#include "D12VertexBuffer.hpp"
#include <cassert>
#include <sstream>

using namespace enigma::graphic;

namespace
{
    BufferCreateInfo MakeDynamicVertexBufferCreateInfo(size_t size, size_t stride, const void* initialData, const char* debugName)
    {
        BufferCreateInfo info;
        info.size        = size;
        info.usage       = BufferUsage::VertexBuffer;
        info.memoryAccess = MemoryAccess::CPUWritable;
        info.initialData = initialData;
        info.debugName   = debugName;
        info.byteStride  = stride;
        info.usagePolicy = BufferUsagePolicy::DynamicUpload;
        return info;
    }

    BufferCreateInfo NormalizeVertexBufferCreateInfo(const BufferCreateInfo& createInfo)
    {
        BufferCreateInfo normalized = createInfo;
        normalized.usage = BufferUsage::VertexBuffer;

        switch (normalized.usagePolicy)
        {
        case BufferUsagePolicy::DynamicUpload:
            normalized.memoryAccess = MemoryAccess::CPUWritable;
            break;
        case BufferUsagePolicy::GpuOnlyCopyReady:
            if (normalized.memoryAccess != MemoryAccess::GPUOnly)
            {
                ERROR_RECOVERABLE("D12VertexBuffer: GpuOnlyCopyReady forces GPUOnly memory access");
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

D12VertexBuffer::D12VertexBuffer(size_t size, size_t stride, const void* initialData, const char* debugName)
    : D12VertexBuffer(MakeDynamicVertexBufferCreateInfo(size, stride, initialData, debugName))
{
}

D12VertexBuffer::D12VertexBuffer(const BufferCreateInfo& createInfo)
    : D12Buffer(NormalizeVertexBufferCreateInfo(createInfo))
      , m_stride(createInfo.byteStride)
      , m_view{}
{
    assert(m_stride > 0 && "Vertex stride must be greater than 0");
    assert(GetSize() % m_stride == 0 && "Buffer size must be multiple of stride");

    UpdateView();

    if (SupportsPersistentMapping())
    {
        MapPersistent();
    }
}

void D12VertexBuffer::UpdateView()
{
    auto* resource = GetResource();
    if (!resource)
    {
        m_view = {};
        return;
    }

    m_view.BufferLocation = resource->GetGPUVirtualAddress();
    m_view.SizeInBytes    = static_cast<UINT>(GetSize());
    m_view.StrideInBytes  = static_cast<UINT>(m_stride);
}

void D12VertexBuffer::SetDebugName(const std::string& name)
{
    D12Buffer::SetDebugName(name);
}

std::string D12VertexBuffer::GetDebugInfo() const
{
    // Teaching points: Provide debugging information specific to the vertex buffer
    std::ostringstream oss;
    oss << "VertexBuffer [" << GetDebugName() << "]\n";
    oss << "  Size: " << GetSize() << " bytes\n";
    oss << "  Stride: " << m_stride << " bytes\n";
    oss << "  Vertex Count: " << GetVertexCount() << "\n";
    oss << "  GPU Address: 0x" << std::hex << m_view.BufferLocation << std::dec << "\n";
    oss << "  Valid: " << (IsValid() ? "Yes" : "No");
    return oss.str();
}

void D12VertexBuffer::ReleaseResource()
{
    m_view = {};
    D12Buffer::ReleaseResource();
}
