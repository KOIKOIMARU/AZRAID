"""Create the short air rush used when passing a boost gate. No sampled recording."""
from pathlib import Path
import math
import random
import struct
import wave

RATE = 44100
DURATION = 0.86
random.seed(711)
frames = bytearray()
slow = fast = phase = 0.0
for index in range(int(RATE * DURATION)):
    t = index / RATE
    noise = random.uniform(-1.0, 1.0)
    slow += (noise - slow) * 0.015
    fast += (noise - fast) * 0.32
    envelope = (1.0 - math.exp(-t / 0.016)) * math.exp(-t / 0.22)
    tail = max(0.0, 1.0 - t / DURATION) ** 2
    phase += 2.0 * math.pi * (105.0 + 135.0 * math.exp(-t / 0.075)) / RATE
    air = (fast - slow) * 1.8 + slow * 1.4
    pressure = math.sin(phase) * 0.11 * math.exp(-t / 0.11)
    value = math.tanh((air + pressure) * envelope * tail * 1.5) * 0.78
    spread = 0.05 * math.sin(t * 22.0) * tail
    left = int(max(-1.0, min(1.0, value * (1.0 + spread))) * 32767)
    right = int(max(-1.0, min(1.0, value * (1.0 - spread))) * 32767)
    frames.extend(struct.pack('<hh', left, right))
destination = Path(__file__).resolve().parents[1] / 'resources/audio/combat/gate_boost.wav'
with wave.open(str(destination), 'wb') as output:
    output.setparams((2, 2, RATE, 0, 'NONE', 'not compressed'))
    output.writeframes(frames)
print(destination)
