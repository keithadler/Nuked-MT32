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
 *  Fallback MIDI backend for platforms with no supported MIDI API.
 *  The emulator still runs; it just has no live input.
 */
#include <stdio.h>
#include "midi.h"

int MIDI_Init(int port)
{
    (void)port;
    printf("  MIDI in: none (built without a MIDI backend)\n");
    return 0;
}

void MIDI_Quit(void)
{
}
