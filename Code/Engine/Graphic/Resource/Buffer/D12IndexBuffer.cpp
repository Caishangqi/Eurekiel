#include "D12IndexBuffer.hpp"
#include <cassert>
#include <sstream>

using namespace enigma::graphic;

// ============================================================================
// 构造函数和析构函数
// ============================================================================

D12IndexBuffer::D12IndexBuffer(size_t size, IndexFormat format, const void* initialData, const char* debugName)
    : D12Buffer([&]()
      {
          BufferCreateInfo info;
          info.size  = size;
          info.usage = BufferUsage::IndexBuffer;
          // 教学要点: 应用层IndexBuffer需要频繁更新，统一使用CPUWritable
          // 即使没有initialData，也可能在后续通过UpdateBuffer更新
          // 参考Iris: 动态索引数据使用UPLOAD heap (D3D12_HEAP_TYPE_UPLOAD)
          info.memoryAccess = MemoryAccess::CPUWritable;
          info.initialData  = initialData;
          info.debugName    = debugName;
          return info;
      }())
      , m_format(format)
      , m_view{}
{
    // 教学要点: 参数验证 - 确保size是索引大小的整数倍
    const size_t indexSize = (format == IndexFormat::Uint16) ? 2 : 4;
    assert(size % indexSize == 0 && "Buffer size must be multiple of index size");
    UNUSED(indexSize)
    // 教学要点: 创建D3D12_INDEX_BUFFER_VIEW
    UpdateView();
}

// ============================================================================
// IndexBufferView管理
// ============================================================================

void D12IndexBuffer::UpdateView()
{
    auto* resource = GetResource();
    if (!resource)
    {
        m_view = {};
        return;
    }

    // 教学要点: 构造D3D12_INDEX_BUFFER_VIEW
    // 1. BufferLocation: GPU虚拟地址
    // 2. SizeInBytes: 缓冲区总大小
    // 3. Format: 索引格式（R16_UINT或R32_UINT）
    m_view.BufferLocation = resource->GetGPUVirtualAddress();
    m_view.SizeInBytes    = static_cast<UINT>(GetSize());
    m_view.Format         = IndexFormatToDXGI(m_format);
}

// ============================================================================
// Debug接口实现
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
// 资源释放
// ============================================================================

void D12IndexBuffer::ReleaseResource()
{
    m_view = {};
    D12Buffer::ReleaseResource();
}
