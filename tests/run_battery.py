#!/usr/bin/env python3
"""
Renders the MIDI corpus through mt32-render and checks the output for the
failure modes that actually bite an emulator: crashes, hangs, stuck notes,
silence where there should be sound, clipping at the rails, and
non-determinism.

    python3 tests/run_battery.py CONTROL.ROM PCM.ROM [MIDIDIR] [RENDERER]
"""
import hashlib, os, subprocess, sys, wave
import numpy as np

SR = 32000


def render(renderer, ctrl, pcm, mid, out, seconds, extra=()):
    cmd = [renderer, "-c", ctrl, "-p", pcm, "-m", mid, "-o", out,
           "-t", str(seconds), *extra]
    try:
        r = subprocess.run(cmd, capture_output=True, timeout=300)
    except subprocess.TimeoutExpired:
        return None, "HANG (>300s)"
    if r.returncode != 0:
        return None, "EXIT %d: %s" % (r.returncode, r.stderr.decode()[:80])
    return out, None


def analyse(path):
    w = wave.open(path)
    n = w.getnframes()
    a = np.frombuffer(w.readframes(n), dtype='<i2').reshape(-1, 2).astype(np.int32)
    tail = a[int(len(a) - 1.0 * SR):]
    return {
        "frames": n,
        "peak": int(np.abs(a).max()),
        "rms": float(np.sqrt((a.astype(float) ** 2).mean())),
        "tail_peak": int(np.abs(tail).max()) if len(tail) else 0,
        "rail_hits": int(((a <= -32768) | (a >= 32767)).sum()),
        "dc_l": float(a[:, 0].mean()),
        "nonfinite": 0,  # int16 cannot be NaN; kept for symmetry
        "sha": hashlib.sha256(a.tobytes()).hexdigest()[:16],
    }


CASES = {
    # name prefix: (seconds, expect_sound, expect_silent_tail)
    "01": (70, True,  True),
    "02": (22, True,  True),
    "03": (16, True,  True),
    "04": (22, True,  True),
    "05": (18, True,  True),
    "06": (14, True,  True),
    "07": (16, True,  True),
    "08": (18, True,  True),
    "09": (14, True,  True),
    "10": (22, True,  True),
    "11": (14, True,  True),   # CC123 must release the notes
    "12": (14, False, True),   # channel 1 unassigned: total silence
    "13": (12, True,  True),
    "14": (14, True,  True),
    "15": (18, True,  True),
    # CC120 postdates the MT-32, so ignoring it is correct: this one must
    # still be sounding at the end. Kept short so the check window sits just
    # after the controller rather than after the patch's natural decay.
    "16": (6,  True,  False),
}


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    ctrl, pcm = sys.argv[1], sys.argv[2]
    mdir = sys.argv[3] if len(sys.argv) > 3 else "tests/midi"
    renderer = sys.argv[4] if len(sys.argv) > 4 else "./build/mt32-render"
    tmp = os.path.join(mdir, "_out")
    os.makedirs(tmp, exist_ok=True)

    files = sorted(f for f in os.listdir(mdir) if f.endswith(".mid"))
    print("Rendering %d files through %s\n" % (len(files), renderer))
    hdr = "%-26s %7s %7s %8s %6s %8s  %s"
    print(hdr % ("file", "peak", "rms", "tailpk", "rails", "dc", "verdict"))
    print("-" * 92)

    failures = []
    for f in files:
        key = f[:2]
        secs, want_sound, want_quiet_tail = CASES.get(key, (20, True, True))
        out = os.path.join(tmp, f.replace(".mid", ".wav"))
        p, err = render(renderer, ctrl, pcm, os.path.join(mdir, f), out, secs)
        if err:
            print(hdr % (f, "-", "-", "-", "-", "-", err))
            failures.append((f, err)); continue

        m = analyse(p)
        problems = []
        if want_sound and m["peak"] < 200:
            problems.append("SILENT (peak %d)" % m["peak"])
        if not want_sound and m["peak"] > 0:
            problems.append("EXPECTED SILENCE (peak %d)" % m["peak"])
        if want_quiet_tail and m["tail_peak"] > 80:
            problems.append("STUCK NOTE (tail %d)" % m["tail_peak"])
        if not want_quiet_tail and m["tail_peak"] < 80:
            problems.append("EXPECTED STILL SOUNDING (tail %d)" % m["tail_peak"])
        if m["rail_hits"] > 0:
            problems.append("CLIPPED x%d" % m["rail_hits"])

        # determinism: render again and compare the audio hash
        out2 = out.replace(".wav", ".b.wav")
        p2, err2 = render(renderer, ctrl, pcm, os.path.join(mdir, f), out2, secs)
        if err2:
            problems.append("2nd run: " + err2)
        elif analyse(p2)["sha"] != m["sha"]:
            problems.append("NON-DETERMINISTIC")
        else:
            os.remove(out2)

        verdict = "ok" if not problems else "; ".join(problems)
        if problems:
            failures.append((f, verdict))
        print(hdr % (f, m["peak"], "%.0f" % m["rms"], m["tail_peak"],
                     m["rail_hits"], "%+.1f" % m["dc_l"], verdict))

    print("-" * 92)
    if failures:
        print("\n%d FAILURE(S):" % len(failures))
        for f, why in failures:
            print("  %-26s %s" % (f, why))
        return 1
    print("\nAll %d files: no crashes, no hangs, no stuck notes, no clipping, "
          "deterministic." % len(files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
