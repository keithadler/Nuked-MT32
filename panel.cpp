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

// Reuse the LCD controller's own 5x8 font for panel lettering, so the panel
// needs no font dependency. Defined in lcd_font.h, included by lcd.cpp.
extern uint8_t lcd_font[160][8];

namespace {

// Byte order matches lcd_buffer: low byte is red.
constexpr uint32_t rgb(int r, int g, int b)
{
    return 0xff000000u | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
}

const uint32_t COL_CASE_TOP   = rgb(0xd2, 0xcd, 0xc0);
const uint32_t COL_CASE_BOT   = rgb(0xaf, 0xa9, 0x9b);
const uint32_t COL_CASE_EDGE  = rgb(0x6e, 0x6a, 0x60);
const uint32_t COL_BEZEL      = rgb(0x1c, 0x1e, 0x1b);
const uint32_t COL_BEZEL_LIP  = rgb(0x45, 0x48, 0x42);
const uint32_t COL_BTN        = rgb(0x35, 0x38, 0x3e);
const uint32_t COL_BTN_LIT    = rgb(0x7d, 0x86, 0x92);
const uint32_t COL_BTN_EDGE   = rgb(0x1b, 0x1d, 0x21);
const uint32_t COL_TEXT       = rgb(0xec, 0xec, 0xe8);
const uint32_t COL_TEXT_DARK  = rgb(0x3a, 0x37, 0x31);
const uint32_t COL_KNOB       = rgb(0x2c, 0x2e, 0x33);
const uint32_t COL_KNOB_EDGE  = rgb(0x16, 0x17, 0x1a);
const uint32_t COL_KNOB_MARK  = rgb(0xf0, 0xd8, 0x70);
const uint32_t COL_LED_ON     = rgb(0x62, 0xf0, 0x80);
const uint32_t COL_LED_OFF    = rgb(0x24, 0x40, 0x28);

// --- Geometry ---------------------------------------------------------------

constexpr int LCD_X = 236, LCD_Y = 38;          // native 840 x 100
constexpr int BEZEL = 10;

constexpr int BTN_Y = 196, BTN_W = 78, BTN_H = 60, BTN_GAP = 8;
constexpr int BTN_X0 = 236;

constexpr int KNOB_CX = 1152, KNOB_CY = 126, KNOB_R = 54;

struct ButtonDef { const char *label; const char *key; };

// Index is the bit in mt32.button, which matches the MT32_BUTTTON_* enum.
// Keys 1-0 map to bits 0-9, same as the keyboard handling in main.cpp.
const ButtonDef kButtons[10] = {
    {"1",      "1"}, {"2",      "2"}, {"3",      "3"},
    {"GROUP",  "4"}, {"VOLUME", "5"}, {"4",      "6"},
    {"5",      "7"}, {"RHYTHM", "8"}, {"SOUND",  "9"},
    {"MASTER", "0"},
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

} // namespace

int panel_hit_button(int x, int y)
{
    if (y < BTN_Y || y >= BTN_Y + BTN_H) return -1;
    for (int i = 0; i < 10; i++) {
        int bx = BTN_X0 + i * (BTN_W + BTN_GAP);
        if (x >= bx && x < bx + BTN_W) return i;
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
    // Chassis with a soft vertical gradient.
    for (int y = 0; y < panel_h; y++) {
        float t = float(y) / float(panel_h - 1);
        int r = int((1 - t) * (COL_CASE_TOP & 0xff)         + t * (COL_CASE_BOT & 0xff));
        int g = int((1 - t) * ((COL_CASE_TOP >> 8) & 0xff)  + t * ((COL_CASE_BOT >> 8) & 0xff));
        int b = int((1 - t) * ((COL_CASE_TOP >> 16) & 0xff) + t * ((COL_CASE_BOT >> 16) & 0xff));
        uint32_t c = rgb(r, g, b);
        for (int x = 0; x < panel_w; x++) out[y * panel_w + x] = c;
    }
    fill_rect(out, 0, panel_h - 4, panel_w, 4, COL_CASE_EDGE);
    fill_rect(out, 0, 0, panel_w, 2, rgb(0xe6, 0xe2, 0xd6));

    // Identity block. Functional text only - no manufacturer branding.
    draw_text(out, 30,  50, 5, "MT-32", COL_TEXT_DARK);
    draw_text(out, 30, 100, 2, "LA SYNTHESIZER", COL_TEXT_DARK);
    draw_text(out, 30, 122, 2, "EMULATOR", COL_TEXT_DARK);
    draw_text(out, 30, 158, 2, "NUKED-MT32", COL_TEXT_DARK);

    // Power / activity LED.
    draw_text(out, 30, 222, 2, "POWER", COL_TEXT_DARK);
    disc(out, 108, 227, 7, mt32.lcd_is_on() ? COL_LED_ON : COL_LED_OFF, COL_BEZEL);

    // LCD bezel and the display itself, blitted 1:1.
    fill_rect(out, LCD_X - BEZEL + 3, LCD_Y - BEZEL + 4,
              lcd_w + BEZEL * 2, lcd_h + BEZEL * 2, rgb(0x8f, 0x8a, 0x7e));
    fill_rect(out, LCD_X - BEZEL, LCD_Y - BEZEL,
              lcd_w + BEZEL * 2, lcd_h + BEZEL * 2, COL_BEZEL);
    fill_rect(out, LCD_X - BEZEL, LCD_Y - BEZEL, lcd_w + BEZEL * 2, 2, COL_BEZEL_LIP);
    for (int y = 0; y < lcd_h; y++)
        for (int x = 0; x < lcd_w; x++)
            px(out, LCD_X + x, LCD_Y + y, mt32.lcd_buffer[y][x]);

    // Buttons.
    for (int i = 0; i < 10; i++) {
        int bx = BTN_X0 + i * (BTN_W + BTN_GAP);
        bool down = (mt32.button >> i) & 1;
        round_rect(out, bx, BTN_Y, BTN_W, BTN_H, 7,
                   down ? COL_BTN_LIT : COL_BTN, COL_BTN_EDGE);
        draw_text_centred(out, bx + BTN_W / 2, BTN_Y + 16, 2, kButtons[i].label, COL_TEXT);
        draw_text_centred(out, bx + BTN_W / 2, BTN_Y + 38, 1, kButtons[i].key, COL_TEXT);
    }
    draw_text(out, BTN_X0, BTN_Y + BTN_H + 10, 1,
              "KEYS 1-0 OR CLICK", COL_TEXT_DARK);

    // Volume knob. 0-1023 mapped over a 270 degree sweep.
    disc(out, KNOB_CX, KNOB_CY, KNOB_R, COL_KNOB, COL_KNOB_EDGE);
    disc(out, KNOB_CX, KNOB_CY, KNOB_R - 12, rgb(0x3a, 0x3d, 0x43), COL_KNOB_EDGE);
    {
        int v = mt32.knob;
        if (v < 0) v = 0;
        if (v > 1023) v = 1023;
        double a = (-225.0 + 270.0 * (v / 1023.0)) * 3.14159265 / 180.0;
        int ex = KNOB_CX + int(cos(a) * (KNOB_R - 8));
        int ey = KNOB_CY + int(sin(a) * (KNOB_R - 8));
        line(out, KNOB_CX, KNOB_CY, ex, ey, 2, COL_KNOB_MARK);
        disc(out, KNOB_CX, KNOB_CY, 5, COL_KNOB_MARK, COL_KNOB_EDGE);
    }
    draw_text_centred(out, KNOB_CX, KNOB_CY + KNOB_R + 12, 2, "VOLUME", COL_TEXT_DARK);
    draw_text_centred(out, KNOB_CX, KNOB_CY + KNOB_R + 32, 1, "DRAG OR -/=", COL_TEXT_DARK);

    // Cosmetic chassis detail: vent slots and corner screws.
    for (int i = 0; i < 6; i++)
        fill_rect(out, 26, 264 + i * 9, 150, 3, rgb(0xbd, 0xb8, 0xaa));

    const int sx_[4] = {16, panel_w - 17, 16, panel_w - 17};
    const int sy_[4] = {16, 16, panel_h - 21, panel_h - 21};
    for (int i = 0; i < 4; i++) {
        disc(out, sx_[i], sy_[i], 7, rgb(0x9a, 0x95, 0x88), rgb(0x70, 0x6c, 0x60));
        line(out, sx_[i] - 4, sy_[i], sx_[i] + 4, sy_[i], 0, rgb(0x5e, 0x5a, 0x50));
    }
}
