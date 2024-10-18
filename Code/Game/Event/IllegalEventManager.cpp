#include "IllegalEventManager.hpp"

IllegalEventManager* IllegalEventManager::_instance = nullptr;

IllegalEventManager* IllegalEventManager::getInstance()
{
    if (_instance == nullptr)
    {
        _instance = new IllegalEventManager();
    }
    return _instance;
}

IllegalEventManager::IllegalEventManager()
{
    printf("[event]    IllegalEventManager Initialized\n");
}
