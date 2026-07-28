#!/usr/bin/env python3
"""Loudness-normalize every VO wav in Content/Audio/VO to a common RMS level.

Different ElevenLabs voices render at very different loudness (AGEIS-7 comes out
much quieter than Rachel); this evens them out so in-game voice volume is one
constant. Peak-limited so nothing clips. Idempotent: run after every generate.
"""

import array
import math
import os
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "Content", "Audio", "VO")

TARGET_RMS_DB = -20.0   # speech target
PEAK_CEIL = 0.94        # ~-0.5 dBFS

for dirpath, _, files in os.walk(VO):
    for name in sorted(files):
        if not name.lower().endswith(".wav"):
            continue
        path = os.path.join(dirpath, name)
        with wave.open(path, "rb") as r:
            params = r.getparams()
            if params.sampwidth != 2:
                print(f"skip (not 16-bit): {name}")
                continue
            samples = array.array("h", r.readframes(params.nframes))

        if not samples:
            continue
        rms = math.sqrt(sum(s * s for s in samples) / len(samples))
        peak = max(1, max(abs(s) for s in samples))
        if rms <= 0:
            continue
        target = (10 ** (TARGET_RMS_DB / 20.0)) * 32768.0
        gain = target / rms
        gain = min(gain, PEAK_CEIL * 32767.0 / peak)   # never clip
        if abs(gain - 1.0) < 0.03:
            print(f"ok        {name} (gain {gain:.2f})")
            continue
        out = array.array("h", (max(-32768, min(32767, int(s * gain))) for s in samples))
        with wave.open(path, "wb") as w:
            w.setparams(params)
            w.writeframes(out.tobytes())
        print(f"normalized {name}: gain x{gain:.2f}")
