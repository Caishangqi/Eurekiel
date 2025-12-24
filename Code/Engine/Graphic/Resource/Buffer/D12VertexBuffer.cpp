#include "D12VertexBuffer.hpp"
#include <cassert>
#include <sstream>

using namespace enigma::graphic;

// ============================================================================
// Constructor and destructor
// ============================================================================
D12VertexBuffer::D12VertexBuffer(size_t size, size_t stride, const void* initialData, const char* debugName)
    : D12Buffer([&]()
      {
          BufferCreateInfo info;
          info.size  = size;
          info.usage = BufferUsage::VertexBuffer;
          // Teaching points: The application layer VertexBuffer needs to be updated frequently, so use CPUWritable uniformly.
          // Even if there is no initialData, it may be updated later through UpdateBuffer
          // Refer to Iris: Dynamic vertex data uses UPLOAD heap (D3D12_HEAP_TYPE_UPLOAD)
          info.memoryAccess = MemoryAccess::CPUWritable;
          info.initialData  = initialData;
          info.debugName    = debugName;
          return info;
      }())
      , m_stride(stride)
      , m_view{}
{
    //Teaching points: Parameter verification - ensure the legality of the input
    assert(stride > 0 && "Vertex stride must be greater than 0");
    assert(size % stride == 0 && "Buffer size must be multiple of stride");

    //Teaching points: Create D3D12_VERTEX_BUFFER_VIEW
    // This is the key structure for DirectX 12 bound vertex buffers
    UpdateView();

    // [NEW] Persistent mapping buffer, supports Ring Buffer copy strategy of DrawVertexBuffer
    // Teaching points: The VertexBuffer of the UPLOAD heap needs to be persistently mapped so that:
    // 1. DrawVertexBuffer can read data through GetPersistentMappedData()
    // 2. Data can be copied to the renderer’s Ring Buffer
    // 3. Avoid the overhead of Map/Unmap per frame
    MapPersistent();
}

// ============================================================================
// VertexBufferView management
// ============================================================================
void D12VertexBuffer::UpdateView()
{
    // Teaching points: Obtain the resource pointer of the base class
    auto* resource = GetResource();
    if (!resource)
    {
        // Clear the view when the resource is invalid
        m_view = {};
        return;
    }

    // Teaching points: Construct D3D12_VERTEX_BUFFER_VIEW
    // 1. BufferLocation: GPU virtual address (obtained from ID3D12Resource)
    // 2. SizeInBytes: total buffer size
    // 3. StrideInBytes: single vertex size
    m_view.BufferLocation = resource->GetGPUVirtualAddress();
    m_view.SizeInBytes    = static_cast<UINT>(GetSize());
    m_view.StrideInBytes  = static_cast<UINT>(m_stride);
}

// ============================================================================
// Debug interface implementation
// ============================================================================
void D12VertexBuffer::SetDebugName(const std::string& name)
{
    // Teaching points: Call the base class implementation and set the DirectX object name
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

// ============================================================================
// Resource release
// ============================================================================
void D12VertexBuffer::ReleaseResource()
{
    // Teaching points: Clean up VertexBufferView
    m_view = {};

    // Call the base class implementation to release DirectX resources
    D12Buffer::ReleaseResource();
}
