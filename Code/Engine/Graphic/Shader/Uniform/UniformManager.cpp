// Windows头文件预处理宏
#define WIN32_LEAN_AND_MEAN  // 减少Windows.h包含内容
#define NOMINMAX             // 防止Windows.h定义min/max宏

#include "UniformManager.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"

#include <cstring>

// 遵循四层架构: UniformManager → D3D12RenderSystem → D12Buffer → DX12 Native
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"

namespace enigma::graphic
{
    void* PerObjectBufferState::GetDataAt(size_t index)
    {
        return static_cast<uint8_t*>(mappedData) + index * elementSize;
    }

    size_t PerObjectBufferState::GetCurrentIndex() const
    {
        switch (frequency)
        {
        case UpdateFrequency::PerObject:
            return currentIndex % maxCount;
        case UpdateFrequency::PerPass:
        case UpdateFrequency::PerFrame:
            return 0; // single index
        default:
            return 0;
        }
    }

    UniformManager::UniformManager()
    {
    }

    UniformManager::~UniformManager()
    {
    }

    bool UniformManager::IsSlotRegistered(uint32_t slot) const
    {
        return m_slotToTypeMap.find(slot) != m_slotToTypeMap.end();
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

    PerObjectBufferState* UniformManager::GetBufferStateBySlot(uint32_t rootSlot)
    {
        // 查找slot是否已注册
        auto slotIt = m_slotToTypeMap.find(rootSlot);
        if (slotIt == m_slotToTypeMap.end())
        {
            return nullptr; // Slot未注册，静默返回
        }

        // 获取TypeId并查找BufferState
        std::type_index typeId   = slotIt->second;
        auto            bufferIt = m_perObjectBuffers.find(typeId);
        if (bufferIt != m_perObjectBuffers.end())
        {
            return &bufferIt->second;
        }

        // TypeId已注册但Buffer未创建（异常情况）
        LogWarn("UniformManager", "Slot %u registered but buffer not created for TypeId: %s", rootSlot, typeId.name());
        ERROR_RECOVERABLE(Stringf("Slot %u registered but buffer not created for TypeId: %s", rootSlot, typeId.name()));
        return nullptr;
    }
} // namespace enigma::graphic
