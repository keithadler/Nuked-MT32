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
#include "rom.h"
#include <stdio.h>
#include <string.h>
#include <vector>

// --- Minimal SHA-1 (RFC 3174) ----------------------------------------------

namespace {

struct Sha1 {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    uint64_t len = 0;
    uint8_t buf[64] = {};
    size_t buflen = 0;

    static uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

    void block(const uint8_t *p) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t(p[i*4]) << 24) | (uint32_t(p[i*4+1]) << 16) |
                   (uint32_t(p[i*4+2]) << 8) | uint32_t(p[i*4+3]);
        for (int i = 16; i < 80; i++)
            w[i] = rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;                   k = 0xCA62C1D6u; }
            uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }

    void update(const uint8_t *p, size_t n) {
        len += n;
        while (n) {
            size_t take = 64 - buflen;
            if (take > n) take = n;
            memcpy(buf + buflen, p, take);
            buflen += take; p += take; n -= take;
            if (buflen == 64) { block(buf); buflen = 0; }
        }
    }

    std::string final() {
        uint64_t bits = len * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        uint8_t zero = 0;
        while (buflen != 56) update(&zero, 1);
        uint8_t tail[8];
        for (int i = 0; i < 8; i++) tail[i] = uint8_t(bits >> (56 - i * 8));
        update(tail, 8);

        char out[41];
        for (int i = 0; i < 5; i++)
            snprintf(out + i * 8, 9, "%08x", h[i]);
        return std::string(out, 40);
    }
};

// SHA-1 digests only. These are facts about ROM images, not ROM contents.
// Digests cross-checked against the Munt project's ROM database.
struct KnownRom { const char *sha1; const char *name; };

const KnownRom kKnownRoms[] = {
    // Control ROMs - MT-32 "old" (64 KiB). NOT supported by this core.
    {"5a5cb5a77d7d55ee69657c2f870416daed52dea7", "MT-32 Control v1.04 (old, 64K)"},
    {"e17a3a6d265bf1fa150312061134293d2b58288c", "MT-32 Control v1.05 (old, 64K)"},
    {"a553481f4e2794c10cfe597fef154eef0d8257de", "MT-32 Control v1.06 (old, 64K)"},
    {"b083518fffb7f66b03c23b7eb4f868e62dc5a987", "MT-32 Control v1.07 (old, 64K)"},
    {"7b8c2a5ddb42fd0732e2f22b3340dcf5360edf92", "MT-32 Control BlueRidge (old, 64K)"},
    // Control ROMs - MT-32 "new" (128 KiB). These are what this core wants.
    {"5837064c9df4741a55f7c4d8787ac158dff2d3ce", "MT-32 Control v2.03 (new, 128K)"},
    {"2c16432b6c73dd2a3947cba950a0f4c19d6180eb", "MT-32 Control v2.04 (new, 128K)"},
    {"2869cf4c235d671668cfcb62415e2ce8323ad4ed", "MT-32 Control v2.06 (new, 128K)"},
    {"47b52adefedaec475c925e54340e37673c11707c", "MT-32 Control v2.07 (new, 128K)"},
    // Control ROM halves (32 KiB, byte-multiplexed - a pair makes one 64 KiB
    // "old" MT-32 control ROM). Listed so the tool can name a lone half.
    {"9cd4858014c4e8a9dff96053f784bfaac1092a2e", "MT-32 Control v1.04, half A of 2 (old)"},
    {"fe8db469b5bfeb37edb269fd47e3ce6d91014652", "MT-32 Control v1.04, half B of 2 (old)"},
    {"57a09d80d2f7ca5b9734edbe9645e6e700f83701", "MT-32 Control v1.05, half A of 2 (old)"},
    {"52e3c6666db9ef962591a8ee99be0cde17f3a6b6", "MT-32 Control v1.05, half B of 2 (old)"},
    {"cc83bf23cee533097fb4c7e2c116e43b50ebacc8", "MT-32 Control v1.06, half A of 2 (old)"},
    {"bf4f15666bc46679579498386704893b630c1171", "MT-32 Control v1.06, half B of 2 (old)"},
    {"13f06b38f0d9e0fc050b6503ab777bb938603260", "MT-32 Control v1.07, half A of 2 (old)"},
    {"c55e165487d71fa88bd8c5e9c083bc456c1a89aa", "MT-32 Control v1.07, half B of 2 (old)"},
    {"11a6ae5d8b6ee328b371af7f1e40b82125aa6b4d", "MT-32 Control BlueRidge, half A of 2 (old)"},
    {"e0934320d7cbb5edfaa29e0d01ae835ef620085b", "MT-32 Control BlueRidge, half B of 2 (old)"},
    // PCM halves (256 KiB each; a pair makes the 512 KiB PCM ROM).
    {"3a1e19b0cd4036623fd1d1d11f5f25995585962b", "MT-32 PCM ROM, first half of 2"},
    {"2cadb99d21a6a4a6f5b61b6218d16e9b43f61d01", "MT-32 PCM ROM, second half of 2"},
    {"3ad889fde5db5b6437cbc2eb6e305312fec3df93", "CM-32L PCM ROM, second half of 2"},
    // PCM
    {"f6b1eebc4b2d200ec6d3d21d51325d5b48c60252", "MT-32 PCM ROM (512K)"},
    {"289cc298ad532b702461bfc738009d9ebe8025ea", "CM-32L/CM-64/LAPC-I PCM ROM (1M)"},
};

} // namespace

std::string rom_sha1(const uint8_t *data, size_t len)
{
    Sha1 s;
    s.update(data, len);
    return s.final();
}

const char *rom_identify(const std::string &sha1)
{
    for (const auto &r : kKnownRoms)
        if (sha1 == r.sha1)
            return r.name;
    return "";
}

bool rom_load_control(const char *path, uint8_t *dst, bool &old_machine,
                      std::string &error)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        error = std::string("cannot open control ROM \"") + path + "\"";
        return false;
    }
    fseek(f, 0, SEEK_END);
    long actual = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (actual != long(ROM_CONTROL_SIZE) && actual != long(ROM_CONTROL_SIZE_OLD)) {
        fclose(f);
        char buf[256];
        snprintf(buf, sizeof buf,
                 "control ROM \"%s\" is %ld bytes; expected %zu (MT-32 v2.xx) "
                 "or %zu (MT-32 v1.xx)", path, actual,
                 ROM_CONTROL_SIZE, ROM_CONTROL_SIZE_OLD);
        error = buf;
        if (actual == 0x8000)
            error += "\n  This is a 32 KiB control ROM half. Two multiplexed"
                     "\n  halves make one 64 KiB v1.xx image.";
        return false;
    }

    memset(dst, 0, ROM_CONTROL_SIZE);
    size_t got = fread(dst, 1, size_t(actual), f);
    fclose(f);
    if (got != size_t(actual)) {
        error = std::string("short read on control ROM \"") + path + "\"";
        return false;
    }

    old_machine = (actual == long(ROM_CONTROL_SIZE_OLD));
    return true;
}

bool rom_load(const char *path, uint8_t *dst, size_t expect_size,
              const char *kind, std::string &error)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        error = std::string("cannot open ") + kind + " ROM \"" + path + "\"";
        return false;
    }

    fseek(f, 0, SEEK_END);
    long actual = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (actual < 0) {
        fclose(f);
        error = std::string("cannot determine size of \"") + path + "\"";
        return false;
    }

    if (size_t(actual) != expect_size) {
        // Read what we can so we can still identify the image for the user.
        const size_t asize = size_t(actual);
        std::vector<uint8_t> tmp(asize);
        size_t got = fread(tmp.data(), 1, asize, f);
        fclose(f);

        char buf[512];
        snprintf(buf, sizeof buf,
                 "%s ROM \"%s\" is %ld bytes, expected %zu",
                 kind, path, actual, expect_size);
        error = buf;

        if (got == asize) {
            const char *name = rom_identify(rom_sha1(tmp.data(), got));
            if (*name)
                error += std::string("\n  detected: ") + name;
        }

        if (expect_size == ROM_CONTROL_SIZE && size_t(actual) == 0x8000) {
            error += "\n  This is a 32 KiB control ROM half. Two multiplexed halves"
                     "\n  make one 64 KiB \"old\" MT-32 control ROM - still not what this"
                     "\n  core needs, which is a 128 KiB v2.0x image.";
        }
        else if (expect_size == ROM_CONTROL_SIZE && size_t(actual) == 0x10000) {
            error += "\n  This is an \"old\" MT-32 control ROM. This core emulates the"
                     "\n  \"new\" MT-32 (P8098 CPU, 8 x 16 KiB banked ROM), so it needs a"
                     "\n  128 KiB v2.0x control ROM. For old MT-32 v1.xx, use Munt.";
        }
        return false;
    }

    size_t got = fread(dst, 1, expect_size, f);
    fclose(f);

    if (got != expect_size) {
        error = std::string("short read on ") + kind + " ROM \"" + path + "\"";
        return false;
    }

    return true;
}
