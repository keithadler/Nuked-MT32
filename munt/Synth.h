/* Minimal stand-in for Munt's Synth.h.
 *
 * BReverbModel.cpp uses only these three static helpers. They are transcribed
 * verbatim from Munt's Synth.h so the saturation and muting behaviour is
 * bit-identical. See munt/README.md.
 */
#pragma once
#include <string.h>
#include "Types.h"

namespace MT32Emu {

class Synth {
public:
    static inline Bit16s clipSampleEx(Bit32s sampleEx) {
        return ((-0x8000 <= sampleEx) && (sampleEx <= 0x7FFF))
            ? Bit16s(sampleEx) : Bit16s((sampleEx >> 31) ^ 0x7FFF);
    }

    static inline float clipSampleEx(float sampleEx) {
        return sampleEx;
    }

    template <class S>
    static inline void muteSampleBuffer(S *buffer, Bit32u len) {
        if (buffer == NULL) return;
        memset(buffer, 0, len * sizeof(S));
    }

    static inline void muteSampleBuffer(float *buffer, Bit32u len) {
        if (buffer == NULL) return;
        while (len--) *(buffer++) = 0.0f;
    }
};

}
