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
 */
#include "panel.h"
#include "mt32.h"
#include <string.h>
#include <math.h>
#include "panel_font.h"

// The LCD controller's own font, used only for the tiny key hints under the
// buttons where a dot-matrix look is honest. Defined in lcd_font.h.
extern uint8_t lcd_font[160][8];

namespace {

// Byte order matches lcd_buffer: low byte is red.
constexpr uint32_t rgb(int r, int g, int b)
{
    return 0xff000000u | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
}

const uint32_t COL_CASE_TOP   = rgb(0x2a, 0x2a, 0x2c);   // matte black, lit from above
const uint32_t COL_CASE_BOT   = rgb(0x18, 0x18, 0x1a);
const uint32_t COL_CASE_EDGE  = rgb(0x08, 0x08, 0x09);
const uint32_t COL_PLATE      = rgb(0x1e, 0x1e, 0x21);   // the front plate on the case
const uint32_t COL_PLATE_LIP  = rgb(0x3e, 0x3e, 0x43);
const uint32_t COL_BEZEL      = rgb(0x0c, 0x0d, 0x0c);
const uint32_t COL_BEZEL_LIP  = rgb(0x33, 0x35, 0x33);
const uint32_t COL_GLOW       = rgb(0x14, 0x2a, 0x24);
const uint32_t COL_BTN        = rgb(0x30, 0x30, 0x34);   // raised, slightly proud
const uint32_t COL_BTN_TOP    = rgb(0x4a, 0x4a, 0x50);
const uint32_t COL_BTN_LIT    = rgb(0x7e, 0x86, 0x90);
const uint32_t COL_BTN_EDGE   = rgb(0x0e, 0x0e, 0x10);
const uint32_t COL_SILK       = rgb(0xc6, 0xc6, 0xcb);   // light grey lettering
const uint32_t COL_SILK_DIM   = rgb(0x8c, 0x8c, 0x92);
const uint32_t COL_LABEL      = rgb(0x7c, 0xb4, 0xdc);   // the pale blue button legends
const uint32_t COL_KNOB       = rgb(0x1d, 0x1d, 0x20);
const uint32_t COL_KNOB_RIM   = rgb(0x3a, 0x3a, 0x3f);
const uint32_t COL_KNOB_EDGE  = rgb(0x0a, 0x0a, 0x0c);
const uint32_t COL_KNOB_MARK  = rgb(0xe8, 0xe8, 0xec);   // a plain white pointer
const uint32_t COL_LED_ON     = rgb(0x6c, 0xf4, 0x8a);
const uint32_t COL_LED_OFF    = rgb(0x1e, 0x30, 0x22);

// --- Geometry ---------------------------------------------------------------

// The display is drawn at half its native size. On the hardware it is a small
// window in a wide face, and blitting 840x100 one for one made it the whole
// panel. Two native pixels to one is still 21 pixels a character, which reads
// perfectly well, and it puts the proportions back.
constexpr int LCD_SCALE = 2;
constexpr int LCD_X = 54, LCD_Y = 116;
constexpr int LCD_DW = lcd_w / LCD_SCALE, LCD_DH = lcd_h / LCD_SCALE;
constexpr int BEZEL = 10;

// The front plate, a shade off the case, running the width of the face.
constexpr int PLT_X = 18, PLT_Y = 20, PLT_W = 1444, PLT_H = 240;

// Ten buttons in two rows of five, which is how the hardware arranges them and
// what the order in kButtons has always described: 1 2 3 GROUP VOLUME on top,
// 4 5 RHYTHM SOUND MASTER beneath.
constexpr int BTN_W = 66, BTN_H = 46, BTN_GAP = 12, BTN_ROW_GAP = 30;
constexpr int BTN_X0 = 724, BTN_Y0 = 82;

constexpr int KNOB_CX = 1330, KNOB_CY = 140, KNOB_R = 48;

// Drawing and hit testing both come through here. A row or a gap changed in
// one and not the other is a button that looks right and clicks wrong, which
// is exactly what happened the first time this panel was rearranged.
constexpr int button_x(int i) { return BTN_X0 + (i % 5) * (BTN_W + BTN_GAP); }
constexpr int button_y(int i) { return BTN_Y0 + (i / 5) * (BTN_H + BTN_ROW_GAP); }

// A legend can be wider than the key it labels, as it is on the hardware, so
// the long ones are set on two lines rather than run into their neighbours.
struct ButtonDef { const char *label; const char *label2; const char *key; };

// Index is the bit in mt32.button, which matches the MT32_BUTTTON_* enum.
// Keys 1-0 map to bits 0-9, same as the keyboard handling in main.cpp.
const ButtonDef kButtons[10] = {
    {"1",      nullptr,  "1"},
    {"2",      nullptr,  "2"},
    {"3",      nullptr,  "3"},
    {"SOUND",  "GROUP",  "4"},
    {"VOLUME", nullptr,  "5"},
    {"4",      nullptr,  "6"},
    {"5",      nullptr,  "7"},
    {"RHYTHM", nullptr,  "8"},
    {"SOUND",  nullptr,  "9"},
    {"MASTER", "VOLUME", "0"},
};

inline void px(uint32_t *b, int x, int y, uint32_t c)
{
    if (x < 0 || y < 0 || x >= panel_w || y >= panel_h) return;
    b[y * panel_w + x] = c;
}

void fill_rect(uint32_t *b, int x, int y, int w, int h, uint32_t c)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            px(b, x + i, y + j, c);
}

// Rounded rectangle with a 1px darker border.
void round_rect(uint32_t *b, int x, int y, int w, int h, int r,
                uint32_t fill, uint32_t edge)
{
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int dx = 0, dy = 0;
            if (i < r)          dx = r - i;
            else if (i >= w - r) dx = i - (w - r - 1);
            if (j < r)          dy = r - j;
            else if (j >= h - r) dy = j - (h - r - 1);
            if (dx && dy) {
                int d2 = dx * dx + dy * dy;
                if (d2 > r * r) continue;
                if (d2 > (r - 1) * (r - 1)) { px(b, x + i, y + j, edge); continue; }
            }
            bool border = (i == 0 || j == 0 || i == w - 1 || j == h - 1);
            px(b, x + i, y + j, border ? edge : fill);
        }
    }
}

void disc(uint32_t *b, int cx, int cy, int r, uint32_t fill, uint32_t edge)
{
    for (int j = -r; j <= r; j++) {
        for (int i = -r; i <= r; i++) {
            int d2 = i * i + j * j;
            if (d2 > r * r) continue;
            px(b, cx + i, cy + j, d2 > (r - 2) * (r - 2) ? edge : fill);
        }
    }
}

void line(uint32_t *b, int x0, int y0, int x1, int y1, int thick, uint32_t c)
{
    int steps = abs(x1 - x0) > abs(y1 - y0) ? abs(x1 - x0) : abs(y1 - y0);
    if (steps <= 0) steps = 1;
    for (int s = 0; s <= steps; s++) {
        int x = x0 + (x1 - x0) * s / steps;
        int y = y0 + (y1 - y0) * s / steps;
        for (int j = -thick; j <= thick; j++)
            for (int i = -thick; i <= thick; i++)
                px(b, x + i, y + j, c);
    }
}

// Draws text with the LCD font. Rows 0-6 are the glyph; row 7 is the
// controller's underline marker and is skipped, matching lcd_font_render().
int text_width(const char *s, int scale) { return int(strlen(s)) * 6 * scale; }

void draw_text(uint32_t *b, int x, int y, int scale, const char *s, uint32_t c)
{
    for (int n = 0; s[n]; n++) {
        unsigned char ch = (unsigned char)s[n];
        const uint8_t *g = (ch >= 0x20 && ch < 0x80) ? lcd_font[ch - 0x20] : lcd_font[0];
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                if (!(g[row] & (1 << (4 - col)))) continue;
                for (int j = 0; j < scale; j++)
                    for (int i = 0; i < scale; i++)
                        px(b, x + n * 6 * scale + col * scale + i, y + row * scale + j, c);
            }
        }
    }
}

void draw_text_centred(uint32_t *b, int cx, int y, int scale, const char *s, uint32_t c)
{
    draw_text(b, cx - text_width(s, scale) / 2, y, scale, s, c);
}

// --- Silkscreen lettering ---------------------------------------------------
//
// Drawn with coverage rather than with whole pixels. A stroke is a run of
// segments; for every pixel near one, the distance from the pixel centre to
// the segment gives how much of it the stroke covers, and that is the blend.
// Without this the diagonals stair-step and the lettering looks like a
// screenshot of a plotter, which is what it looked like.

inline uint32_t blend(uint32_t dst, uint32_t src, float a)
{
    if (a <= 0.0f) return dst;
    if (a >= 1.0f) return src;
    int dr = dst & 0xff, dg = (dst >> 8) & 0xff, db = (dst >> 16) & 0xff;
    int sr = src & 0xff, sg = (src >> 8) & 0xff, sb = (src >> 16) & 0xff;
    return rgb(int(dr + (sr - dr) * a + 0.5f),
               int(dg + (sg - dg) * a + 0.5f),
               int(db + (sb - db) * a + 0.5f));
}

// Distance from a point to a segment, which is what the coverage is built on.
inline float seg_dist(float px_, float py_, float x0, float y0, float x1, float y1)
{
    float vx = x1 - x0, vy = y1 - y0;
    float wx = px_ - x0, wy = py_ - y0;
    float len2 = vx * vx + vy * vy;
    float t = len2 > 0.0f ? (wx * vx + wy * vy) / len2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float dx = wx - vx * t, dy = wy - vy * t;
    return sqrtf(dx * dx + dy * dy);
}

void line_aa(uint32_t *b, float x0, float y0, float x1, float y1,
             float half, uint32_t c)
{
    int lo_x = int(floorf(fminf(x0, x1) - half - 1.0f));
    int hi_x = int(ceilf (fmaxf(x0, x1) + half + 1.0f));
    int lo_y = int(floorf(fminf(y0, y1) - half - 1.0f));
    int hi_y = int(ceilf (fmaxf(y0, y1) + half + 1.0f));
    if (lo_x < 0) lo_x = 0;
    if (lo_y < 0) lo_y = 0;
    if (hi_x > panel_w - 1) hi_x = panel_w - 1;
    if (hi_y > panel_h - 1) hi_y = panel_h - 1;

    for (int y = lo_y; y <= hi_y; y++)
        for (int x = lo_x; x <= hi_x; x++) {
            float d = seg_dist(x + 0.5f, y + 0.5f, x0, y0, x1, y1);
            float a = half + 0.5f - d;
            if (a <= 0.0f) continue;
            if (a > 1.0f) a = 1.0f;
            b[y * panel_w + x] = blend(b[y * panel_w + x], c, a);
        }
}

//
// Drawn from the stroke font, so the labels on the case do not share the
// display's pixel grid. `scale` is in tenths: 10 gives a capital ten pixels
// tall. `weight` thickens the stroke for the larger sizes.

int silk_width(const char *s, int scale, int tracking)
{
    int n = 0;
    for (const char *p = s; *p; p++) n++;
    if (!n) return 0;
    return n * (6 * scale / 10 + tracking) - tracking;
}

void silk(uint32_t *b, int x, int y, int scale, int weight, const char *str, uint32_t c)
{
    const int tracking = scale / 4 + 2;
    const int adv = 6 * scale / 10 + tracking;
    // Strokes get heavier with the lettering, but not in whole pixels: a
    // caption and a title want the same proportions, not the same nib.
    const float half = 0.35f + 0.030f * scale * float(weight);

    for (const char *p = str; *p; p++, x += adv) {
        if (*p == ' ') continue;
        const uint8_t *g = panel_glyph(*p);
        for (; *g != SEND; g += 2) {
            float x0 = x + (g[0] >> 4)     * scale / 10.0f;
            float y0 = y + (g[0] & 0x0f)   * scale / 10.0f;
            float x1 = x + (g[1] >> 4)     * scale / 10.0f;
            float y1 = y + (g[1] & 0x0f)   * scale / 10.0f;
            line_aa(b, x0, y0, x1, y1, half, c);
        }
    }
}

void silk_centred(uint32_t *b, int cx, int y, int scale, int weight,
                  const char *str, uint32_t c)
{
    silk(b, cx - silk_width(str, scale, scale / 4 + 2) / 2, y, scale, weight, str, c);
}

} // namespace

int panel_hit_button(int x, int y)
{
    for (int i = 0; i < 10; i++) {
        int bx = button_x(i), by = button_y(i);
        if (x >= bx && x < bx + BTN_W && y >= by && y < by + BTN_H) return i;
    }
    return -1;
}

bool panel_hit_knob(int x, int y)
{
    int dx = x - KNOB_CX, dy = y - KNOB_CY;
    return dx * dx + dy * dy <= KNOB_R * KNOB_R;
}

void panel_render(mt32_t &mt32, uint32_t *out)
{
    // --- Case --------------------------------------------------------------
    // Matte black with a little grain, lit from above. The unit this stands in
    // for is a black box, not a beige one.
    for (int y = 0; y < panel_h; y++) {
        float t = float(y) / float(panel_h - 1);
        int r = int((1 - t) * (COL_CASE_TOP & 0xff)         + t * (COL_CASE_BOT & 0xff));
        int g = int((1 - t) * ((COL_CASE_TOP >> 8) & 0xff)  + t * ((COL_CASE_BOT >> 8) & 0xff));
        int b = int((1 - t) * ((COL_CASE_TOP >> 16) & 0xff) + t * ((COL_CASE_BOT >> 16) & 0xff));
        for (int x = 0; x < panel_w; x++) {
            unsigned h = unsigned(x * 1973 + y * 6151);
            h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
            int n = int(h & 3) - 1;
            out[y * panel_w + x] = rgb(r + n, g + n, b + n);
        }
    }
    fill_rect(out, 0, 0, panel_w, 2, rgb(0x46, 0x46, 0x4a));
    fill_rect(out, 0, panel_h - 4, panel_w, 4, COL_CASE_EDGE);

    // The front plate, a shade off the case and very slightly proud of it.
    round_rect(out, PLT_X, PLT_Y, PLT_W, PLT_H, 4, COL_PLATE, rgb(0x0f, 0x0f, 0x11));
    fill_rect(out, PLT_X + 2, PLT_Y + 1, PLT_W - 4, 1, COL_PLATE_LIP);

    // --- Identity ----------------------------------------------------------
    // Set above the display, the way the hardware sets it, in the plain
    // lettering of the panel rather than anybody's logotype.
    silk(out, 48, 46, 10, 1, "MULTI TIMBRE", COL_SILK_DIM);
    silk(out, 48, 66, 10, 1, "SOUND MODULE", COL_SILK_DIM);
    silk(out, 206, 42, 30, 2, "MT-32", COL_SILK);
    silk(out, 206, 84, 10, 1, "NUKED-MT32 EMULATOR", COL_SILK_DIM);

    // --- Display -----------------------------------------------------------
    for (int k = 5; k >= 1; k--)
        round_rect(out, LCD_X - BEZEL - k, LCD_Y - BEZEL - k,
                   LCD_DW + (BEZEL + k) * 2, LCD_DH + (BEZEL + k) * 2, 3 + k,
                   rgb(0x16 + (5 - k), 0x28 + (5 - k) * 2, 0x22 + (5 - k)), COL_GLOW);
    fill_rect(out, LCD_X - BEZEL, LCD_Y - BEZEL,
              LCD_DW + BEZEL * 2, LCD_DH + BEZEL * 2, COL_BEZEL);
    fill_rect(out, LCD_X - BEZEL, LCD_Y - BEZEL, LCD_DW + BEZEL * 2, 2, COL_BEZEL_LIP);

    // Averaged down rather than sampled, so a thin stroke in the character
    // cell stays visible instead of falling between two source pixels.
    for (int y = 0; y < LCD_DH; y++)
        for (int x = 0; x < LCD_DW; x++) {
            unsigned r = 0, g = 0, b = 0;
            for (int j = 0; j < LCD_SCALE; j++)
                for (int i = 0; i < LCD_SCALE; i++) {
                    uint32_t c = mt32.lcd_buffer[y * LCD_SCALE + j][x * LCD_SCALE + i];
                    r += c & 0xff; g += (c >> 8) & 0xff; b += (c >> 16) & 0xff;
                }
            const unsigned n = LCD_SCALE * LCD_SCALE;
            px(out, LCD_X + x, LCD_Y + y, rgb(int(r / n), int(g / n), int(b / n)));
        }

    silk(out, LCD_X - BEZEL, LCD_Y + LCD_DH + BEZEL + 12, 11, 1,
         "MIDI MESSAGE", COL_SILK_DIM);

    // Power lamp, under the near end of the display.
    silk(out, LCD_X + 250, LCD_Y + LCD_DH + BEZEL + 12, 11, 1, "POWER", COL_SILK_DIM);
    disc(out, LCD_X + 360, LCD_Y + LCD_DH + BEZEL + 17, 7, rgb(0x2c, 0x2c, 0x30),
         rgb(0x10, 0x10, 0x12));
    disc(out, LCD_X + 360, LCD_Y + LCD_DH + BEZEL + 17, 4,
         mt32.lcd_is_on() ? COL_LED_ON : COL_LED_OFF, COL_BEZEL);

    // --- Buttons -----------------------------------------------------------
    // Two rows of five. The bracket over the first three is the hardware's own
    // way of saying those select a part.
    {
        int x0 = button_x(0), x1 = button_x(2) + BTN_W, ty = BTN_Y0 - 16;
        line(out, x0, ty + 6, x0, ty, 0, COL_SILK_DIM);
        line(out, x0, ty, x1, ty, 0, COL_SILK_DIM);
        line(out, x1, ty, x1, ty + 6, 0, COL_SILK_DIM);
        silk_centred(out, (x0 + x1) / 2, ty - 16, 11, 1, "PART", COL_SILK_DIM);
    }

    for (int i = 0; i < 10; i++) {
        int bx = button_x(i), by = button_y(i);
        bool down = (mt32.button >> i) & 1;
        bool numbered = (kButtons[i].label[1] == 0);   // "1".."5" are one character

        fill_rect(out, bx + 2, by + 3, BTN_W, BTN_H, rgb(0x0d, 0x0d, 0x0f));
        round_rect(out, bx, by, BTN_W, BTN_H, 4,
                   down ? COL_BTN_LIT : COL_BTN, COL_BTN_EDGE);
        if (!down)
            fill_rect(out, bx + 3, by + 2, BTN_W - 6, 1, COL_BTN_TOP);

        // The numbered keys carry their digit; the function keys are named
        // underneath, in the pale blue the hardware uses for them.
        if (numbered) {
            silk_centred(out, bx + BTN_W / 2, by + 16, 16, 2,
                         kButtons[i].label, down ? rgb(0x18, 0x18, 0x1c) : COL_SILK);
        }
        // A numbered key carries its digit and needs nothing underneath; a
        // function key is named below it, in the pale blue used for those.
        if (!numbered) {
            const int lh = 13;
            int n = kButtons[i].label2 ? 2 : 1;
            // above on the top row, below on the bottom, so a legend never
            // sits between two keys and belongs to neither
            int ly = (i < 5) ? by - 6 - n * lh : by + BTN_H + 7;
            silk_centred(out, bx + BTN_W / 2, ly, 10, 1, kButtons[i].label, COL_LABEL);
            if (kButtons[i].label2)
                silk_centred(out, bx + BTN_W / 2, ly + lh, 10, 1,
                             kButtons[i].label2, COL_LABEL);
        }
        draw_text_centred(out, bx + BTN_W / 2, by + BTN_H - 13, 1,
                          kButtons[i].key, rgb(0x6e, 0x72, 0x78));
    }
    silk(out, BTN_X0, BTN_Y0 + 2 * (BTN_H + BTN_ROW_GAP) + 10, 11, 1,
         "KEYS 1-0 OR CLICK", COL_SILK_DIM);

    // --- Volume ------------------------------------------------------------
    silk_centred(out, KNOB_CX, KNOB_CY - KNOB_R - 32, 11, 1, "SELECT/VOLUME", COL_LABEL);

    // A ring of small dots around the knob, as on the hardware.
    for (int i = 0; i <= 20; i++) {
        double a = (-235.0 + 290.0 * (i / 20.0)) * 3.14159265 / 180.0;
        disc(out, KNOB_CX + int(cos(a) * (KNOB_R + 11)),
                  KNOB_CY + int(sin(a) * (KNOB_R + 11)), 1,
             COL_SILK_DIM, COL_SILK_DIM);
    }

    disc(out, KNOB_CX + 2, KNOB_CY + 3, KNOB_R, rgb(0x0d, 0x0d, 0x0f), rgb(0x0d, 0x0d, 0x0f));
    disc(out, KNOB_CX, KNOB_CY, KNOB_R, COL_KNOB_RIM, COL_KNOB_EDGE);
    disc(out, KNOB_CX, KNOB_CY, KNOB_R - 4, COL_KNOB, COL_KNOB_EDGE);
    for (int i = 0; i < 44; i++) {          // knurling
        double a = i * (2 * 3.14159265 / 44);
        line(out, KNOB_CX + int(cos(a) * (KNOB_R - 4)), KNOB_CY + int(sin(a) * (KNOB_R - 4)),
                  KNOB_CX + int(cos(a) * (KNOB_R - 1)), KNOB_CY + int(sin(a) * (KNOB_R - 1)),
                  0, rgb(0x2a, 0x2a, 0x2e));
    }
    {
        int v = mt32.knob;
        if (v < 0) v = 0;
        if (v > 1023) v = 1023;
        double a = (-235.0 + 290.0 * (v / 1023.0)) * 3.14159265 / 180.0;
        line(out, KNOB_CX + int(cos(a) * 6), KNOB_CY + int(sin(a) * 6),
                  KNOB_CX + int(cos(a) * (KNOB_R - 8)), KNOB_CY + int(sin(a) * (KNOB_R - 8)),
                  2, COL_KNOB_MARK);
    }
    silk_centred(out, KNOB_CX, KNOB_CY + KNOB_R + 24, 11, 1, "DRAG OR -/=", COL_SILK_DIM);

    // --- Corner screws -----------------------------------------------------
    const int sx_[4] = {PLT_X - 10, panel_w - PLT_X + 9, PLT_X - 10, panel_w - PLT_X + 9};
    const int sy_[4] = {14, 14, panel_h - 20, panel_h - 20};
    for (int i = 0; i < 4; i++) {
        disc(out, sx_[i], sy_[i], 6, rgb(0x34, 0x34, 0x38), rgb(0x14, 0x14, 0x16));
        line(out, sx_[i] - 3, sy_[i] - 1, sx_[i] + 3, sy_[i] + 1, 0, rgb(0x16, 0x16, 0x18));
    }
}
