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
 *  Headless offline renderer. Runs the emulator faster (or slower) than
 *  real time with no audio device, feeding it a Standard MIDI File and
 *  writing a WAV. This is the project's test harness: deterministic
 *  output means regressions in the synth engine are diffable.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include "mt32.h"
#include "rom.h"
#include "dcblock.h"

mt32_t mt32;

static constexpr int SAMPLE_RATE = 32000;

// --- Standard MIDI File parsing --------------------------------------------

struct MidiEvent {
    double   time;   // seconds
    std::vector<uint8_t> bytes;
};

struct Reader {
    const uint8_t *p, *end;
    bool ok = true;

    uint8_t u8() {
        if (p >= end) { ok = false; return 0; }
        return *p++;
    }
    uint32_t be(int n) {
        uint32_t v = 0;
        for (int i = 0; i < n; i++) v = (v << 8) | u8();
        return v;
    }
    uint32_t vlq() {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) {
            uint8_t b = u8();
            v = (v << 7) | (b & 0x7f);
            if (!(b & 0x80)) break;
        }
        return v;
    }
};

static bool parse_smf(const std::vector<uint8_t> &data,
                      std::vector<MidiEvent> &out, std::string &err,
                      int &sysex_warnings)
{
    sysex_warnings = 0;
    Reader r{data.data(), data.data() + data.size()};

    if (data.size() < 14 || memcmp(data.data(), "MThd", 4) != 0) {
        err = "not a Standard MIDI File (missing MThd)";
        return false;
    }
    r.p += 4;
    uint32_t hdrlen = r.be(4);
    uint16_t format  = uint16_t(r.be(2));
    uint16_t ntracks = uint16_t(r.be(2));
    int16_t  division = int16_t(r.be(2));
    r.p += hdrlen - 6;

    if (division <= 0) {
        err = "SMPTE time division is not supported";
        return false;
    }
    (void)format;

    // Collect events per track in ticks, then convert with a tempo map.
    struct RawEv { uint64_t tick; std::vector<uint8_t> bytes; bool tempo; uint32_t usPerQn; };
    std::vector<RawEv> evs;

    for (uint16_t t = 0; t < ntracks && r.ok; t++) {
        while (r.ok && r.p + 8 <= r.end && memcmp(r.p, "MTrk", 4) != 0) {
            uint32_t skip = 0;
            for (int i = 4; i < 8; i++) skip = (skip << 8) | r.p[i];
            r.p += 8 + skip;
        }
        if (r.p + 8 > r.end) break;
        r.p += 4;
        uint32_t tracklen = r.be(4);
        const uint8_t *tend = r.p + tracklen;
        if (tend > r.end) tend = r.end;

        uint64_t tick = 0;
        uint8_t running = 0;

        while (r.ok && r.p < tend) {
            tick += r.vlq();
            uint8_t b = r.u8();

            if (b == 0xff) {                       // meta
                uint8_t type = r.u8();
                uint32_t len = r.vlq();
                if (type == 0x51 && len == 3) {
                    uint32_t us = (uint32_t(r.p[0]) << 16) |
                                  (uint32_t(r.p[1]) << 8)  | uint32_t(r.p[2]);
                    evs.push_back({tick, {}, true, us});
                }
                r.p += len;
                continue;
            }
            if (b == 0xf0 || b == 0xf7) {          // SysEx
                uint32_t len = r.vlq();
                std::vector<uint8_t> msg;
                if (b == 0xf0) msg.push_back(0xf0);
                for (uint32_t i = 0; i < len && r.p < tend; i++) msg.push_back(r.u8());
                // A well-formed SysEx event ends with F7. If it does not, the
                // length was wrong and we have just swallowed whatever events
                // followed - which shows up later as inexplicable stuck notes.
                // Warn rather than fail: the rest of the file may still be fine.
                if (msg.empty() || msg.back() != 0xf7)
                    sysex_warnings++;
                evs.push_back({tick, msg, false, 0});
                continue;
            }

            std::vector<uint8_t> msg;
            uint8_t status;
            if (b & 0x80) { status = b; running = b; }
            else          { status = running; r.p--; }
            if (!status) continue;

            msg.push_back(status);
            int nd = ((status & 0xf0) == 0xc0 || (status & 0xf0) == 0xd0) ? 1 : 2;
            for (int i = 0; i < nd; i++) msg.push_back(r.u8());
            evs.push_back({tick, msg, false, 0});
        }
        r.p = tend;
    }

    std::stable_sort(evs.begin(), evs.end(),
                     [](const RawEv &a, const RawEv &b) { return a.tick < b.tick; });

    double   seconds   = 0.0;
    uint64_t lasttick  = 0;
    double   secPerTick = 0.5 / division; // default 120 BPM

    for (auto &e : evs) {
        seconds += double(e.tick - lasttick) * secPerTick;
        lasttick = e.tick;
        if (e.tempo) {
            secPerTick = (e.usPerQn / 1000000.0) / division;
            continue;
        }
        out.push_back({seconds, e.bytes});
    }
    return true;
}

// --- WAV output -------------------------------------------------------------

static void wav_header(FILE *f, uint32_t nframes)
{
    uint32_t datasz = nframes * 4;
    uint32_t riffsz = 36 + datasz;
    auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); w32(riffsz); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); w32(16); w16(1); w16(2);
    w32(SAMPLE_RATE); w32(SAMPLE_RATE * 4); w16(4); w16(16);
    fwrite("data", 1, 4, f); w32(datasz);
}

// ----------------------------------------------------------------------------

static void usage(const char *a0)
{
    printf(
        "mt32-render - headless Nuked-MT32 renderer\n"
        "\n"
        "Usage: %s -c CONTROL.ROM -p PCM.ROM [-m song.mid] [-t SECONDS] -o out.wav\n"
        "\n"
        "  -c, --control PATH   Control ROM image (128 KiB)\n"
        "  -p, --pcm PATH       PCM ROM image (512 KiB)\n"
        "  -m, --midi PATH      Standard MIDI File to play (optional)\n"
        "  -t, --seconds N      Render length; default = MIDI length + 2s, or 5\n"
        "  -o, --out PATH       Output WAV (default out.wav)\n"
        "  -w, --warmup N       Seconds to run before the MIDI starts, so the\n"
        "                       machine is booted. Default: wait until the front\n"
        "                       panel reaches its idle display. 0 disables.\n"
        "      --dc-block       Remove the DC offset, as the real unit's AC\n"
        "                       coupled output does. A mitigation, not a fix -\n"
        "                       see FINDINGS.md.\n"
        "  -h, --help           Show this help\n"
        "\n"
        "ROM images are copyrighted Roland firmware and are NOT distributed\n"
        "with this program. Supply your own, dumped from hardware you own.\n", a0);
}

int main(int argc, char **argv)
{
    std::string control_path, pcm_path, midi_path, out_path = "out.wav";
    double seconds = -1.0;
    double warmup = -1.0;     // <0 means "detect"
    bool dc_block = false;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        auto next = [&](const char *what) -> const char * {
            if (i + 1 >= argc) { fprintf(stderr, "error: %s requires a value\n", what); exit(1); }
            return argv[++i];
        };
        if      (!strcmp(a, "-h") || !strcmp(a, "--help"))    { usage(argv[0]); return 0; }
        else if (!strcmp(a, "-c") || !strcmp(a, "--control")) control_path = next(a);
        else if (!strcmp(a, "-p") || !strcmp(a, "--pcm"))     pcm_path = next(a);
        else if (!strcmp(a, "-m") || !strcmp(a, "--midi"))    midi_path = next(a);
        else if (!strcmp(a, "-o") || !strcmp(a, "--out"))     out_path = next(a);
        else if (!strcmp(a, "-t") || !strcmp(a, "--seconds")) seconds = atof(next(a));
        else if (!strcmp(a, "--dc-block")) dc_block = true;
        else if (!strcmp(a, "-w") || !strcmp(a, "--warmup")) warmup = atof(next(a));
        else { fprintf(stderr, "error: unknown argument \"%s\"\n", a); return 1; }
    }

    if (control_path.empty() || pcm_path.empty()) {
        fprintf(stderr, "error: --control and --pcm are required\n");
        return 1;
    }

    std::string err;
    if (!rom_load(control_path.c_str(), mt32.rom, ROM_CONTROL_SIZE, "control", err)) {
        fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    if (!rom_load(pcm_path.c_str(), mt32.pcm, ROM_PCM_SIZE, "PCM", err)) {
        fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    std::vector<MidiEvent> events;
    if (!midi_path.empty()) {
        FILE *f = fopen(midi_path.c_str(), "rb");
        if (!f) { fprintf(stderr, "error: cannot open \"%s\"\n", midi_path.c_str()); return 1; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        std::vector<uint8_t> data(size_t(n < 0 ? 0 : n));
        size_t rd = fread(data.data(), 1, data.size(), f);
        fclose(f);
        data.resize(rd);
        int sysex_warnings = 0;
        if (!parse_smf(data, events, err, sysex_warnings)) {
            fprintf(stderr, "error: %s: %s\n", midi_path.c_str(), err.c_str());
            return 1;
        }
        printf("  midi:    %s (%zu events)\n", midi_path.c_str(), events.size());
        if (sysex_warnings)
            fprintf(stderr,
                    "  WARNING: %d SysEx block(s) do not end with F7. The file's\n"
                    "           SysEx lengths look wrong, so following events may\n"
                    "           have been consumed. Expect missing or stuck notes.\n",
                    sysex_warnings);
    }

    if (seconds < 0) {
        seconds = events.empty() ? 5.0 : events.back().time + 2.0;
    }

    // The MT-32 takes a couple of seconds to boot and ignores MIDI until it
    // has. Without this the opening bars of a file are silently dropped.
    if (warmup != 0.0) {
        double elapsed = 0.0;
        const double limit = (warmup > 0.0) ? warmup : 6.0;
        bool ready = false;
        while (elapsed < limit) {
            mt32.clock(256);
            elapsed += 256.0 / SAMPLE_RATE;
            if (warmup < 0.0) {
                // Idle display reached: the unit is listening.
                const uint8_t *d = mt32.lcd_text();
                std::string t;
                for (int i = 0; i < 20; i++)
                    t += (d[i] >= 0x20 && d[i] < 0x7f) ? char(d[i]) : ' ';
                if (t.find("vol:") != std::string::npos) { ready = true; break; }
            }
        }
        printf("  warmup:  %.2fs%s\n", elapsed,
               warmup < 0.0 ? (ready ? " (idle display reached)"
                                     : " (timed out waiting for idle display)")
                            : "");
    }

    FILE *out = fopen(out_path.c_str(), "wb");
    if (!out) { fprintf(stderr, "error: cannot write \"%s\"\n", out_path.c_str()); return 1; }

    uint32_t total = uint32_t(seconds * SAMPLE_RATE);
    wav_header(out, total);

    DcBlocker dc;
    const uint32_t CHUNK = 1024;
    size_t next_ev = 0;
    uint32_t done = 0;
    int64_t peak = 0;

    while (done < total) {
        uint32_t n = std::min(CHUNK, total - done);
        double t_end = double(done + n) / SAMPLE_RATE;

        while (next_ev < events.size() && events[next_ev].time <= t_end) {
            for (uint8_t b : events[next_ev].bytes)
                mt32.post_midi(b);
            next_ev++;
        }

        mt32.clock(n);
        if (dc_block)
            dc.process(&mt32.samples[0][0], int(n));
        fwrite(mt32.samples, 4, n, out);

        for (uint32_t i = 0; i < n; i++) {
            int64_t l = mt32.samples[i][0] < 0 ? -int64_t(mt32.samples[i][0]) : mt32.samples[i][0];
            int64_t r = mt32.samples[i][1] < 0 ? -int64_t(mt32.samples[i][1]) : mt32.samples[i][1];
            peak = std::max(peak, std::max(l, r));
        }

        done += n;
    }

    fclose(out);
    printf("  wrote:   %s (%.2fs, %u frames, peak %lld / 32767)\n",
           out_path.c_str(), seconds, total, (long long)peak);
    return 0;
}
