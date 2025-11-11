// 教学要点: Windows头文件预处理宏，必须在所有头文件之前定义
#define WIN32_LEAN_AND_MEAN  //  减少Windows.h包含内容，加速编译并避免冲突
#define NOMINMAX             //  防止Windows.h定义min/max宏，避免与std::min/max冲突

#include "UniformManager.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"

#include <cstring>

//  架构正确性: 使用D3D12RenderSystem静态API，遵循严格四层分层架构
// Layer 4 (UniformManager) → Layer 3 (D3D12RenderSystem) → Layer 2 (D12Buffer) → Layer 1 (DX12 Native)
#include "CustomImageIndexBuffer.hpp"
#include "MatricesUniforms.hpp"
#include "PerObjectUniforms.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
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
            return 0; // 单索引
        default:
            return 0;
        }
    }

    // ========================================================================
    // 构造函数 - RAII自动初始化
    // ========================================================================
    UniformManager::UniformManager()
    {
    }

    // ========================================================================
    // 析构函数 - RAII自动释放
    // ========================================================================

    UniformManager::~UniformManager()
    {
    }

    // ========================================================================
    // GetBufferStateBySlot() - 根据Root Slot查找Buffer状态 (Phase 1)
    // ========================================================================

    PerObjectBufferState* UniformManager::GetBufferStateBySlot(uint32_t rootSlot)
    {
        // Phase 1: 硬编码Slot到TypeId映射
        // 已实现: Slot 7 (MatricesUniforms), Slot 13 (PerObjectUniforms)
        if (rootSlot == 7)
        {
            // Slot 7 → MatricesUniforms
            std::type_index typeId = std::type_index(typeid(MatricesUniforms));
            auto            it     = m_perObjectBuffers.find(typeId);
            if (it != m_perObjectBuffers.end())
            {
                return &it->second;
            }
        }
        else if (rootSlot == 1)
        {
            // Slot 1 → PerObjectUniforms
            std::type_index typeId = std::type_index(typeid(PerObjectUniforms));
            auto            it     = m_perObjectBuffers.find(typeId);
            if (it != m_perObjectBuffers.end())
            {
                return &it->second;
            }
        }

        // Phase 2 TODO: 扩展到所有14个Slot
        // Slot 0: CameraAndPlayerUniforms
        // Slot 1: PerObjectUniforms  (已实现)
        // Slot 2: ScreenAndSystemUniforms
        // Slot 3: IDUniforms
        // Slot 4: WorldAndWeatherUniforms
        // Slot 5: BiomeAndDimensionUniforms
        // Slot 6: RenderingUniforms
        // Slot 7: MatricesUniforms (已实现)
        // Slot 8-12: 纹理Buffer (ColorTargets, DepthTextures等)


        return nullptr;
    }
} // namespace enigma::graphic
