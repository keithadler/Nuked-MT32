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
#include "reverb.h"
#include "munt/BReverbModel.h"
#include <vector>
#include <string.h>

using MT32Emu::BReverbModel;
using MT32Emu::ReverbMode;

static constexpr int MAX_CHUNK = 1024;

Mt32Reverb::Mt32Reverb() {}

Mt32Reverb::~Mt32Reverb()
{
    for (int i = 0; i < 4; i++)
        delete static_cast<BReverbModel *>(models_[i]);
}

void Mt32Reverb::init()
{
    for (int i = 0; i < 4; i++) {
        if (models_[i]) continue;
        // mt32CompatibleModel = true: the MT-32's reverb, not the CM-32L's.
        BReverbModel *m = BReverbModel::createBReverbModel(
            ReverbMode(i), true, MT32Emu::RendererType_BIT16S);
        m->open();
        m->setParameters(uint8_t(time_), uint8_t(level_));
        models_[i] = m;
    }
}

void Mt32Reverb::setMode(int mode)
{
    if (mode < 0) mode = 0;
    if (mode > 3) mode = 3;
    if (mode == mode_) return;
    mode_ = mode;
    if (models_[mode_])
        static_cast<BReverbModel *>(models_[mode_])->mute();
}

void Mt32Reverb::setParameters(int time, int level)
{
    if (time  < 0) time  = 0;  if (time  > 7) time  = 7;
    if (level < 0) level = 0;  if (level > 7) level = 7;
    time_ = time; level_ = level;
    for (int i = 0; i < 4; i++)
        if (models_[i])
            static_cast<BReverbModel *>(models_[i])->setParameters(uint8_t(time_), uint8_t(level_));
}

void Mt32Reverb::observeMidiByte(uint8_t b)
{
    if (locked_)
        return;

    if (b == 0xf0) { sx_state_ = IN_SYSEX; sx_len_ = 0; return; }
    if (sx_state_ != IN_SYSEX) return;

    if (b == 0xf7) {
        // Roland MT-32 DT1: 41 <dev> 16 12 <a1 a2 a3> <data...>
        // System area 10 00 01 = reverb mode, 02 = time, 03 = level.
        if (sx_len_ >= 8 && sx_[0] == 0x41 && sx_[2] == 0x16 && sx_[3] == 0x12 &&
            sx_[4] == 0x10 && sx_[5] == 0x00) {
            int addr = sx_[6];
            int n = sx_len_ - 7 - 1;          // minus address bytes and checksum
            for (int i = 0; i < n && addr + i <= 0x03; i++) {
                int v = sx_[7 + i] & 0x7f;
                switch (addr + i) {
                case 0x01: setMode(v); break;
                case 0x02: setParameters(v, level_); break;
                case 0x03: setParameters(time_, v); break;
                default: break;
                }
            }
        }
        sx_state_ = IDLE; sx_len_ = 0;
        return;
    }

    if (b & 0x80) { sx_state_ = IDLE; sx_len_ = 0; return; }   // aborted
    if (sx_len_ < int(sizeof sx_))
        sx_[sx_len_++] = b;
}

void Mt32Reverb::process(int16_t *frames, int count)
{
    BReverbModel *m = static_cast<BReverbModel *>(models_[mode_]);
    if (!m) return;

    static std::vector<int16_t> il, ir, ol, orr;
    if (int(il.size()) < MAX_CHUNK) { il.resize(MAX_CHUNK); ir.resize(MAX_CHUNK);
                                      ol.resize(MAX_CHUNK); orr.resize(MAX_CHUNK); }

    int done = 0;
    while (done < count) {
        int n = count - done;
        if (n > MAX_CHUNK) n = MAX_CHUNK;

        for (int i = 0; i < n; i++) {
            il[i] = frames[(done + i) * 2];
            ir[i] = frames[(done + i) * 2 + 1];
        }

        if (m->process(il.data(), ir.data(), ol.data(), orr.data(), MT32Emu::Bit32u(n))) {
            // Dry plus wet, saturating - the reverb chip sits alongside the
            // DAC output rather than replacing it.
            for (int i = 0; i < n; i++) {
                int32_t l = int32_t(il[i]) + int32_t(ol[i]);
                int32_t r = int32_t(ir[i]) + int32_t(orr[i]);
                if (l < -32768) l = -32768; if (l > 32767) l = 32767;
                if (r < -32768) r = -32768; if (r > 32767) r = 32767;
                frames[(done + i) * 2]     = int16_t(l);
                frames[(done + i) * 2 + 1] = int16_t(r);
            }
        }
        done += n;
    }
}
