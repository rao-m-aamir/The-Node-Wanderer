"""Measures every file in assets/audio: length, peak, loudness, and (for loops) the seam.

A loop's seam is checked two ways: the jump between its last and first sample compared with the
largest ordinary sample-to-sample step, and the spectral change across the wrap compared with the
median change between neighbouring 50 ms windows elsewhere. A clean loop has both close to normal.

    python tools/audio/check_audio.py [--loops-only]
"""
import os
import sys

import numpy as np
import pyloudnorm as pyln
import soundfile as sf

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "assets", "audio")


def spectrum(x):
    return np.log10(np.abs(np.fft.rfft(x * np.hanning(len(x)))) + 1e-9)


def seam(data, rate):
    """Plays the end of the loop straight into its start and compares the spectral change at the
    join with the changes in the 3 seconds around it, using only bands that carry real energy."""
    mono = data.mean(axis=1)
    steps = np.abs(np.diff(data, axis=0)).max(axis=1)
    wrap = np.abs(data[0] - data[-1]).max()
    span = int(3.0 * rate)
    joined = np.concatenate([mono[-span:], mono[:span]])
    n, hop = int(0.05 * rate), int(0.01 * rate)
    longterm = spectrum(mono[:min(len(mono), 30 * rate)][:2 ** 20])
    freqs = np.fft.rfftfreq(n, 1.0 / rate)
    lt_freqs = np.fft.rfftfreq(min(len(mono), 30 * rate, 2 ** 20), 1.0 / rate)
    energy = np.interp(freqs, lt_freqs, longterm)
    keep = energy > energy.max() - 3.0  # within 60 dB of the strongest band
    changes, at_join = [], None
    for i in range(0, len(joined) - 2 * n, hop):
        c = np.mean(np.abs(spectrum(joined[i:i + n])[keep] - spectrum(joined[i + n:i + 2 * n])[keep]))
        if abs(i + n - span) < hop:
            at_join = c
        elif abs(i + n - span) > 6 * hop:
            changes.append(c)
    changes = np.array(changes)
    return wrap, np.percentile(steps, 99.99), at_join, np.median(changes), np.percentile(changes, 98)


def main():
    loops_only = "--loops-only" in sys.argv
    worst = []
    for folder, _, files in sorted(os.walk(ROOT)):
        for name in sorted(files):
            if not name.endswith(".ogg"):
                continue
            path = os.path.join(folder, name)
            rel = os.path.relpath(path, ROOT).replace("\\", "/")
            is_loop = rel.startswith(("music/", "ambience/"))
            if loops_only and not is_loop:
                continue
            data, rate = sf.read(path, always_2d=True)
            peak = 20 * np.log10(np.max(np.abs(data)) + 1e-12)
            meter = pyln.Meter(rate)
            padded = data if len(data) >= int(0.5 * rate) else np.concatenate([data, np.zeros((int(0.5 * rate) - len(data), data.shape[1]))])
            loud = meter.integrated_loudness(padded)
            line = f"{rel:<28} {len(data) / rate:7.2f}s ch{data.shape[1]}  peak {peak:6.1f} dBFS  {loud:6.1f} LUFS"
            if is_loop:
                wrap, step, across, median, p98 = seam(data, rate)
                ok = wrap <= step * 1.05 and across <= p98
                line += f"  seam: jump {wrap:.4f} (max step {step:.4f})  change at join {across:.3f} (nearby median {median:.3f}, p98 {p98:.3f})  {'OK' if ok else 'CHECK'}"
                if not ok:
                    worst.append(rel)
            if peak > -0.5:
                worst.append(rel + " (peak)")
            print(line)
    print("needs a look: " + (", ".join(worst) if worst else "none"))


if __name__ == "__main__":
    main()
