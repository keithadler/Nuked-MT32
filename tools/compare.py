#!/usr/bin/env python3
"""
Differential test against Munt (the reference MT-32 emulator).

Renders nothing itself - give it two WAVs of the same material, one from
Munt with reverb disabled and one from Nuked-MT32, and it reports where
they disagree. Sample-level comparison is useless here because Nuked feeds
MIDI through an emulated UART, so note onsets land at different times;
everything below is phase-independent.

    python3 tools/compare.py munt.wav nuked.wav

Requires numpy.
"""
import sys, wave
import numpy as np


def load(path):
    w = wave.open(path)
    n = w.getnframes()
    s = np.frombuffer(w.readframes(n), dtype='<i2').reshape(-1, 2).astype(float)
    return s, w.getframerate()


def envelope(ref, nuk, sr):
    print("Envelope (RMS per 250 ms, left channel):")
    print("  %-8s %10s %10s %8s" % ("time", "munt", "nuked", "ratio"))
    step = sr // 4
    n = min(len(ref), len(nuk))
    for i in range(0, n, step):
        a, b = ref[i:i+step, 0], nuk[i:i+step, 0]
        if not len(a):
            break
        ra = np.sqrt((a*a).mean())
        rb = np.sqrt((b*b).mean())
        r = (rb/ra) if ra > 1 else float('nan')
        print("  %6.2fs %10.0f %10.0f %8.2f" % (i/sr, ra, rb, r))


def spectrum(ref, nuk, sr, t, dur=0.35):
    def sp(x):
        seg = x[int(t*sr):int((t+dur)*sr), 0]
        seg = seg * np.hanning(len(seg))
        S = np.abs(np.fft.rfft(seg, 32768))
        return S / (S.max() or 1)
    f = np.fft.rfftfreq(32768, 1/sr)
    A, B = sp(ref), sp(nuk)
    m = f < 8000
    ca = (f[m]*A[m]).sum() / max(A[m].sum(), 1e-9)
    cb = (f[m]*B[m]).sum() / max(B[m].sum(), 1e-9)
    r = np.corrcoef(np.log10(A[m]+1e-6), np.log10(B[m]+1e-6))[0, 1]
    print("\nSpectrum @ %.2fs:" % t)
    print("  log-spectrum correlation (0-8 kHz): %.3f" % r)
    print("  spectral centroid: munt %.0f Hz, nuked %.0f Hz  (%+.1f%%)"
          % (ca, cb, 100*(cb-ca)/ca if ca else 0))


def dc_and_range(ref, nuk):
    print("\nDC bias and range:")
    print("  whole-file mean   munt %+8.2f   nuked %+8.2f" %
          (ref[:, 0].mean(), nuk[:, 0].mean()))
    for name, x in (("munt", ref), ("nuked", nuk)):
        print("  %-5s L min %7d max %7d   R min %7d max %7d"
              % (name, x[:, 0].min(), x[:, 0].max(),
                 x[:, 1].min(), x[:, 1].max()))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    ref, sr = load(sys.argv[1])
    nuk, _ = load(sys.argv[2])
    envelope(ref, nuk, sr)
    dc_and_range(ref, nuk)
    for t in (1.05, 2.60):
        spectrum(ref, nuk, sr, t)
    return 0


if __name__ == "__main__":
    sys.exit(main())
