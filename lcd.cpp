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
 *  Thanks:
 *      John McMaster (https://siliconprawn.org):
 *          LA32 chip decap
 *
 */
#include <string.h>
#include "mt32.h"
#include "lcd_font.h"
#include "mame/emu.h"
#include "mame/i8x9x.h"

void mt32_t::lcd_reset(bool soft)
{
    lcd_cursor = 0;
    lcd_cgram_addr = 0;
    lcd_cgram_row = 0;
    lcd_cursor_dir = 0;
    lcd_lines = 1;
    lcd_cursor_font = 0;
    lcd_cursor_blink = 0;
    lcd_display_on = 0;

    if (!soft)
    {
        memset(lcd_cg, 0, sizeof(lcd_cg));
    }
}

void mt32_t::lcd_write(uint8_t a, uint8_t data)
{
    lcd_cycles += lcd_delay;

    if (lcd_cycles < mcu->total_cycles() + lcd_delay)
        lcd_cycles = mcu->total_cycles() + lcd_delay;

    if (a == 0)
    {
        if (data & 0x80)
        {
            lcd_cursor = data & 31;
            if ((data >> 6) & 1)
                lcd_cursor += 10;
            if (lcd_cursor > 19)
                lcd_cursor = 19;
        }
        else if (data & 0x40)
        {
            lcd_cg[lcd_cgram_addr][lcd_cgram_row & 7] = data & 0x1f;
            lcd_cgram_row = (lcd_cgram_row + 1) & 7;
        }
        else if (data & 0x20)
        {
            lcd_cgram_addr = data & 3;
            lcd_cgram_row = 0;
        }
        else if (data == 0x10)
        {
            lcd_reset();
        }
        else
        {
            switch (data & 0x1e)
            {
                case 4:
                    lcd_cursor_dir = data & 1;
                    break;
                case 6:
                    if (data & 1)
                    {
                        if (lcd_lines == 1)
                        {
                            if (lcd_cursor)
                                lcd_cursor--;
                        }
                        else
                        {
                            if (lcd_cursor != 0 && lcd_cursor != 10)
                                lcd_cursor--;
                        }
                    }
                    else
                    {
                        if (lcd_lines == 1)
                        {
                            if (lcd_cursor != 19)
                                lcd_cursor++;
                        }
                        else
                        {
                            if (lcd_cursor != 9 && lcd_cursor != 19)
                                lcd_cursor++;
                        }
                    }
                    break;
                case 8:
                    lcd_cursor_font = data & 1;
                    break;
                case 10:
                    lcd_cursor_blink = data & 1;
                    break;
                case 12:
                    lcd_display_on = data & 1;
                    break;
                case 14:
                    lcd_cursor_on = data & 1;
                    break;
                case 18:
                    lcd_lines = 1 + (data & 1);
                    break;
            }
        }
    }
    else
    {
        lcd_data[lcd_cursor] = data;
        if (lcd_cursor_dir)
        {
            if (lcd_lines == 1)
            {
                if (lcd_cursor)
                    lcd_cursor--;
            }
            else
            {
                if (lcd_cursor != 0 && lcd_cursor != 10)
                    lcd_cursor--;
            }
        }
        else
        {
            if (lcd_lines == 1)
            {
                if (lcd_cursor != 19)
                    lcd_cursor++;
            }
            else
            {
                if (lcd_cursor != 9 && lcd_cursor != 19)
                    lcd_cursor++;
            }
        }
    }
}


static constexpr uint32_t lcd_col1 = 0xff18f2b3;
static constexpr uint32_t lcd_col2 = 0xff30ad23;

void mt32_t::lcd_font_render(int32_t x, int32_t y, uint8_t ch, bool cursor)
{
    uint8_t* f;
    if (ch < 4)
        f = lcd_cg[ch];
    else if (ch >= 0x20 && ch < 0x80)
        f = lcd_font[ch - 0x20];
    else if (ch >= 0xa0 && ch < 0xe0)
        f = lcd_font[ch - 0x40];
    else
        f = lcd_font[0];
    for (uint32_t i = 0; i < 8; i++)
    {
        for (uint32_t j = 0; j < 5; j++)
        {
            uint32_t e = (f[i] & (1 << (4 - j))) != 0;
            if (cursor)
            {
                if (lcd_cursor_font)
                    e = 0;
            }
            else
            {
                if (i == 7)
                    e = 0;
            }
            uint32_t col;
            if (e)
            {
                col = lcd_col1;
            }
            else
            {
                col = lcd_col2;
            }
            uint32_t xx = x + i * 6;
            if (i == 7)
                xx += 6;
            uint32_t yy = y + j * 6;
            for (uint32_t ii = 0; ii < 5; ii++)
            {
                for (uint32_t jj = 0; jj < 5; jj++)
                {
                    lcd_buffer[xx + ii][yy + jj] = col;
                }
            }
        }
    }
}

void mt32_t::lcd_render()
{
    if (!lcd_display_on)
    {
        memset(lcd_buffer, 0, sizeof(lcd_buffer));
        return;
    }

    for (uint32_t i = 0; i < lcd_h; i++)
    {
        for (uint32_t j = 0; j < lcd_w; j++)
        {
            lcd_buffer[i][j] = 0xff2d6914;
        }
    }

    if (lcd_lines != 2)
        return;

    for (uint32_t i = 0; i < 20; i++)
    {
        bool cursor = (i == lcd_cursor) && lcd_cursor_on && lcd_cursor_blink;

        lcd_font_render(25, 25 + i * 40, lcd_data[i], cursor);
    }
}
