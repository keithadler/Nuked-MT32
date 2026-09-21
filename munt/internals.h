/* Transcribed from Munt's mt32emu/src/internals.h - see munt/README.md. */
#pragma once
#include "Types.h"

// 0: maximum speed, slightly lower accuracy. 1: maximum accuracy.
#ifndef MT32EMU_BOSS_REVERB_PRECISE_MODE
#define MT32EMU_BOSS_REVERB_PRECISE_MODE 0
#endif

namespace MT32Emu {

typedef Bit16s IntSample;
typedef Bit32s IntSampleEx;
typedef float  FloatSample;

enum ReverbMode {
    REVERB_MODE_ROOM,
    REVERB_MODE_HALL,
    REVERB_MODE_PLATE,
    REVERB_MODE_TAP_DELAY
};

}
