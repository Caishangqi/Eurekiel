#pragma once

#include "Engine.hpp"
#include "Console/DevConsole.hpp"
#include "EventSystem.hpp"
#include "NamedStrings.hpp"
#include "ErrorWarningAssert.hpp"

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

extern NamedStrings                         g_gameConfigBlackboard;
extern EventSystem*                         g_theEventSystem;
extern DevConsole*                          g_theDevConsole;
extern enigma::core::Engine*                g_theEngine;
extern enigma::core::ConsoleSubsystem*      g_theConsole;
extern enigma::resource::ResourceSubsystem* g_theResource;
extern enigma::core::ScheduleSubsystem*     g_theSchedule;
extern enigma::graphic::RendererSubsystem*  g_theRendererSubsystem;

#define UNUSED(x) (void)(x);
#define STATIC static
#define POINTER_SAFE_DELETE(p) { if(p) { delete (p); (p) = nullptr; } }
