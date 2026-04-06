#pragma once

// Engine-owned build preferences.
// Projects may override these macros through compiler definitions.

//#define ENGINE_DISABLE_AUDIO

#if defined(_DEBUG) && !defined(ENGINE_DEBUG_RENDER)
#define ENGINE_DEBUG_RENDER
#endif
