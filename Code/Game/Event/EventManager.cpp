#include "EventManager.hpp"

EventManager* EventManager::_instance = nullptr;

EventManager* EventManager::getInstance()
{
    if (_instance == nullptr)
    {
        _instance = new EventManager();
    }
    return _instance;
}

EventManager::EventManager()
{
    printf("[event]    EventManager Initialized\n");
}
