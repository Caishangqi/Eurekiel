#pragma once
#include <vector>

class Listener;
struct Event;
#define EVENT_DISPATCH(event) EventManager::GetInstance()->DispatchEvent(event)
#define EVENT_REGISTER(listener) EventManager::GetInstance()->RegisterListener(listener)

/**
 * The New Event Manager Class that inspire by Minecraft Spigot API
 * https://hub.spigotmc.org/javadocs/bukkit/org/bukkit/event/package-summary.html
 * without using any smart-pointers and function pointer related std
 *
 * Although still using template and typeid or else need write numerous repetitive code
 * I promise I know how template work and how typeid work, this Class have been
 * test and no memory leak and potential type unsafe operation
 */
class EventManager
{
protected:
    static EventManager* instance;

private:
    std::vector<Listener*> listeners;
    EventManager();
    EventManager(const EventManager&)            = delete;
    EventManager& operator=(const EventManager&) = delete;

public:
    ~EventManager();
    static EventManager* GetInstance();

    void RegisterListener(Listener* listener);

    void DispatchEvent(Event* event);
};
