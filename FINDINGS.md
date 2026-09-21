# LA32 findings

Measured notes on the synth-engine bugs upstream flagged but could not track
down. Everything here was produced on this tree with an MT-32 **v2.04** control
ROM (sha1 `2c16432b6c73dd2a3947cba950a0f4c19d6180eb`) and the 512 KiB PCM ROM
(`f6b1eebc4b2d200ec6d3d21d51325d5b48c60252`).

The reference is **Munt** (`munt/munt`) rendering the same note sequence with
`setReverbEnabled(false)`, since Nuked-MT32 has no reverb at all. Munt is a
behavioral emulator, not gate-accurate, so it is a strong cross-check and not
ground truth. Real hardware has not been consulted.

Munt's default `COARSE` analog mode boosts high frequencies, so it was checked
whether that alone explained the differences below. It does not: switching to
`AnalogOutputMode_DIGITAL_ONLY`, which matches the digital-only path Nuked
emulates, moves the centroid 868 -> 846 Hz, leaves Munt's DC at -6.25 and the
level ratio at 2.02. All three discrepancies survive the control.

Reproduce with `tools/compare.py munt.wav nuked.wav`.

## What is already correct

- **Pitch is exact.** Across a single note and a three-note chord, every
  spectral peak matches Munt to the FFT bin: 261.7, 330.1, 392.6, 523.4, 660.2,
  785.2, 989.3, 1177.7 Hz. Oscillator tuning and note handling are right.
- **Envelope shape is right.** RMS tracks Munt over attack, sustain and a ~2 s
  decay, and both fall silent at the same moment.
- **Broad timbre is close.** Log-spectrum correlation 0-8 kHz is 0.93 on a
  single note and 0.87 on a chord.
- **Output routing loses nothing.** `outch` can be 0-3 while the mix sums only
  accumulators 2,3 (left) and 6,7 (right), which looks like partials being
  dropped. Instrumented across four parts plus rhythm, all 891,356 active
  partial-samples routed to `outch 2` and 100% of accumulated energy reached
  the output. Channels 0/1 appear to serve the separate outputs.

## Open discrepancies, most concrete first

### 1. Systematic negative DC bias

Nuked's whole-file mean is **-452** where Munt's is **-6.5**, and the sample
range is skewed: Nuked L runs -11532/+8898 against Munt's symmetric
-7354/+7286. A near-DC spectral peak at ~3 Hz shows up in Nuked and not Munt.

Note that DC offset *during a note* is authentic - Munt shows it too
(-738 on one note, +301 on a chord). What differs is that Munt's averages out
over time while Nuked's stays negative.

**Localized to the `w1` wave path.** Instrumenting the chain shows panning is
not responsible - `left` and `ot2` sum back to `oo` - and that inside the wave
stage the two halves behave completely differently:

```
mean oo             :  -46.136   per active partial-sample
mean ot3 (from w2)  :   +0.001   balanced
mean ot4 (from w1)  :  -21.003   <-- the entire DC source
w1 complemented     :   58.06%   <-- should be ~50% for a symmetric wave
w2 complemented     :    0.15%
```

The thing to explain is the 58/42 split on
`if (sign_flip ^ sign1) w1 = ~w1;` - either the sign logic
(`sign1 = !w469`, `w469 = !(w467 ^ w326)`) fires too often, or `calc_pow`'s
output distribution is asymmetric by design and the hardware compensates
downstream. Resolving that needs the decap.

**Consequence:** the offset is ~21% of signal RMS and skews the clipping range
(-11532/+8898 against Munt's symmetric -7350/+7302), so negative peaks clip
first and this should worsen with polyphony.

**Two fixes were tried and both failed** - do not repeat them. Reworking the
pan split (carry forced to 1, round-to-nearest, or plain `ot2 = oo - left`)
reaches at best -446.16, with three of the variants byte-identical, confirming
they are algebraically the same. Replacing the one's complement `~w1` with a
true two's complement `-w1` reaches -451.88.

The original suspicion, recorded because it was wrong, was the pan-split
arithmetic in `la32.cpp`:

```c
int32_t ot1 = mul(oo, pan1);
accum[oddeven][outch] += ot1 >> 7;
int32_t c   = (~ot1 >> 6) & 1;
int32_t ot2 = oo + (~ot1 >> 7) + c;
```

For arithmetic shifts, `x >> 7` + `(~x) >> 7` is exactly `-1`, so the two
channels sum to `oo - 1 + c`. The carry `c` is bit 6 of `~ot1`, making the
split round to nearest - which leaves an average **-0.5 LSB per active partial
per sample**, always negative. Whether the hardware corrects this elsewhere is
the open question.

### 2. Output is roughly 2x Munt's level

RMS ratio is 2.017 against `DIGITAL_ONLY`, and unchanged by every variant
tried, so it is a stable property of the signal chain rather than noise. Beware: this may be a
convention difference rather than a Nuked bug, since Munt applies its own output
gain and `AnalogOutputMode`. Do not "fix" this by scaling until it is checked
against hardware. The hardcoded `s_l *= 3; s_r *= 3;` at the end of
`la32_t::clock` is where any such calibration lives.

### 3. Timbre is consistently darker

Spectral centroid is **689 Hz vs 846 Hz** on a chord against `DIGITAL_ONLY`
(about 19% low) and 664 vs 828 on a single note. Upper harmonics are
relatively attenuated. Consistent across material, so it is systematic rather
than incidental.

### 4. The accumulator ping-pong is inert

`accum` is `[2][8]` and `oddeven` flips per sample, which is the shape of a
double buffer - but `memset(accum, 0, sizeof(accum)); // TEMP` clears **both**
banks every sample and reads use the same index as writes, so the second bank
never does anything. Whether the hardware reads the opposite bank (a one-sample
delay) is unresolved. The `// TEMP` is upstream's own marker.

## Dead ends, so nobody repeats them

- **The `// FIXME:` above `w49`/`w50` is not a stub.** Both are initialised
  `false` and then unconditionally assigned by the exhaustive
  `switch ((i >> 3) & 3)` immediately below. The comment flags uncertainty about
  the mapping, not missing code.
- **The long decay is not a bug.** A note decaying for seconds after note-off
  looked wrong, but Munt decays over the same span and both reach silence at the
  same time.
- **Sample-level waveform comparison is useless.** Nuked feeds MIDI through an
  emulated UART at roughly one byte per 3840 cycles, so note onsets land at
  different times than Munt's immediate `playMsg`. Pearson correlation on raw
  samples is ~-0.01 even though the spectra agree at 0.93. Compare spectra.
