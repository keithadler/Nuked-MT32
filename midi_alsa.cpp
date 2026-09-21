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
 *  Linux ALSA sequencer MIDI input backend.
 *
 *  Creates a writable sequencer port named "Nuked-MT32" that other clients
 *  can connect to with aconnect(1) or any patchbay. Incoming events are
 *  decoded back to a raw byte stream and fed to the emulated UART, so
 *  SysEx passes through intact.
 */
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdio.h>
#include "mt32.h"
#include "midi.h"
#include "reverb.h"


extern mt32_t mt32;
extern Mt32Reverb reverb;
extern bool reverb_enabled;

static inline void mt32_post_midi_observed(uint8_t b)
{
    mt32.post_midi(b);
    if (reverb_enabled) reverb.observeMidiByte(b);
}

static snd_seq_t       *seq_handle;
static snd_midi_event_t *midi_parser;
static pthread_t         midi_thread;
static volatile bool     midi_running;

static void *midi_loop(void *)
{
    while (midi_running) {
        snd_seq_event_t *ev = nullptr;
        int r = snd_seq_event_input(seq_handle, &ev);
        if (r < 0) {
            if (r == -EAGAIN || r == -ENOSPC)
                continue;
            break;
        }
        if (!ev)
            continue;

        unsigned char buf[1024];
        long n = snd_midi_event_decode(midi_parser, buf, sizeof buf, ev);
        if (n > 0) {
            for (long i = 0; i < n; i++)
                mt32_post_midi_observed(buf[i]);
        }
    }
    return nullptr;
}

int MIDI_Init(int port)
{
    (void)port;

    if (snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
        fprintf(stderr, "ALSA: cannot open sequencer\n");
        seq_handle = nullptr;
        return 0;
    }

    snd_seq_set_client_name(seq_handle, "Nuked-MT32");

    int p = snd_seq_create_simple_port(
        seq_handle, "Nuked-MT32",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_SYNTH);
    if (p < 0) {
        fprintf(stderr, "ALSA: cannot create sequencer port\n");
        snd_seq_close(seq_handle);
        seq_handle = nullptr;
        return 0;
    }

    // No size limit, so long SysEx dumps are delivered whole.
    if (snd_midi_event_new(0, &midi_parser) < 0) {
        fprintf(stderr, "ALSA: cannot create MIDI event parser\n");
        snd_seq_close(seq_handle);
        seq_handle = nullptr;
        return 0;
    }
    snd_midi_event_no_status(midi_parser, 1);

    midi_running = true;
    if (pthread_create(&midi_thread, nullptr, midi_loop, nullptr) != 0) {
        fprintf(stderr, "ALSA: cannot start MIDI thread\n");
        midi_running = false;
        snd_midi_event_free(midi_parser);
        midi_parser = nullptr;
        snd_seq_close(seq_handle);
        seq_handle = nullptr;
        return 0;
    }

    printf("  MIDI in: ALSA sequencer port \"Nuked-MT32\" (client %d:%d)\n",
           snd_seq_client_id(seq_handle), p);
    return 1;
}

void MIDI_Quit(void)
{
    if (midi_running) {
        midi_running = false;
        pthread_cancel(midi_thread);
        pthread_join(midi_thread, nullptr);
    }
    if (midi_parser) { snd_midi_event_free(midi_parser); midi_parser = nullptr; }
    if (seq_handle)  { snd_seq_close(seq_handle);        seq_handle = nullptr; }
}
