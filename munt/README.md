# Vendored reverb model from Munt

This directory carries **`BReverbModel.h`** and **`BReverbModel.cpp`** verbatim
from [Munt](https://github.com/munt/munt) (`mt32emu/src/`), together with the
smallest set of headers those two files need.

## Why it is here

Nuked-MT32 emulates the MT-32's digital path from the silicon. The reverb is a
separate dedicated chip and **no decap of it exists**, so there is nothing to
be accurate to. Munt's `BReverbModel` is a behavioural model of the same Boss
reverb, and it is the best available stand-in.

It is therefore **not** part of the chip-accurate emulation. It is an
approximation, and the emulator labels it as one. Use `--reverb=off` to hear
the raw digital output with no reverb at all, which is what upstream
Nuked-MT32 produces.

## Licensing

`BReverbModel.h` / `BReverbModel.cpp` are:

    Copyright (C) 2003-2009 Dean Beeler, Jerome Fisher
    Copyright (C) 2011-2022 Dean Beeler, Jerome Fisher, Sergey V. Mikayev

licensed **LGPL-2.1-or-later**. The full text is in `COPYING.LESSER.txt`.
LGPL-2.1-or-later is compatible with this project's GPL-2.0-or-later, so the
combined work is distributable under the GPL. The original copyright headers
are preserved unmodified.

## The shim headers

`globals.h`, `Types.h`, `Enumerations.h`, `internals.h` and `Synth.h` here are
**not** Munt's real headers. They are minimal stand-ins providing only what
`BReverbModel` references, so the two real files can stay byte-identical to
upstream and be refreshed by a straight copy. The declarations in them are
transcribed from the Munt sources they replace — in particular
`Synth::clipSampleEx` and `Synth::muteSampleBuffer` are copied verbatim so the
saturation behaviour is identical.

If you update `BReverbModel.*` from upstream Munt, check whether it has started
using anything else from `Synth.h` or `internals.h`.
