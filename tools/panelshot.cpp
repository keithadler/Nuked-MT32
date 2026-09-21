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
 *  Headless front panel capture. Boots the machine from real ROMs, plays a
 *  chord so the display has something on it, then writes the panel as a PPM.
 *
 *  This exists so the picture in the README is generated from the same code
 *  the window draws, rather than being a mockup that drifts away from it.
 *
 *      mt32-panelshot CONTROL.ROM PCM.ROM out.ppm [button] [knob]
 *
 *  `button` holds one of the ten buttons down (0-9) while the panel is drawn,
 *  which is how the firmware is made to write its menu pages. `knob` is the
 *  volume position, 0-1023.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "mt32.h"
#include "rom.h"
#include "panel.h"

static mt32_t mt32;
static std::vector<uint32_t> buf(panel_w * panel_h);

static void run(int steps)
{
    for (int i = 0; i < steps; i++)
        mt32.clock(256);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        printf("usage: mt32-panelshot CONTROL.ROM PCM.ROM out.ppm [button 0-9] [knob 0-1023]\n");
        return 2;
    }
    const char *ctrl = argv[1], *pcm = argv[2], *out_path = argv[3];
    int button = argc > 4 ? atoi(argv[4]) : -1;
    int knob   = argc > 5 ? atoi(argv[5]) : 780;

    std::string err;
    if (!rom_load_control(ctrl, mt32.rom, mt32.old_machine, err)) {
        printf("control ROM: %s\n", err.c_str());
        return 1;
    }
    if (!rom_load(pcm, mt32.pcm, ROM_PCM_SIZE, "PCM", err)) {
        printf("PCM ROM: %s\n", err.c_str());
        return 1;
    }

    run(40000);   /* boot, until the panel settles on its idle page */

    /* A chord on part 1, so the part activity display has something to show. */
    const uint8_t notes[3] = { 60, 64, 67 };
    for (int n = 0; n < 3; n++) {
        mt32.post_midi(0x90);
        mt32.post_midi(notes[n]);
        mt32.post_midi(100);
        run(2000);
    }
    run(12000);

    if (button >= 0 && button <= 9) {
        mt32.button |= 1u << button;
        run(20000);   /* let the firmware redraw for the held key */
    }

    mt32.knob = knob;
    panel_render(mt32, buf.data());

    FILE *fp = fopen(out_path, "wb");
    if (!fp) {
        printf("cannot write %s\n", out_path);
        return 1;
    }
    fprintf(fp, "P6\n%d %d\n255\n", panel_w, panel_h);
    for (int i = 0; i < panel_w * panel_h; i++) {
        uint32_t c = buf[i];
        fputc(c & 0xff, fp);
        fputc((c >> 8) & 0xff, fp);
        fputc((c >> 16) & 0xff, fp);
    }
    fclose(fp);

    const uint8_t *t = mt32.lcd_text();
    printf("display reads: \"%.20s\"\n", t ? (const char *)t : "(none)");
    printf("wrote %s (%dx%d)\n", out_path, panel_w, panel_h);
    return 0;
}
