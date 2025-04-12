#pragma once

#include "DevConsole.hpp"
#include "EventSystem.hpp"
#include "NamedStrings.hpp"
#include "ErrorWarningAssert.hpp"

extern NamedStrings g_gameConfigBlackboard;
extern EventSystem* g_theEventSystem;
extern DevConsole*  g_theDevConsole;

#define UNUSED(x) (void)(x);
#define STATIC static
#define POINTER_SAFE_DELETE(p) { if(p) { delete (p); (p) = nullptr; } }
