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
 *  Identifies MT-32 ROM images by SHA-1 and says whether each one is
 *  usable with this emulator.
 */
#include <stdio.h>
#include <vector>
#include <string>
#include "rom.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("mt32-romid - identify MT-32 ROM images\n\n"
               "Usage: %s FILE...\n\n"
               "Prints each file's size, SHA-1, and identity if known, and\n"
               "flags which images this emulator can actually use.\n", argv[0]);
        return argc < 2 ? 1 : 0;
    }

    int usable_control = 0, usable_pcm = 0;

    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f) {
            printf("%s\n  cannot open\n", argv[i]);
            continue;
        }
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n < 0) { fclose(f); printf("%s\n  cannot size\n", argv[i]); continue; }

        std::vector<uint8_t> b{};
        b.resize(size_t(n));
        size_t rd = fread(b.data(), 1, b.size(), f);
        fclose(f);
        b.resize(rd);

        std::string h = rom_sha1(b.data(), b.size());
        const char *id = rom_identify(h);

        const char *verdict;
        if (rd == ROM_CONTROL_SIZE)  { verdict = "USABLE as control ROM"; usable_control++; }
        else if (rd == ROM_PCM_SIZE) { verdict = "USABLE as PCM ROM";     usable_pcm++; }
        else                          verdict = "not usable by this core";

        printf("%s\n  %zu bytes, sha1 %s\n  %s\n  -> %s\n",
               argv[i], rd, h.c_str(), *id ? id : "(unrecognized)", verdict);
    }

    printf("\n%d usable control ROM(s), %d usable PCM ROM(s).\n",
           usable_control, usable_pcm);
    if (!usable_control)
        printf("This core needs a 128 KiB MT-32 v2.0x control ROM "
               "(v2.03/2.04/2.06/2.07).\n");
    return 0;
}
