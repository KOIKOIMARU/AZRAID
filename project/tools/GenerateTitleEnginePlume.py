"""Generate the original AZRAID title jet; no third-party source imagery.

UV bottom is the nozzle. The white alpha mask is tinted by the two Object3d
layers in TitleScene. Regenerate from the repository root with this script.
"""

import math
from pathlib import Path

from PIL import Image


def build() -> None:
    width, height = 256, 512
    image = Image.new("RGBA", (width, height))
    pixels = image.load()
    for y in range(height):
        distance = 1.0 - (y + 0.5) / height
        # Connected, dense throat; contracting jet with four shock cells.
        radius = 0.34 * (1.0 - distance) ** 0.72
        radius *= 0.87 + 0.13 * math.cos(distance * math.tau * 4.0)
        radius = max(radius, 0.004)
        axial = (1.0 - distance) ** 0.48
        axial *= min(1.0, (1.0 - distance) / 0.055)
        for x in range(width):
            across = (x + 0.5) / width - 0.5
            core = math.exp(-((across / (radius * 0.40)) ** 2))
            body = math.exp(-((across / radius) ** 4))
            halo = math.exp(-((across / (radius * 1.4)) ** 2))
            cells = 0.0
            for center in (0.16, 0.35, 0.55, 0.75):
                diamond = abs(distance - center) / 0.08 + abs(across) / (radius * 0.55)
                cells = max(cells, math.exp(-((diamond / 0.65) ** 4)))
            throat = math.exp(-((distance / 0.09) ** 2))
            alpha = min(1.0, (body * 0.30 + core * 0.36 + cells * 0.40 + halo * 0.10 + throat * body * 0.40) * axial)
            pixels[x, y] = (255, 255, 255, round(alpha * 255))
    destination = Path(__file__).resolve().parents[1] / "resources/effects/title_engine_plume.png"
    destination.parent.mkdir(parents=True, exist_ok=True)
    image.save(destination, optimize=True)
    print(destination)


if __name__ == "__main__":
    build()
