#pragma once

#include "Engine.hpp"
#include "Console/DevConsole.hpp"
#include "Event/EventSubsystem.hpp"
#include "Event/StringEventBus.hpp"
#include "NamedStrings.hpp"
#include "ErrorWarningAssert.hpp"

// Import EventArgs and EventCallbackFunction for global use
using enigma::event::EventArgs;
using enigma::event::EventCallbackFunction;

class Window;

namespace enigma::core
{
    class ImGuiSubsystem;
}

namespace enigma::graphic
{
    class RendererSubsystem;
}

namespace enigma::resource
{
    class ResourceSubsystem;
}

// Forward declarations
namespace enigma::core
{
    class ScheduleSubsystem;
    class ConsoleSubsystem;
}

namespace enigma::event
{
    class EventSubsystem;
}

extern NamedStrings                         g_gameConfigBlackboard;
extern DevConsole*                          g_theDevConsole;
extern Window*                              g_theWindow;
extern enigma::core::Engine*                g_theEngine;
extern enigma::core::ConsoleSubsystem*      g_theConsole;
extern enigma::resource::ResourceSubsystem* g_theResource;
extern enigma::core::ImGuiSubsystem*        g_theImGui;
extern enigma::core::ScheduleSubsystem*     g_theSchedule;
extern enigma::graphic::RendererSubsystem*  g_theRendererSubsystem;
extern enigma::core::LoggerSubsystem*       g_theLogger;
extern enigma::event::EventSubsystem*       g_theEventSubsystem;

// ============================================================================
// Legacy EventSystem Compatibility Macros
// These provide drop-in replacement for g_theEventSystem calls
// ============================================================================

// Helper to get StringEventBus from EventSubsystem
#define g_theStringEventBus (g_theEventSubsystem->GetStringBus())

// Legacy compatibility - Subscribe/Unsubscribe/Fire string events
#define SubscribeEventCallbackFunction(eventName, callback) \
    g_theEventSubsystem->SubscribeStringEvent(eventName, callback)

#define UnsubscribeEventCallbackFunction(eventName, callback) \
    g_theEventSubsystem->UnsubscribeStringEvent(eventName, callback)

#define FireEvent_Args(eventName, args) \
    g_theEventSubsystem->FireStringEvent(eventName, args)

#define FireEvent_NoArgs(eventName) \
    g_theEventSubsystem->FireStringEvent(eventName)

// Generic FireEvent macro - auto-selects based on argument count
// Usage: FireEvent("eventName") or FireEvent("eventName", args)
#define FIRE_EVENT_GET_MACRO(_1, _2, NAME, ...) NAME
#define FireEvent(...) FIRE_EVENT_GET_MACRO(__VA_ARGS__, FireEvent_Args, FireEvent_NoArgs)(__VA_ARGS__)

#define UNUSED(x) (void)(x);
#define STATIC static
#define POINTER_SAFE_DELETE(p) { if(p) { delete (p); (p) = nullptr; } }
