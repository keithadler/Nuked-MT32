/*
 * Copyright (C) 2024, 2025 nukeykt
 *
 * This file is part of Nuked-MT32.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  Optional DC blocker, modelling the AC coupling on the real unit's output.
 *
 *  This core emulates the digital path up to the DAC, and that signal carries
 *  a patch-dependent DC offset - measured at -478 on patch 0 and +2007 on
 *  patch 81, the latter about 6% of full scale. On real hardware the output
 *  is AC coupled, so no DC reaches the jacks. It costs headroom and makes
 *  clipping asymmetric, since negative peaks reach the rail first.
 *
 *  This is a MITIGATION, not a fix. It does not address whatever produces the
 *  offset inside the LA32 - see FINDINGS.md. It is off by default so the raw
 *  emulated signal stays available unchanged.
 */
#pragma once
#include <stdint.h>

class DcBlocker {
public:
    // One-pole high pass: y[n] = x[n] - x[n-1] + R * y[n-1].
    // R = 0.9995 at 32 kHz puts the corner near 2.5 Hz, well below the
    // lowest note the MT-32 produces.
    void process(int16_t *frames, int count)
    {
        for (int i = 0; i < count; i++) {
            for (int ch = 0; ch < 2; ch++) {
                double x = frames[i * 2 + ch];
                double y = x - prev_x[ch] + kR * prev_y[ch];
                prev_x[ch] = x;
                prev_y[ch] = y;
                int32_t v = int32_t(y < 0 ? y - 0.5 : y + 0.5);
                if (v < -32768) v = -32768;
                if (v >  32767) v =  32767;
                frames[i * 2 + ch] = int16_t(v);
            }
        }
    }

    void reset() { prev_x[0] = prev_x[1] = prev_y[0] = prev_y[1] = 0.0; }

private:
    static constexpr double kR = 0.9995;
    double prev_x[2] = {0.0, 0.0};
    double prev_y[2] = {0.0, 0.0};
};
