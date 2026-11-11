"""Convert the CC0 JRPG Pack 5 source OGGs to bounded PCM loops.

Usage: python PrepareCombatMusic.py <extracted-source-directory>
Requires numpy and soundfile only for this offline asset conversion.
"""
import argparse
from pathlib import Path

import numpy as np
import soundfile as sf


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    output = Path(__file__).resolve().parents[1] / "resources/audio/music"
    output.mkdir(parents=True, exist_ok=True)
    tracks = {
        "Action3 - Preparing For Battle.ogg": "stage.wav",
        "Action2 - Army Approaching.ogg": "boss.wav",
        "Action1 - Encounter With The Witches.ogg": "fever.wav",
    }
    for original, filename in tracks.items():
        samples, rate = sf.read(args.source / original, always_2d=True, dtype="float32")
        assert samples.shape[1] == 2 and np.isfinite(samples).all()
        # Keep the composer's complete musical loop; only prevent a boundary click.
        edge = min(int(rate * 0.004), len(samples) // 2)
        samples[:edge] *= np.linspace(0.0, 1.0, edge)[:, None]
        samples[-edge:] *= np.linspace(1.0, 0.0, edge)[:, None]
        peak = float(np.max(np.abs(samples)))
        samples *= min(1.0, 0.707 / max(peak, 1e-9))
        assert samples.nbytes / 2 < 32 * 1024 * 1024
        sf.write(output / filename, samples, rate, subtype="PCM_16")
        rms = float(np.sqrt(np.mean(samples * samples)))
        print(f"{filename}: {len(samples) / rate:.3f}s, {rate}Hz stereo, "
              f"peak={np.max(np.abs(samples)):.4f}, rms={rms:.4f}")


if __name__ == "__main__":
    main()
