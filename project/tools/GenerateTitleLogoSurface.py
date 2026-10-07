"""Generate AZRAID's continuous logo lighting from original math, with no source art.

TitleScene maps every stroke into the same texture in screen coordinates. Color
is sampled per pixel instead of interpolating different gradients on each stroke.
The horizontal span is -2048..2048 title pixels relative to the traveling light;
the vertical span is the complete front-face gradient. No external assets or
Python packages are required. Run this script from any working directory.
"""

import math
from pathlib import Path
import struct
import zlib


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def build() -> None:
    width, height = 2048, 256
    top = (246.0, 245.0, 240.0)
    bottom = (60.0, 80.0, 114.0)
    shadow = (53.0, 72.0, 106.0)
    highlight = (255.0, 253.0, 246.0)
    light = []
    for x in range(width):
        distance = ((x + 0.5) / width - 0.5) * 4096.0
        light.append((math.exp(-(distance / 135.0) ** 2) * 0.68,
                      math.exp(-((distance + 155.0) / 180.0) ** 2) * 0.25))
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # PNG row filter: none.
        v = (y + 0.5) / height
        base = [top[i] + (bottom[i] - top[i]) * v * 0.90 for i in range(3)]
        for sheen, dark in light:
            for i in range(3):
                color = base[i] + (shadow[i] - base[i]) * dark
                color += (highlight[i] - color) * sheen
                raw.append(round(color))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    destination = Path(__file__).resolve().parents[1] / "resources/effects/title_logo_surface.png"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(png)
    print(destination)


if __name__ == "__main__":
    build()
