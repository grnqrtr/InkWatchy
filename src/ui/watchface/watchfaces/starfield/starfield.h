#pragma once

#include "defines.h"

#if WATCHFACE_STARFIELD || WATCHFACE_STARFIELD_BLOCKWATCH

// Forward declaration
struct watchfaceDefOne;

#if WATCHFACE_STARFIELD
extern const watchfaceDefOne starfieldDef;
#endif

#if WATCHFACE_STARFIELD_BLOCKWATCH
extern const watchfaceDefOne starfieldBlockwatchDef;
#endif

#endif