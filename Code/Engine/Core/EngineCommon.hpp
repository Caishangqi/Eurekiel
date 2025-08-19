#pragma once

#include "Engine.hpp"
#include "Console/DevConsole.hpp"
#include "EventSystem.hpp"
#include "NamedStrings.hpp"
#include "ErrorWarningAssert.hpp"

// Forward declarations
namespace enigma::core {
    class ConsoleSubsystem;
}

extern NamedStrings                        g_gameConfigBlackboard;
extern EventSystem*                        g_theEventSystem;
extern DevConsole*                         g_theDevConsole;
extern enigma::core::Engine*               g_theEngine;
extern enigma::core::ConsoleSubsystem*     g_theConsole;

#define UNUSED(x) (void)(x);
#define STATIC static
#define POINTER_SAFE_DELETE(p) { if(p) { delete (p); (p) = nullptr; } }
