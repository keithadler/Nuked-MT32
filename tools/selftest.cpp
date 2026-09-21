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
 *  End-to-end self test. Boots the machine, checks the front panel, pushes
 *  MIDI in and checks that audio comes out. Exits non-zero on any failure,
 *  so it works as a regression gate while the synth engine is worked on.
 */
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "mt32.h"
#include "rom.h"

mt32_t mt32;

static constexpr int SR = 32000;
static double T = 0;
static long   peak = 0;

static void run(double secs)
{
    double end = T + secs;
    while (T < end) {
        mt32.clock(256);
        T += 256.0 / SR;
        for (int i = 0; i < 256; i++) {
            long v = mt32.samples[i][0];
            if (v < 0) v = -v;
            if (v > peak) peak = v;
        }
    }
}

static std::string lcd()
{
    const uint8_t *d = mt32.lcd_text();
    std::string s;
    for (int i = 0; i < 20; i++) {
        uint8_t c = d[i];
        s += (c >= 0x20 && c < 0x7f) ? char(c) : '.';
    }
    return s;
}

static void send(std::vector<uint8_t> v)
{
    for (uint8_t b : v)
        mt32.post_midi(b);
}

static int failures = 0;

static void check(bool ok, const char *what, const std::string &detail)
{
    printf("  %s  %s", ok ? "[ok]  " : "[FAIL]", what);
    if (!detail.empty())
        printf("  -> %s", detail.c_str());
    putchar('\n');
    if (!ok)
        failures++;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: %s CONTROL.ROM PCM.ROM\n", argv[0]);
        return 2;
    }

    std::string err;
    if (!rom_load_control(argv[1], mt32.rom, mt32.old_machine, err)) {
        printf("error: %s\n", err.c_str());
        return 2;
    }
    if (!rom_load(argv[2], mt32.pcm, ROM_PCM_SIZE, "PCM", err)) {
        printf("error: %s\n", err.c_str());
        return 2;
    }

    printf("Nuked-MT32 self test\n");
    printf("  machine: MT-32 %s\n", mt32.old_machine ? "v1.xx (old)" : "v2.xx (new)");

    // 1. Boot splash.
    run(0.05);
    std::string splash = lcd();
    check(splash.find("Roland MT-32") != std::string::npos,
          "boot splash on LCD", splash);

    // 2. Idle display.
    run(2.5);
    std::string idle = lcd();
    check(idle.find("vol:") != std::string::npos, "idle display", idle);

    // 3. Display SysEx round trip - proves CPU, UART, SysEx and checksum.
    {
        std::vector<uint8_t> sx = {0xf0, 0x41, 0x10, 0x16, 0x12, 0x20, 0x00, 0x00};
        const char *msg = "SELFTEST OK         ";
        int sum = 0x20;
        for (int i = 0; i < 20; i++) { sx.push_back(uint8_t(msg[i])); sum += uint8_t(msg[i]); }
        sx.push_back(uint8_t((128 - (sum & 0x7f)) & 0x7f));
        sx.push_back(0xf7);
        send(sx);
        run(1.5);
        std::string got = lcd();
        check(got.find("SELFTEST OK") != std::string::npos,
              "display SysEx accepted", got);
    }

    // 4. Channel 1 is unassigned by default and must stay silent.
    peak = 0;
    send({0x90, 60, 110});
    run(1.0);
    check(peak == 0, "unassigned channel 1 stays silent",
          "peak " + std::to_string(peak));
    send({0x80, 60, 0});
    run(0.3);

    // 5. Part 1 is MIDI channel 2 by default - this must make sound.
    peak = 0;
    send({0xc1, 0x00});
    run(0.2);
    send({0x91, 60, 110});
    run(1.2);
    check(peak > 500, "note on channel 2 produces audio",
          "peak " + std::to_string(peak));
    send({0x81, 60, 0});

    // 6. Rhythm part on channel 10.
    peak = 0;
    send({0x99, 37, 110});
    run(1.0);
    check(peak > 500, "rhythm on channel 10 produces audio",
          "peak " + std::to_string(peak));

    printf("\n%d check(s) failed.\n", failures);
    if (failures == 0)
        printf("PASS\n");
    return failures ? 1 : 0;
}
