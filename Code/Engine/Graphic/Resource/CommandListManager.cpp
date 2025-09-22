#include "Engine/Graphic/Resource/CommandListManager.hpp"
#include "Engine/Core/EngineCommon.hpp"
using namespace enigma::graphic;

#pragma region CommandListWrapper
CommandListManager::CommandListWrapper::CommandListWrapper()
{
}

CommandListManager::CommandListWrapper::~CommandListWrapper()
{
}
#pragma endregion


CommandListManager::CommandListManager()
{
}

CommandListManager::~CommandListManager()
{
}

bool CommandListManager::Initialize(uint32_t graphicsCount, uint32_t computeCount, uint32_t copyCount)
{
    UNUSED(graphicsCount)
    UNUSED(computeCount)
    UNUSED(copyCount)
    return false;
}

void CommandListManager::Shutdown()
{
}

bool CommandListManager::WaitForGPU(uint32_t timeoutMs)
{
    UNUSED(timeoutMs)
    return false;
}

uint32_t CommandListManager::GetAvailableCount(Type type) const
{
    UNUSED(type)
    return 0;
}
