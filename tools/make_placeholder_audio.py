#!/usr/bin/env python3
"""Generate 12 placeholder bird-call clips as mono 8kHz 8-bit signed PCM.

These are synthesized warbles (NOT real recordings) so the project builds and
the audio path can be exercised end-to-end. Replace them with real Creative-
Commons recordings via tools/fetch_audio.sh (xeno-canto) before release.
"""
import math
import os
import struct

RATE = 8000
OUT = os.path.join(os.path.dirname(__file__), "..", "resources", "audio")

# (base_hz, warble_hz, warble_depth, chirps, dur_s) — loosely evocative per bird
SPECS = [
    (2200, 7, 400, 5, 1.4),   # 00 mockingbird - varied repeats
    (3000, 5, 300, 3, 1.1),   # 01 painted bunting
    (2600, 9, 250, 2, 1.0),   # 02 scissor-tailed flycatcher
    (2400, 6, 350, 4, 1.2),   # 03 vermilion flycatcher
    (1100, 3, 200, 2, 1.3),   # 04 screech-owl - low trill
    (2000, 8, 500, 3, 1.2),   # 05 carolina wren - loud
    (2100, 8, 450, 3, 1.2),   # 06 bewick's wren
    (2500, 4, 300, 4, 1.3),   # 07 robin - caroling
    (4000, 10, 200, 2, 1.0),  # 08 parula - buzzy rise
    (4200, 6, 250, 2, 1.0),   # 09 golden-cheeked warbler
    (1800, 5, 300, 2, 0.9),   # 10 blue jay - harsh
    (2800, 6, 400, 4, 1.2),   # 11 cardinal - clear whistle
]


def clip(base, warble, depth, chirps, dur):
    n = int(RATE * dur)
    seg = n // chirps
    out = bytearray()
    for i in range(n):
        c = i // seg           # which chirp
        p = (i % seg) / seg    # 0..1 within chirp
        env = math.sin(math.pi * p) ** 0.6            # per-chirp swell
        sweep = 1.0 + 0.25 * math.sin(2 * math.pi * (c + 1) * 0.5)
        f = base * sweep + depth * math.sin(2 * math.pi * warble * i / RATE)
        s = math.sin(2 * math.pi * f * i / RATE)
        # add a touch of harmonic for brightness
        s += 0.3 * math.sin(4 * math.pi * f * i / RATE)
        v = max(-127, min(127, int(100 * env * s)))
        out.append(struct.pack("b", v)[0])
    return bytes(out)


def main():
    os.makedirs(OUT, exist_ok=True)
    for idx, spec in enumerate(SPECS):
        data = clip(*spec)
        path = os.path.join(OUT, f"call_{idx:02d}.pcm")
        with open(path, "wb") as f:
            f.write(data)
        print(f"wrote {os.path.relpath(path)} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
