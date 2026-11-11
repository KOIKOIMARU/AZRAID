"""Create the original short air-cut / confirmation sound used by a just dodge."""

from pathlib import Path
import math
import random
import struct
import wave


def generate(destination: Path) -> None:
    rate = 44100
    rng = random.Random(0xD0D6E)
    samples = []
    low_noise = 0.0
    phase = 0.0
    for index in range(int(rate * 0.52)):
        time = index / rate
        attack = min(time / 0.003, 1.0)
        noise = rng.uniform(-1.0, 1.0)
        low_noise += (noise - low_noise) * 0.10
        air = (noise - low_noise) * math.exp(-time / 0.047) * 0.40
        phase += math.tau * (1280.0 - 560.0 * min(time / 0.22, 1.0)) / rate
        chime = (math.sin(phase) + 0.24 * math.sin(phase * 1.5)) * math.exp(-time / 0.105) * 0.23
        body = math.sin(math.tau * (106.0 * time - 145.0 * time * time)) * math.exp(-time / 0.035) * 0.15
        tail = min((0.52 - time) / 0.05, 1.0)
        samples.append((air + chime + body) * attack * tail)
    gain = 0.72 / max(abs(value) for value in samples)
    pcm = bytearray()
    for index, value in enumerate(samples):
        right = samples[max(0, index - 11)] * 0.94
        pcm.extend(struct.pack('<hh', round(value * gain * 32767), round(right * gain * 32767)))
    destination.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(destination), 'wb') as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(pcm)


if __name__ == '__main__':
    generate(Path(__file__).resolve().parents[1] / 'resources/audio/combat/just_dodge.wav')
