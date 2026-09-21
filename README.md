# Nuked-MT32

Roland MT-32 emulator, worked on by nukeykt in late 2024 / early 2025 and
[released as-is](https://github.com/nukeykt/Nuked-MT32) in September 2026.
This tree continues it with a portable build and a test harness.

Uses the MCS-96 emulator from Olivier Galibert / MAME.
The LA32 emulator is based on the decap by [John McMaster](https://siliconprawn.org).

## Status

Honest state of things, from upstream plus what has been verified here:

- The emulation core (MCS-96 CPU, LA32, LCD) builds clean on Apple Silicon.
- **It boots and it makes sound.** With an MT-32 v2.04 control ROM the machine
  shows ` ** Roland MT-32 ** `, settles to the idle display, accepts MIDI
  including SysEx, and renders audio. `mt32-selftest` checks all of this.
- **The reverb chip is not emulated at all** - no decap of it is available.
- **There are known bugs in the LA32 synth engine** that upstream could not
  track down. Output has not been compared against real hardware, so do not
  assume it is accurate yet.

### Leads on the LA32 bugs

Measured on this tree, for whoever picks this up:

- `la32.cpp` clears the accumulator with `memset(accum, 0, sizeof(accum));
  // TEMP` at the top of every sample. `accum` is `[2][8]` and `oddeven` flips
  per sample, so it is shaped like a ping-pong buffer - but because both banks
  are cleared every sample and reads use the same index as writes, the
  double-buffering currently does nothing. Whether the hardware really reads
  the opposite bank (a one-sample delay) is unresolved.
- The `// FIXME:` above `w49`/`w50` is **not** a stub: both are assigned by the
  exhaustive `switch ((i >> 3) & 3)` immediately below. The comment appears to
  flag uncertainty about the mapping, not missing code.
- Output routing is *not* losing signal. `outch` can be 0-3 while the mix only
  sums accumulators 2,3 (left) and 6,7 (right), which looks like dropped
  partials - but measured over multiple parts and the rhythm channel, every
  active partial routed to `outch 2` and 100% of accumulated energy reached
  the output. Channels 0/1 appear to serve the separate outputs.
- A rendered note decays for roughly 5 seconds after note-off, which looks long
  and is worth checking against hardware or Munt.

### Gotcha: MIDI channels

The MT-32 assigns parts 1-8 to MIDI channels **2-9**, and rhythm to channel 10.
Channel 1 is unassigned by default, so notes sent there are correctly ignored
and produce silence. This is the first thing to check if you hear nothing.

## Self test

```sh
./build/mt32-selftest CONTROL.ROM PCM.ROM
```

Boots the machine and asserts the splash, the idle display, a display-SysEx
round trip (which exercises CPU, UART, SysEx parsing and checksum validation),
that unassigned channel 1 stays silent, and that channels 2 and 10 produce
audio. Exits non-zero on failure, so it works as a regression gate while the
synth engine is being worked on.

To wire it into CTest, point the build at your ROMs:

```sh
cmake -B build -DMT32_TEST_CONTROL_ROM=/path/CONTROL.ROM \
                -DMT32_TEST_PCM_ROM=/path/PCM.ROM
ctest --test-dir build
```

## ROM requirements

This core emulates the **"new" MT-32 (v2.x)** - a P8098 CPU with 128 KiB of
control ROM in 8 x 16 KiB banks. It needs:

| Image       | Size            | Notes                                  |
|-------------|-----------------|----------------------------------------|
| Control ROM | 131072 (128 KiB) | MT-32 v2.03 / v2.04 / v2.06 / v2.07    |
| PCM ROM     | 524288 (512 KiB) | Shared between old and new MT-32       |

A 64 KiB **v1.xx control ROM will not work** - that is the "old" MT-32, a
different machine. The loader detects this and says so explicitly. For old
MT-32 emulation, use [Munt](https://github.com/munt/munt).

**ROM images are copyrighted Roland firmware and are not distributed with this
program.** Supply your own, dumped from hardware you own. `.gitignore` is set
up to keep ROM images out of the repository; please keep it that way.

## Building

Requires CMake 3.16+, a C++17 compiler, SDL2, and OpenGL.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

macOS: `brew install cmake sdl2`
Debian/Ubuntu: `apt install cmake libsdl2-dev libasound2-dev`

## Running

```sh
./build/nuked-mt32 -c MT32_CONTROL.ROM -p MT32_PCM.ROM
```

Keys: `1`-`0` are the front panel buttons, `-`/`=` move the volume knob,
`Esc` quits.

MIDI input is automatic. On macOS the emulator publishes a virtual CoreMIDI
destination named **Nuked-MT32** - select it as the output in any DAW or MIDI
player - and also connects to every physical MIDI source it finds.

## Identifying ROM images

`mt32-romid` fingerprints ROM files by SHA-1 and says which are usable:

```sh
./build/mt32-romid *.bin *.ROM
```

It recognizes full images and the 32 KiB / 256 KiB halves that MAME-style
dumps come in, so a pile of `ic26`/`ic27` files can be sorted out quickly.

## Offline rendering

`mt32-render` runs the machine headless with no audio device, plays a Standard
MIDI File through it and writes a WAV. Output is deterministic, so synth-engine
regressions are diffable - useful while chasing the LA32 bugs.

```sh
./build/mt32-render -c MT32_CONTROL.ROM -p MT32_PCM.ROM -m song.mid -o out.wav
```

## Changes from upstream

- CMake build replacing the Visual Studio-only solution; builds on macOS,
  Linux and Windows from one tree.
- Fixed `mame/emu.h` including `"..\mt32.h"` with a backslash, which no
  non-Windows compiler accepts.
- Portable `main.cpp`: no `Windows.h`, no `gl\GL.h`, no `__fallthrough`.
- Fixed `glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_2D, GL_CLAMP)` - the second
  argument should be `GL_TEXTURE_WRAP_T`. Also moved to `GL_CLAMP_TO_EDGE`.
- CoreMIDI backend for macOS with a virtual destination; winmm retained for
  Windows; ALSA selected on Linux.
- ROM loading moved out of `main()` into `rom.cpp`, with size validation,
  SHA-1 identification against known images, and real error messages. Upstream
  did `return 0` from `main()` on any ROM problem, which exited silently.
- ROM paths are now command-line arguments with `--help`, rather than fixed
  filenames in the working directory.
- Added `mt32-render`, a headless renderer with a Standard MIDI File parser.
- HiDPI-correct rendering, vsync, and clean shutdown of audio/MIDI/GL.

## License

GPL-2.0, as upstream. The MAME-derived MCS-96 core under `mame/` carries its
own license - see `mame/COPYING`.
