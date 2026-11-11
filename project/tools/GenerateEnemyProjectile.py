"""Generate original AZRAID enemy projectile textures without source imagery.

The alpha-blended head contains both the dark silhouette and the dense hot
core. A separate additive layer supplies the directional wake. Run anywhere.
"""

import math
from pathlib import Path

from PIL import Image


def mix(a, b, t):
    t = min(1.0, max(0.0, t))
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def build():
    output = Path(__file__).resolve().parents[1] / "resources" / "effects"
    core = Image.new("RGBA", (256, 256))
    pixels = core.load()
    for y in range(256):
        for x in range(256):
            u, v = (x + 0.5) / 256 - 0.5, (y + 0.5) / 256 - 0.5
            # A slightly faceted head, with a solid interior rather than a ring.
            radius = (abs(u) ** 2.6 + abs(v) ** 2.6) ** (1 / 2.6)
            hot = (255, 244, 219)
            red = (255, 52, 24)
            rim = (116, 8, 16)
            color = mix(hot, red, (radius - 0.11) / 0.19)
            color = mix(color, rim, (radius - 0.31) / 0.09)
            if radius <= 0.40:
                alpha = 1.0
            else:
                alpha = 0.25 * math.exp(-((radius - 0.40) / 0.033) ** 2)
            pixels[x, y] = (*color, round(alpha * 255))
    core.save(output / "azraid_enemy_core.png")

    tail = Image.new("RGBA", (96, 384))
    pixels = tail.load()
    for y in range(384):
        distance = (y + 0.5) / 384
        width = 0.19 * (1 - distance) ** 1.15 + 0.002
        for x in range(96):
            across = abs((x + 0.5) / 96 - 0.5)
            strand = math.exp(-((across / (width * 0.35)) ** 2))
            envelope = math.exp(-((across / width) ** 4))
            color = mix((231, 24, 16), (255, 224, 191), strand * (1 - distance))
            alpha = envelope * (1 - distance) ** 1.6 * min(1.0, distance / 0.035)
            pixels[x, y] = (*color, round(alpha * 255))
    tail.save(output / "azraid_enemy_wake.png")


if __name__ == "__main__":
    build()
