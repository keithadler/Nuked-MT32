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
 *  macOS CoreMIDI input backend.
 *
 *  Creates a virtual destination named "Nuked-MT32" so any DAW or MIDI
 *  utility can target the emulator directly, and additionally connects to
 *  every physical MIDI source present so a hardware keyboard just works.
 *
 *  Raw bytes are handed straight to mt32_post_midi_observed(), which feeds the
 *  emulated UART - so running status and SysEx pass through untouched.
 */
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
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

static MIDIClientRef  midi_client;
static MIDIEndpointRef midi_virtual_dest;
static MIDIPortRef    midi_in_port;

static void feed_packets(const MIDIPacketList *pktlist)
{
    const MIDIPacket *packet = &pktlist->packet[0];
    for (UInt32 i = 0; i < pktlist->numPackets; i++) {
        for (UInt16 j = 0; j < packet->length; j++)
            mt32_post_midi_observed(packet->data[j]);
        packet = MIDIPacketNext(packet);
    }
}

static void connect_all_sources(void)
{
    ItemCount n = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < n; i++) {
        MIDIEndpointRef src = MIDIGetSource(i);
        if (!src)
            continue;

        // Don't loop our own virtual destination back into ourselves.
        CFStringRef name = NULL;
        MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &name);

        if (MIDIPortConnectSource(midi_in_port, src, NULL) == noErr && name) {
            char buf[256] = {0};
            CFStringGetCString(name, buf, sizeof buf, kCFStringEncodingUTF8);
            printf("  MIDI in: %s\n", buf);
        }
        if (name)
            CFRelease(name);
    }
}

int MIDI_Init(int port)
{
    (void)port;

    OSStatus err = MIDIClientCreate(CFSTR("Nuked-MT32"), NULL, NULL, &midi_client);
    if (err != noErr) {
        fprintf(stderr, "CoreMIDI: MIDIClientCreate failed (%d)\n", (int)err);
        return 0;
    }

    err = MIDIDestinationCreateWithBlock(
        midi_client, CFSTR("Nuked-MT32"), &midi_virtual_dest,
        ^(const MIDIPacketList *pktlist, void *srcConnRefCon) {
            (void)srcConnRefCon;
            feed_packets(pktlist);
        });
    if (err != noErr)
        fprintf(stderr, "CoreMIDI: could not create virtual destination (%d)\n", (int)err);
    else
        printf("  MIDI in: virtual destination \"Nuked-MT32\"\n");

    err = MIDIInputPortCreateWithBlock(
        midi_client, CFSTR("Nuked-MT32 In"), &midi_in_port,
        ^(const MIDIPacketList *pktlist, void *srcConnRefCon) {
            (void)srcConnRefCon;
            feed_packets(pktlist);
        });
    if (err == noErr)
        connect_all_sources();
    else
        fprintf(stderr, "CoreMIDI: could not create input port (%d)\n", (int)err);

    return 1;
}

void MIDI_Quit(void)
{
    if (midi_in_port)      { MIDIPortDispose(midi_in_port);          midi_in_port = 0; }
    if (midi_virtual_dest) { MIDIEndpointDispose(midi_virtual_dest); midi_virtual_dest = 0; }
    if (midi_client)       { MIDIClientDispose(midi_client);         midi_client = 0; }
}
