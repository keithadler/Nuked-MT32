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
 *  Front panel: draws a hardware-style face around the emulated LCD, with
 *  clickable buttons and a volume knob.
 *
 *  The layout follows the shape of the real unit - display left of centre,
 *  a row of function buttons beneath it, volume on the right - but uses
 *  plain functional labels and carries no manufacturer branding or logo.
 */
#pragma once
#include <stdint.h>

class mt32_t;

static constexpr int panel_w = 1240;
static constexpr int panel_h = 340;

// Renders the whole panel, including the LCD, into `out` (panel_w * panel_h,
// 0xAABBGGRR byte order matching lcd_buffer).
void panel_render(mt32_t &mt32, uint32_t *out);

// Hit testing. Returns the button index 0-9 for a press, or -1.
int panel_hit_button(int x, int y);

// True if (x, y) is inside the volume knob.
bool panel_hit_knob(int x, int y);
