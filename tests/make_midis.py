#!/usr/bin/env python3
"""
Generates a corpus of Standard MIDI Files that stress MT-32 specific
behaviour: its 8 parts on MIDI channels 2-9, rhythm on channel 10, its
32-partial voice limit, and its SysEx map.

    python3 tests/make_midis.py OUTDIR
"""
import os, struct, sys

TPQ = 480


def vlq(n):
    out = bytearray([n & 0x7f]); n >>= 7
    while n:
        out.insert(0, (n & 0x7f) | 0x80); n >>= 7
    return bytes(out)


class Track:
    def __init__(self):
        self.ev = []

    def at(self, t, data):
        self.ev.append((int(t), bytes(data)))

    # MT-32: part N is MIDI channel N+1 (0-based ch 1..8); rhythm is ch 9.
    def on(self, t, ch, n, v=100):   self.at(t, [0x90 | ch, n & 0x7f, v & 0x7f])
    def off(self, t, ch, n):         self.at(t, [0x80 | ch, n & 0x7f, 0])
    def note(self, t, ch, n, v, d):  self.on(t, ch, n, v); self.off(t + d, ch, n)
    def prog(self, t, ch, p):        self.at(t, [0xC0 | ch, p & 0x7f])
    def cc(self, t, ch, c, v):       self.at(t, [0xB0 | ch, c & 0x7f, v & 0x7f])
    def bend(self, t, ch, val):      self.at(t, [0xE0 | ch, val & 0x7f, (val >> 7) & 0x7f])

    def sysex(self, t, addr, data):
        """Roland MT-32 SysEx with a correct Roland checksum.

        A Standard MIDI File encodes SysEx as F0 <vlq length> <bytes...>,
        where the length counts everything after it including the closing F7.
        Writing F0 followed directly by the data produces a file whose next
        byte (0x41, the Roland ID) is read as a 65-byte length, which then
        swallows the following events.
        """
        body = list(addr) + list(data)
        chk = (128 - (sum(body) & 0x7f)) & 0x7f
        payload = [0x41, 0x10, 0x16, 0x12] + body + [chk, 0xF7]
        self.at(t, [0xF0] + list(vlq(len(payload))) + payload)

    def display(self, t, text):
        self.sysex(t, [0x20, 0x00, 0x00], [ord(c) for c in text[:20].ljust(20)])

    def reset(self, t):
        self.sysex(t, [0x7F, 0x00, 0x00], [0x00])

    def write(self, path, tempo_us=500000):
        trk = bytearray()
        trk += vlq(0) + b'\xff\x51\x03' + struct.pack('>I', tempo_us)[1:]
        last = 0
        for t, d in sorted(self.ev, key=lambda e: e[0]):
            trk += vlq(t - last) + d; last = t
        trk += vlq(TPQ) + b'\xff\x2f\x00'
        hdr = b'MThd' + struct.pack('>IHHH', 6, 0, 1, TPQ)
        with open(path, 'wb') as f:
            f.write(hdr + b'MTrk' + struct.pack('>I', len(trk)) + bytes(trk))

    def balance(self):
        """on-minus-off per channel; all zero means nothing can hang."""
        bal = {}
        for _, d in self.ev:
            st = d[0]
            if (st & 0xf0) == 0x90 and d[2] > 0: bal[st & 0xf] = bal.get(st & 0xf, 0) + 1
            if (st & 0xf0) == 0x80:              bal[st & 0xf] = bal.get(st & 0xf, 0) - 1
        return bal


def build(outdir):
    made = []

    def emit(name, t, tempo=500000, note=""):
        p = os.path.join(outdir, name)
        t.write(p, tempo)
        made.append((name, note, t.balance()))

    # 1. Every one of the 128 timbres, one note each.
    t = Track(); t.display(0, " ALL 128 TIMBRES ")
    for i in range(128):
        st = i * (TPQ // 2)
        t.prog(st, 1, i); t.note(st + 10, 1, 60, 100, TPQ // 2 - 40)
    emit("01-all-timbres.mid", t, note="sweeps every patch 0-127")

    # 2. The whole rhythm map.
    t = Track(); t.display(0, "  RHYTHM MAP  ")
    for i, key in enumerate(range(24, 88)):
        t.note(i * (TPQ // 4), 9, key, 110, 60)
    emit("02-rhythm-map.mid", t, note="every rhythm key 24-87 on ch10")

    # 3. Polyphony past the 32-partial limit: forces voice stealing.
    t = Track(); t.display(0, " POLYPHONY STRESS ")
    for i in range(40):
        t.on(i * 40, 1 + (i % 8), 36 + i, 100)
    for i in range(40):
        t.off(TPQ * 6 + i * 20, 1 + (i % 8), 36 + i)
    emit("03-polyphony.mid", t, note="40 overlapping notes across 8 parts")

    # 4. All eight parts plus rhythm at once.
    t = Track(); t.display(0, " ALL PARTS ")
    for ch in range(1, 9):
        t.prog(0, ch, (ch * 13) % 128)
    for bar in range(8):
        b = bar * TPQ * 2
        for ch in range(1, 9):
            t.note(b, ch, 48 + ch * 3, 95, TPQ * 2 - 50)
        for k in range(4):
            t.note(b + k * (TPQ // 2), 9, 35 if k % 2 == 0 else 38, 105, 60)
    emit("04-all-parts.mid", t, note="8 melodic parts + rhythm simultaneously")

    # 5. Heavy SysEx: many display writes interleaved with notes.
    t = Track()
    for i in range(24):
        t.display(i * (TPQ // 2), ("SYSEX TEST %02d" % i).ljust(20))
        t.note(i * (TPQ // 2) + 20, 1, 60 + (i % 12), 100, TPQ // 3)
    emit("05-sysex-display.mid", t, note="24 display SysEx during playback")

    # 6. Reset SysEx mid-stream - must not hang or leave notes stuck.
    t = Track(); t.display(0, " RESET TEST ")
    for n in (60, 64, 67): t.on(TPQ, 1, n, 100)
    t.reset(TPQ * 3)
    for n in (60, 64, 67): t.off(TPQ * 4, 1, n)
    t.note(TPQ * 6, 1, 72, 100, TPQ)
    emit("06-sysex-reset.mid", t, note="full reset while notes are sounding")

    # 7. Controllers: bend, modulation, expression, pan, sustain.
    t = Track(); t.display(0, " CONTROLLERS ")
    t.prog(0, 1, 48)
    t.on(TPQ, 1, 60, 100)
    for i in range(32):
        t.bend(TPQ + i * 30, 1, 8192 + int(4000 * (1 if i % 2 else -1) * i / 32))
        t.cc(TPQ + i * 30, 1, 1, i * 4)       # modulation
        t.cc(TPQ + i * 30, 1, 11, 127 - i * 3)  # expression
        t.cc(TPQ + i * 30, 1, 10, i * 4)      # pan
    t.bend(TPQ * 5, 1, 8192)
    t.off(TPQ * 5 + 10, 1, 60)
    t.cc(TPQ * 6, 1, 64, 127)                 # sustain on
    t.note(TPQ * 6 + 10, 1, 64, 100, 60)
    t.cc(TPQ * 8, 1, 64, 0)                   # sustain off
    emit("07-controllers.mid", t, note="bend, mod, expression, pan, sustain")

    # 8. Range and velocity extremes.
    t = Track(); t.display(0, " EXTREMES ")
    for i, n in enumerate([0, 1, 12, 24, 36, 60, 84, 96, 108, 120, 126, 127]):
        t.note(i * (TPQ // 2), 1, n, 127, TPQ // 2 - 30)
    for i, v in enumerate([1, 2, 8, 32, 64, 100, 126, 127]):
        t.note(TPQ * 7 + i * (TPQ // 2), 1, 60, v, TPQ // 2 - 30)
    emit("08-extremes.mid", t, note="notes 0-127 and velocities 1-127")

    # 9. Very fast note churn - stresses the emulated UART.
    t = Track(); t.display(0, " RAPID CHURN ")
    for i in range(400):
        st = i * 12
        t.note(st, 1 + (i % 4), 48 + (i % 36), 90, 10)
    emit("09-rapid.mid", t, note="400 very short notes, UART stress")

    # 10. Rapid program changes while notes sound.
    t = Track(); t.display(0, " PROG CHANGE ")
    for i in range(60):
        st = i * (TPQ // 4)
        t.prog(st, 1, (i * 7) % 128)
        t.note(st + 5, 1, 60, 100, TPQ // 4 - 20)
    emit("10-program-changes.mid", t, note="60 program changes during notes")

    # 11. CC123 All Notes Off must release every part.
    t = Track(); t.display(0, " ALL NOTES OFF ")
    for n in (55, 59, 62, 67): t.on(TPQ, 1, n, 100)
    for n in (43, 47):         t.on(TPQ, 2, n, 100)
    t.cc(TPQ * 4, 1, 123, 0)
    t.cc(TPQ * 4, 2, 123, 0)
    # deliberately no explicit note-offs: CC123 must clear them
    emit("11-all-notes-off.mid", t, note="CC123 must release every part")

    # 16. CC120 All Sound Off is NOT in the MT-32's implementation chart - it
    # was added to the MIDI spec with General MIDI, three years after this
    # machine shipped. Ignoring it is period-correct, so this file is expected
    # to still be sounding at the end.
    t = Track(); t.display(0, " CC120 IGNORED ")
    for n in (55, 59, 62, 67): t.on(TPQ, 1, n, 100)
    t.cc(TPQ * 4, 1, 120, 0)
    emit("16-cc120-ignored.mid", t, note="CC120 unsupported: must KEEP sounding")

    # 12. Channel 1 is unassigned on the MT-32 and must stay silent.
    t = Track(); t.display(0, " CH1 MUST BE MUTE ")
    for i in range(16):
        t.note(i * (TPQ // 2), 0, 48 + i * 3, 120, TPQ // 2 - 20)
    emit("12-channel1-silent.mid", t, note="ch1 unassigned: expect silence")

    # 13. Long SysEx (timbre-area write) - exercises buffered SysEx.
    t = Track(); t.display(0, " LONG SYSEX ")
    payload = [(i * 7) & 0x7f for i in range(128)]
    t.sysex(TPQ, [0x08, 0x00, 0x00], payload)
    t.note(TPQ * 3, 1, 60, 100, TPQ)
    emit("13-long-sysex.mid", t, note="128-byte SysEx payload")

    # 14. Master volume sweep via SysEx.
    t = Track(); t.display(0, " MASTER VOLUME ")
    t.on(TPQ, 1, 60, 110)
    for i in range(20):
        t.sysex(TPQ + i * 60, [0x10, 0x00, 0x16], [max(0, 100 - i * 5)])
    t.off(TPQ * 5, 1, 60)
    t.sysex(TPQ * 5 + 10, [0x10, 0x00, 0x16], [100])
    emit("14-master-volume.mid", t, note="SysEx master volume sweep")

    # 15. Sustained chord held a long time - checks envelopes settle.
    t = Track(); t.display(0, " LONG SUSTAIN ")
    t.prog(0, 1, 48)
    for n in (48, 52, 55, 60, 64): t.on(TPQ, 1, n, 100)
    for n in (48, 52, 55, 60, 64): t.off(TPQ * 20, 1, n)
    emit("15-long-sustain.mid", t, note="5-note chord held ~10s")

    return made


def verify(path):
    """Re-parse a generated file the way mt32-render does and report the
    byte count, so encoding mistakes surface here rather than as mystery
    stuck notes later."""
    d = open(path, 'rb').read()
    if d[:4] != b'MThd':
        return None, "not an SMF"
    pos = d.index(b'MTrk') + 8
    total = 0
    run = 0

    def rvlq(p):
        v = 0
        while True:
            b = d[p]; p += 1; v = (v << 7) | (b & 0x7f)
            if not b & 0x80:
                return v, p

    while pos < len(d) - 3:
        _, pos = rvlq(pos)
        b = d[pos]
        if b == 0xff:
            pos += 1; t = d[pos]; pos += 1
            L, pos = rvlq(pos); pos += L
            if t == 0x2f:
                break
            continue
        if b in (0xf0, 0xf7):
            pos += 1
            L, pos = rvlq(pos)
            total += 1 + L
            pos += L
            continue
        if b & 0x80:
            run = b; pos += 1
        n = 1 if (run & 0xf0) in (0xc0, 0xd0) else 2
        pos += n
        total += 1 + n
    return total, None


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "tests/midi"
    os.makedirs(outdir, exist_ok=True)
    made = build(outdir)
    print("Generated %d files in %s\n" % (len(made), outdir))
    bad = 0
    for name, note, bal in made:
        nbytes, perr = verify(os.path.join(outdir, name))
        if perr:
            print("  %-26s PARSE ERROR: %s" % (name, perr)); bad += 1; continue
        # 11 and 12 are deliberately unbalanced; everything else must be clean.
        expect_unbalanced = name.startswith(("11-", "12-", "16-"))
        ok = all(v == 0 for v in bal.values()) or expect_unbalanced
        if not ok: bad += 1
        print("  %-26s %-40s %6d bytes %s"
              % (name, note, nbytes, "" if ok else "UNBALANCED " + str(bal)))
    if bad:
        print("\n%d file(s) have unintended hanging notes" % bad)
        return 1
    print("\nAll note-on/note-off pairs balanced (except the two that test "
          "controller-driven release).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
