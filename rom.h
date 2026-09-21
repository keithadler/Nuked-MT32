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
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>

// Expected image sizes for the machine this core emulates (MT-32 "new", v2.x).
static constexpr size_t ROM_CONTROL_SIZE = 0x20000; // 128 KiB, 8 banks x 16 KiB
static constexpr size_t ROM_PCM_SIZE     = 0x80000; // 512 KiB

// Loads `path` into `dst`, which must be exactly `expect_size` bytes.
// On failure returns false and fills `error` with a human-readable reason.
// `kind` is "control" or "PCM" and is used for diagnostics only.
bool rom_load(const char *path, uint8_t *dst, size_t expect_size,
              const char *kind, std::string &error);

// Lowercase hex SHA-1 of a buffer.
std::string rom_sha1(const uint8_t *data, size_t len);

// Returns a human-readable ROM name for a known SHA-1, or "" if unrecognized.
// Only hashes are stored here - never ROM contents.
const char *rom_identify(const std::string &sha1);
