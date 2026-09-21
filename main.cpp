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
#include <stdio.h>
#include <string.h>
#include <string>
#include <SDL.h>
#include <SDL_opengl.h>
#include "mt32.h"
#include "midi.h"
#include "rom.h"
#include "panel.h"
#include "dcblock.h"
#include "reverb.h"

static SDL_Window *window;
static SDL_GLContext gl_context;
static uint32_t panel_buffer[panel_h * panel_w];

mt32_t mt32;
static DcBlocker dc_blocker;
static bool dc_block_enabled = false;
Mt32Reverb reverb;
bool reverb_enabled = true;

static constexpr int SAMPLE_RATE = 32000;

static void mt32_callback(void *, Uint8 *stream, int len)
{
    const int frames = len / 4; // stereo, 16-bit
    mt32.clock(frames);
    if (reverb_enabled)
        reverb.process(&mt32.samples[0][0], frames);
    if (dc_block_enabled)
        dc_blocker.process(&mt32.samples[0][0], frames);
    memcpy(stream, mt32.samples, size_t(len));
}

static void usage(const char *argv0)
{
    printf(
        "Nuked-MT32 - Roland MT-32 (\"new\", v2.x) emulator\n"
        "\n"
        "Usage: %s [options] [CONTROL.ROM PCM.ROM]\n"
        "\n"
        "Options:\n"
        "  -c, --control PATH   Control ROM image (128 KiB)\n"
        "  -p, --pcm PATH       PCM ROM image (512 KiB)\n"
        "  -s, --scale N        Initial window scale factor (default 1)\n"
        "      --reverb MODE    munt (default) or off. The MT-32's reverb chip\n"
        "                       has never been decapped, so this is Munt's\n"
        "                       behavioural model, not chip-accurate emulation.\n"
        "      --dc-block       Remove the DC offset, as the real unit's AC\n"
        "                       coupled output does (mitigation, not a fix)\n"
        "  -h, --help           Show this help\n"
        "\n"
        "If no ROM paths are given, the current directory is searched for\n"
        "MT32_CONTROL.ROM / MT32_PCM.ROM, then mt32_cpu.bin / mt32_pcm.bin.\n"
        "\n"
        "ROM images are copyrighted Roland firmware and are NOT distributed\n"
        "with this program. Supply your own, dumped from hardware you own.\n"
        "\n"
        "Front panel: click the buttons, or press 1-0. Drag the volume knob,\n"
        "or use -/= . Esc quits.\n",
        argv0);
}

static bool file_exists(const char *p)
{
    FILE *f = fopen(p, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

int main(int argc, char **argv)
{
    std::string control_path, pcm_path;
    int scale = 1;

    for (int i = 1; i < argc; i++) {
        // Accept both "--opt value" and "--opt=value".
        std::string arg(argv[i]);
        std::string inline_val;
        bool has_inline = false;
        if (arg.size() > 2 && arg.compare(0, 2, "--") == 0) {
            size_t eq = arg.find('=');
            if (eq != std::string::npos) {
                inline_val = arg.substr(eq + 1);
                arg = arg.substr(0, eq);
                has_inline = true;
            }
        }
        const char *a = arg.c_str();
        auto next = [&](const char *what) -> const char * {
            if (has_inline) return inline_val.c_str();
            if (i + 1 >= argc) {
                fprintf(stderr, "error: %s requires a value\n", what);
                exit(1);
            }
            return argv[++i];
        };

        if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(argv[0]); return 0; }
        else if (!strcmp(a, "-c") || !strcmp(a, "--control")) control_path = next(a);
        else if (!strcmp(a, "-p") || !strcmp(a, "--pcm"))     pcm_path = next(a);
        else if (!strcmp(a, "-s") || !strcmp(a, "--scale"))   scale = atoi(next(a));
        else if (!strcmp(a, "--dc-block")) dc_block_enabled = true;
        else if (!strcmp(a, "--reverb")) {
            const char *v = next(a);
            if      (!strcmp(v, "off"))  reverb_enabled = false;
            else if (!strcmp(v, "munt")) reverb_enabled = true;
            else { fprintf(stderr, "error: --reverb takes munt or off\n"); return 1; }
        }
        else if (a[0] == '-') {
            fprintf(stderr, "error: unknown option \"%s\"\n", a);
            return 1;
        }
        else if (control_path.empty()) control_path = a;
        else if (pcm_path.empty())     pcm_path = a;
        else {
            fprintf(stderr, "error: unexpected argument \"%s\"\n", a);
            return 1;
        }
    }

    if (scale < 1) scale = 1;

    if (control_path.empty()) {
        if      (file_exists("MT32_CONTROL.ROM")) control_path = "MT32_CONTROL.ROM";
        else if (file_exists("mt32_cpu.bin"))     control_path = "mt32_cpu.bin";
    }
    if (pcm_path.empty()) {
        if      (file_exists("MT32_PCM.ROM")) pcm_path = "MT32_PCM.ROM";
        else if (file_exists("mt32_pcm.bin")) pcm_path = "mt32_pcm.bin";
    }

    if (control_path.empty() || pcm_path.empty()) {
        fprintf(stderr,
                "error: no ROM images found.\n"
                "       Pass them explicitly, e.g.:\n"
                "         %s -c MT32_CONTROL.ROM -p MT32_PCM.ROM\n"
                "       Run with --help for details.\n", argv[0]);
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

    {
        const char *cn = rom_identify(rom_sha1(mt32.rom, ROM_CONTROL_SIZE));
        const char *pn = rom_identify(rom_sha1(mt32.pcm, ROM_PCM_SIZE));
        printf("Nuked-MT32\n");
        printf("  control: %s%s%s\n", control_path.c_str(), *cn ? " - " : "", cn);
        printf("  pcm:     %s%s%s\n", pcm_path.c_str(), *pn ? " - " : "", pn);
    }

    if (reverb_enabled) {
        reverb.init();
        printf("  reverb:  munt model (behavioural, not chip-accurate)\n");
    } else {
        printf("  reverb:  off\n");
    }

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "error: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    int w = panel_w * scale;
    int h = panel_h * scale;

    window = SDL_CreateWindow("Nuked-MT32",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                              SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "error: SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        fprintf(stderr, "error: SDL_GL_CreateContext: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    mt32.button = 0;

    int  mouse_button = -1;   // panel button currently held by the mouse
    bool knob_drag    = false;
    int  knob_ref_y   = 0;
    int  knob_ref_val = 0;

    GLuint lcd_tex;
    glGenTextures(1, &lcd_tex);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, lcd_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    MIDI_Init(0);

    SDL_AudioSpec spec = {};
    spec.freq     = SAMPLE_RATE;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = 1024;
    spec.callback = mt32_callback;

    SDL_AudioSpec spec_actual = {};
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &spec, &spec_actual, 0);
    if (!dev) {
        fprintf(stderr, "error: SDL_OpenAudioDevice: %s\n", SDL_GetError());
        MIDI_Quit();
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("  audio:   %d Hz, %d ch, %d frame buffer\n",
           spec_actual.freq, spec_actual.channels, spec_actual.samples);

    SDL_PauseAudioDevice(dev, 0);

    bool quit = false;
    while (!quit) {
        int dw, dh;
        SDL_GL_GetDrawableSize(window, &dw, &dh);

        panel_render(mt32, panel_buffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, panel_w, panel_h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, panel_buffer);

        glViewport(0, 0, dw, dh);
        glClear(GL_COLOR_BUFFER_BIT);

        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(-1, -1);
        glTexCoord2f(0, 0); glVertex2f(-1,  1);
        glTexCoord2f(1, 0); glVertex2f( 1,  1);
        glTexCoord2f(1, 1); glVertex2f( 1, -1);
        glEnd();

        SDL_GL_SwapWindow(window);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    quit = true;
                    break;
                }
                if (event.key.keysym.scancode == SDL_SCANCODE_MINUS) {
                    mt32.knob -= 10;
                    if (mt32.knob < 0) mt32.knob = 0;
                    printf("knob: %i\n", mt32.knob);
                    break;
                }
                if (event.key.keysym.scancode == SDL_SCANCODE_EQUALS) {
                    mt32.knob += 10;
                    if (mt32.knob > 1023) mt32.knob = 1023;
                    printf("knob: %i\n", mt32.knob);
                    break;
                }
                [[fallthrough]];

            case SDL_KEYUP: {
                int bit = -1;
                if (event.key.keysym.scancode >= SDL_SCANCODE_1 &&
                    event.key.keysym.scancode <= SDL_SCANCODE_0)
                    bit = event.key.keysym.scancode - SDL_SCANCODE_1;

                if (bit >= 0) {
                    if (event.type == SDL_KEYDOWN) mt32.button |=  (1u << bit);
                    else                           mt32.button &= ~(1u << bit);
                }
                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                if (event.button.button != SDL_BUTTON_LEFT)
                    break;
                int ww, wh;
                SDL_GetWindowSize(window, &ww, &wh);
                int px = ww ? event.button.x * panel_w / ww : 0;
                int py = wh ? event.button.y * panel_h / wh : 0;

                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    int b = panel_hit_button(px, py);
                    if (b >= 0) {
                        mouse_button = b;
                        mt32.button |= (1u << b);
                    } else if (panel_hit_knob(px, py)) {
                        knob_drag = true;
                        knob_ref_y = event.button.y;
                        knob_ref_val = mt32.knob;
                    }
                } else {
                    if (mouse_button >= 0) {
                        mt32.button &= ~(1u << mouse_button);
                        mouse_button = -1;
                    }
                    knob_drag = false;
                }
                break;
            }

            case SDL_MOUSEMOTION: {
                if (!knob_drag)
                    break;
                // Drag up to raise, down to lower; full sweep over ~200 px.
                int dy = knob_ref_y - event.motion.y;
                int v = knob_ref_val + dy * 1024 / 200;
                if (v < 0) v = 0;
                if (v > 1023) v = 1023;
                mt32.knob = v;
                break;
            }

            default:
                break;
            }
        }
    }

    SDL_PauseAudioDevice(dev, 1);
    SDL_CloseAudioDevice(dev);
    MIDI_Quit();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
