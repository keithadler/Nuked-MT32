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
 *  Reverb, using the behavioural Boss reverb model vendored from Munt.
 *
 *  The MT-32's reverb is a separate chip and no decap of it exists, so this
 *  is NOT part of the chip-accurate emulation - it is an approximation
 *  standing in for hardware nobody has imaged. See munt/README.md.
 *
 *  Mode, time and level follow the machine: the MIDI stream is watched for
 *  writes to the MT-32 system area (0x10 0x00 0x01..0x03), which is how a
 *  song sets its own reverb, and they can also be pinned from the command
 *  line.
 */
#pragma once
#include <stdint.h>

class Mt32Reverb {
public:
    Mt32Reverb();
    ~Mt32Reverb();

    void init();                       // allocate the four mode models
    void setMode(int mode);            // 0 room, 1 hall, 2 plate, 3 tap delay
    void setParameters(int time, int level);   // each 0-7
    void lockSettings(bool locked) { locked_ = locked; }

    // Feed every MIDI byte here so reverb settings track the song.
    // Ignored while settings are locked by the command line.
    void observeMidiByte(uint8_t b);

    // In-place over interleaved stereo int16 frames.
    void process(int16_t *frames, int count);

    int mode()  const { return mode_; }
    int time()  const { return time_; }
    int level() const { return level_; }

private:
    void *models_[4] = {nullptr, nullptr, nullptr, nullptr};
    int mode_ = 0, time_ = 5, level_ = 3;
    bool locked_ = false;

    // SysEx sniffer state
    enum { IDLE, IN_SYSEX } sx_state_ = IDLE;
    uint8_t sx_[16];
    int sx_len_ = 0;
};
