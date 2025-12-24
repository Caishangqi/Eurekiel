#include "D12IndexBuffer.hpp"
#include <cassert>
#include <sstream>

using namespace enigma::graphic;

// ============================================================================
// Constructor and destructor
// ============================================================================
D12IndexBuffer::D12IndexBuffer(size_t size, IndexFormat format, const void* initialData, const char* debugName)
    : D12Buffer([&]()
      {
          BufferCreateInfo info;
          info.size  = size;
          info.usage = BufferUsage::IndexBuffer;
          //Teaching points: The application layer IndexBuffer needs to be updated frequently, and CPUWritable should be used uniformly.
          // Even if there is no initialData, it may be updated later through UpdateBuffer
          // Refer to Iris: Dynamic index data uses UPLOAD heap (D3D12_HEAP_TYPE_UPLOAD)
          info.memoryAccess = MemoryAccess::CPUWritable;
          info.initialData  = initialData;
          info.debugName    = debugName;
          return info;
      }())
      , m_format(format)
      , m_view{}
{
    //Teaching Points: Parameter Validation - Make sure size is an integer multiple of the index size
    const size_t indexSize = (format == IndexFormat::Uint16) ? 2 : 4;
    assert(size % indexSize == 0 && "Buffer size must be multiple of index size");
    UNUSED(indexSize)
    //Teaching points: Create D3D12_INDEX_BUFFER_VIEW
    UpdateView();

    // [NEW] Persistent mapping buffer, supports Ring Buffer copy strategy of DrawVertexBuffer
    // Teaching points: The IndexBuffer of the UPLOAD heap needs to be persistently mapped so that:
    // 1. DrawVertexBuffer can read data through GetPersistentMappedData()
    // 2. Data can be copied to the renderer’s Ring Buffer
    // 3. Avoid the overhead of Map/Unmap per frame
    MapPersistent();
}

// ============================================================================
// IndexBufferView management
// ============================================================================
void D12IndexBuffer::UpdateView()
{
    auto* resource = GetResource();
    if (!resource)
    {
        m_view = {};
        return;
    }

    //Teaching points: Construct D3D12_INDEX_BUFFER_VIEW
    // 1. BufferLocation: GPU virtual address
    // 2. SizeInBytes: total buffer size
    // 3. Format: index format (R16_UINT or R32_UINT)
    m_view.BufferLocation = resource->GetGPUVirtualAddress();
    m_view.SizeInBytes    = static_cast<UINT>(GetSize());
    m_view.Format         = IndexFormatToDXGI(m_format);
}

// ============================================================================
// Debug interface implementation
// ============================================================================
void D12IndexBuffer::SetDebugName(const std::string& name)
{
    D12Buffer::SetDebugName(name);
}

std::string D12IndexBuffer::GetDebugInfo() const
{
    std::ostringstream oss;
    oss << "IndexBuffer [" << GetDebugName() << "]\n";
    oss << "  Size: " << GetSize() << " bytes\n";
    oss << "  Format: " << (m_format == IndexFormat::Uint16 ? "Uint16" : "Uint32") << "\n";
    oss << "  Index Count: " << GetIndexCount() << "\n";
    oss << "  GPU Address: 0x" << std::hex << m_view.BufferLocation << std::dec << "\n";
    oss << "  Valid: " << (IsValid() ? "Yes" : "No");
    return oss.str();
}

// ============================================================================
// Resource release
// ============================================================================
void D12IndexBuffer::ReleaseResource()
{
    m_view = {};
    D12Buffer::ReleaseResource();
}
