#!/usr/bin/env python3
"""Generate synthetic click-track WAV files used as the ground-truth accuracy
benchmark for bpmcore (see bpm-app-plan-v2.md, section 3).

Each track is a metronome at an exact, known BPM, so the correct answer is
known precisely (unlike real-track metadata, which is often rounded/wrong).
For each BPM we generate:
  - a clean click track
  - a version with additive noise
  - a version with a light synthetic reverb (short exponential IR convolution)

Usage:
    python3 generate_click_tracks.py [--out-dir DIR] [--duration SECONDS]
"""

import argparse
import math
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 44100
BPMS = [120.00, 128.00, 95.00, 174.00]
DEFAULT_DURATION_SEC = 20.0
CLICK_DURATION_SEC = 0.015  # short percussive click
CLICK_FREQ_HZ = 1000.0
NOISE_AMPLITUDE = 0.05
REVERB_DECAY_SEC = 0.12
REVERB_MIX = 0.35


def _lcg_random(seed: int):
    """Tiny deterministic PRNG (no numpy dependency) for reproducible noise."""
    state = seed & 0xFFFFFFFF
    while True:
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        yield (state / 0x7FFFFFFF) * 2.0 - 1.0


def generate_click_track(bpm: float, duration_sec: float) -> list:
    n_samples = int(duration_sec * SAMPLE_RATE)
    samples = [0.0] * n_samples

    period_sec = 60.0 / bpm
    click_len = int(CLICK_DURATION_SEC * SAMPLE_RATE)

    t = 0.0
    while t < duration_sec:
        start = int(t * SAMPLE_RATE)
        for i in range(click_len):
            idx = start + i
            if idx >= n_samples:
                break
            # Exponentially-decaying tone burst: a clean, sharp transient.
            envelope = (1.0 - i / click_len) ** 2
            phase = 2.0 * math.pi * CLICK_FREQ_HZ * (i / SAMPLE_RATE)
            samples[idx] = envelope * math.sin(phase)
        t += period_sec

    return samples


def add_noise(samples: list, amplitude: float, seed: int) -> list:
    rng = _lcg_random(seed)
    return [s + amplitude * next(rng) for s in samples]


def apply_synthetic_reverb(samples: list, decay_sec: float, mix: float) -> list:
    ir_len = int(decay_sec * SAMPLE_RATE)
    ir = [pow(0.001, i / ir_len) for i in range(ir_len)]  # exponential decay to -60dB

    # Simple FIR convolution via sparse impulses (clicks are sparse, so this
    # stays fast enough without needing FFT-based convolution here).
    n = len(samples)
    wet = [0.0] * n
    for i, s in enumerate(samples):
        if s == 0.0:
            continue
        limit = min(ir_len, n - i)
        for k in range(limit):
            wet[i + k] += s * ir[k]

    return [(1.0 - mix) * dry + mix * w for dry, w in zip(samples, wet)]


def normalize(samples: list, peak: float = 0.9) -> list:
    max_abs = max((abs(s) for s in samples), default=0.0)
    if max_abs <= 0.0:
        return samples
    scale = peak / max_abs
    return [s * scale for s in samples]


def write_wav(path: Path, samples: list) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)  # 16-bit PCM
        wf.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for s in samples:
            clamped = max(-1.0, min(1.0, s))
            frames += struct.pack("<h", int(clamped * 32767.0))
        wf.writeframes(bytes(frames))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, default=Path(__file__).resolve().parent.parent / "core" / "tests" / "data" / "synthetic")
    parser.add_argument("--duration", type=float, default=DEFAULT_DURATION_SEC)
    args = parser.parse_args()

    for bpm in BPMS:
        clean = generate_click_track(bpm, args.duration)

        bpm_tag = f"{bpm:.2f}".replace(".", "_")

        clean_norm = normalize(clean)
        write_wav(args.out_dir / f"click_{bpm_tag}_clean.wav", clean_norm)

        noisy = normalize(add_noise(clean, NOISE_AMPLITUDE, seed=int(bpm * 100)))
        write_wav(args.out_dir / f"click_{bpm_tag}_noisy.wav", noisy)

        reverb = normalize(apply_synthetic_reverb(clean, REVERB_DECAY_SEC, REVERB_MIX))
        write_wav(args.out_dir / f"click_{bpm_tag}_reverb.wav", reverb)

        print(f"generated {bpm:.2f} BPM: clean, noisy, reverb")

    print(f"done: {len(BPMS) * 3} files written to {args.out_dir}")


if __name__ == "__main__":
    main()
