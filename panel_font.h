/*
 * Copyright (C) 2026 Keith Adler
 *
 * This file is part of Nuked-MT32.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *  A stroke font for the faceplate lettering.
 *
 *  The panel used to be labelled in the LCD's own 5x7 character font, which
 *  made the front of the unit read like a debug overlay: the same pixel shapes
 *  on the case as on the display. Silkscreen on real hardware is drawn, not
 *  dot matrix, so this is a set of line segments per character. It scales
 *  cleanly, it costs a few hundred bytes, and it does not look like the
 *  display it sits next to.
 *
 *  Each glyph is a run of segments on a 6 wide by 10 tall grid, origin at the
 *  top left, terminated by 0xff. Coordinates are packed one per nibble.
 */
#pragma once
#include <stdint.h>

/* x0 y0 x1 y1 packed as two bytes: (x0<<4|y0), (x1<<4|y1) */
#define S(x0, y0, x1, y1) (uint8_t)(((x0) << 4) | (y0)), (uint8_t)(((x1) << 4) | (y1))
#define SEND 0xff

static const uint8_t panel_glyph_A[] = { S(0,9,3,0), S(3,0,6,9), S(1,6,5,6), SEND };
static const uint8_t panel_glyph_B[] = { S(0,0,0,9), S(0,0,4,0), S(4,0,5,1), S(5,1,5,3),
                                         S(5,3,4,4), S(4,4,0,4), S(4,4,5,5), S(5,5,5,8),
                                         S(5,8,4,9), S(4,9,0,9), SEND };
static const uint8_t panel_glyph_C[] = { S(6,2,5,0), S(5,0,1,0), S(1,0,0,2), S(0,2,0,7),
                                         S(0,7,1,9), S(1,9,5,9), S(5,9,6,7), SEND };
static const uint8_t panel_glyph_D[] = { S(0,0,0,9), S(0,0,4,0), S(4,0,6,2), S(6,2,6,7),
                                         S(6,7,4,9), S(4,9,0,9), SEND };
static const uint8_t panel_glyph_E[] = { S(6,0,0,0), S(0,0,0,9), S(0,9,6,9), S(0,4,4,4), SEND };
static const uint8_t panel_glyph_F[] = { S(6,0,0,0), S(0,0,0,9), S(0,4,4,4), SEND };
/* No diagonal closing the top right: at the sizes used on the panel that
 * corner fills in and the letter reads as a B. The opening is what makes it
 * a G, so it is left wide. */
static const uint8_t panel_glyph_G[] = { S(5,0,1,0), S(1,0,0,2), S(0,2,0,7),
                                         S(0,7,1,9), S(1,9,5,9), S(5,9,6,6),
                                         S(6,6,3,6), SEND };
static const uint8_t panel_glyph_H[] = { S(0,0,0,9), S(6,0,6,9), S(0,4,6,4), SEND };
static const uint8_t panel_glyph_I[] = { S(1,0,5,0), S(3,0,3,9), S(1,9,5,9), SEND };
static const uint8_t panel_glyph_J[] = { S(4,0,4,7), S(4,7,3,9), S(3,9,1,9), S(1,9,0,7), SEND };
static const uint8_t panel_glyph_K[] = { S(0,0,0,9), S(6,0,0,5), S(2,3,6,9), SEND };
static const uint8_t panel_glyph_L[] = { S(0,0,0,9), S(0,9,6,9), SEND };
static const uint8_t panel_glyph_M[] = { S(0,9,0,0), S(0,0,3,4), S(3,4,6,0), S(6,0,6,9), SEND };
static const uint8_t panel_glyph_N[] = { S(0,9,0,0), S(0,0,6,9), S(6,9,6,0), SEND };
static const uint8_t panel_glyph_O[] = { S(1,0,5,0), S(5,0,6,2), S(6,2,6,7), S(6,7,5,9),
                                         S(5,9,1,9), S(1,9,0,7), S(0,7,0,2), S(0,2,1,0), SEND };
static const uint8_t panel_glyph_P[] = { S(0,9,0,0), S(0,0,5,0), S(5,0,6,1), S(6,1,6,4),
                                         S(6,4,5,5), S(5,5,0,5), SEND };
static const uint8_t panel_glyph_Q[] = { S(1,0,5,0), S(5,0,6,2), S(6,2,6,7), S(6,7,5,9),
                                         S(5,9,1,9), S(1,9,0,7), S(0,7,0,2), S(0,2,1,0),
                                         S(4,7,6,9), SEND };
static const uint8_t panel_glyph_R[] = { S(0,9,0,0), S(0,0,5,0), S(5,0,6,1), S(6,1,6,4),
                                         S(6,4,5,5), S(5,5,0,5), S(2,5,6,9), SEND };
static const uint8_t panel_glyph_S[] = { S(6,1,5,0), S(5,0,1,0), S(1,0,0,2), S(0,2,1,4),
                                         S(1,4,5,4), S(5,4,6,6), S(6,6,5,9), S(5,9,1,9),
                                         S(1,9,0,8), SEND };
static const uint8_t panel_glyph_T[] = { S(0,0,6,0), S(3,0,3,9), SEND };
static const uint8_t panel_glyph_U[] = { S(0,0,0,7), S(0,7,2,9), S(2,9,4,9), S(4,9,6,7),
                                         S(6,7,6,0), SEND };
static const uint8_t panel_glyph_V[] = { S(0,0,3,9), S(3,9,6,0), SEND };
static const uint8_t panel_glyph_W[] = { S(0,0,1,9), S(1,9,3,4), S(3,4,5,9), S(5,9,6,0), SEND };
static const uint8_t panel_glyph_X[] = { S(0,0,6,9), S(6,0,0,9), SEND };
static const uint8_t panel_glyph_Y[] = { S(0,0,3,4), S(6,0,3,4), S(3,4,3,9), SEND };
static const uint8_t panel_glyph_Z[] = { S(0,0,6,0), S(6,0,0,9), S(0,9,6,9), SEND };

static const uint8_t panel_glyph_0[] = { S(1,0,5,0), S(5,0,6,2), S(6,2,6,7), S(6,7,5,9),
                                         S(5,9,1,9), S(1,9,0,7), S(0,7,0,2), S(0,2,1,0),
                                         S(1,7,5,2), SEND };
static const uint8_t panel_glyph_1[] = { S(1,2,3,0), S(3,0,3,9), S(1,9,5,9), SEND };
static const uint8_t panel_glyph_2[] = { S(0,2,1,0), S(1,0,5,0), S(5,0,6,2), S(6,2,0,9),
                                         S(0,9,6,9), SEND };
static const uint8_t panel_glyph_3[] = { S(0,0,6,0), S(6,0,3,4), S(3,4,5,4), S(5,4,6,6),
                                         S(6,6,5,9), S(5,9,1,9), S(1,9,0,8), SEND };
static const uint8_t panel_glyph_4[] = { S(5,9,5,0), S(5,0,0,6), S(0,6,6,6), SEND };
static const uint8_t panel_glyph_5[] = { S(6,0,0,0), S(0,0,0,4), S(0,4,4,4), S(4,4,6,6),
                                         S(6,6,5,9), S(5,9,1,9), S(1,9,0,8), SEND };
static const uint8_t panel_glyph_6[] = { S(5,0,1,0), S(1,0,0,3), S(0,3,0,7), S(0,7,1,9),
                                         S(1,9,5,9), S(5,9,6,7), S(6,7,5,5), S(5,5,1,5),
                                         S(1,5,0,6), SEND };
static const uint8_t panel_glyph_7[] = { S(0,0,6,0), S(6,0,2,9), SEND };
static const uint8_t panel_glyph_8[] = { S(1,0,5,0), S(5,0,6,2), S(6,2,5,4), S(5,4,1,4),
                                         S(1,4,0,2), S(0,2,1,0), S(1,4,0,6), S(0,6,0,7),
                                         S(0,7,1,9), S(1,9,5,9), S(5,9,6,7), S(6,7,6,6),
                                         S(6,6,5,4), SEND };
static const uint8_t panel_glyph_9[] = { S(1,9,5,9), S(5,9,6,6), S(6,6,6,2), S(6,2,5,0),
                                         S(5,0,1,0), S(1,0,0,2), S(0,2,1,4), S(1,4,5,4),
                                         S(5,4,6,3), SEND };

static const uint8_t panel_glyph_dash[]  = { S(1,5,5,5), SEND };
static const uint8_t panel_glyph_equal[] = { S(1,3,5,3), S(1,6,5,6), SEND };
static const uint8_t panel_glyph_dot[]   = { S(3,9,3,9), SEND };
static const uint8_t panel_glyph_slash[] = { S(6,0,0,9), SEND };
static const uint8_t panel_glyph_none[]  = { SEND };

#undef S

/* the segments for one character, or an empty run */
static inline const uint8_t *panel_glyph(char c)
{
    switch (c) {
      case 'A': return panel_glyph_A;  case 'B': return panel_glyph_B;
      case 'C': return panel_glyph_C;  case 'D': return panel_glyph_D;
      case 'E': return panel_glyph_E;  case 'F': return panel_glyph_F;
      case 'G': return panel_glyph_G;  case 'H': return panel_glyph_H;
      case 'I': return panel_glyph_I;  case 'J': return panel_glyph_J;
      case 'K': return panel_glyph_K;  case 'L': return panel_glyph_L;
      case 'M': return panel_glyph_M;  case 'N': return panel_glyph_N;
      case 'O': return panel_glyph_O;  case 'P': return panel_glyph_P;
      case 'Q': return panel_glyph_Q;  case 'R': return panel_glyph_R;
      case 'S': return panel_glyph_S;  case 'T': return panel_glyph_T;
      case 'U': return panel_glyph_U;  case 'V': return panel_glyph_V;
      case 'W': return panel_glyph_W;  case 'X': return panel_glyph_X;
      case 'Y': return panel_glyph_Y;  case 'Z': return panel_glyph_Z;
      case '0': return panel_glyph_0;  case '1': return panel_glyph_1;
      case '2': return panel_glyph_2;  case '3': return panel_glyph_3;
      case '4': return panel_glyph_4;  case '5': return panel_glyph_5;
      case '6': return panel_glyph_6;  case '7': return panel_glyph_7;
      case '8': return panel_glyph_8;  case '9': return panel_glyph_9;
      case '-': return panel_glyph_dash;
      case '=': return panel_glyph_equal;
      case '.': return panel_glyph_dot;
      case '/': return panel_glyph_slash;
      default:  return panel_glyph_none;
    }
}
