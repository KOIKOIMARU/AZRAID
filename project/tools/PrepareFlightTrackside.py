"""Generate a texture-free, 36 m roadside module for SKYBREAK.

Nine shared instances form one 324 m loop. All vertices stay outside the flight
corridor; this is visual roadside infrastructure, not a collision obstacle.
No downloaded material or new raster texture is required.
"""
import json
from pathlib import Path
import struct


def main():
    output = Path(__file__).resolve().parents[1] / "resources/scenery"
    output.mkdir(parents=True, exist_ok=True)
    colors = [(0.27, 0.30, 0.33, 1), (0.68, 0.70, 0.71, 1), (0.95, 0.65, 0.20, 1)]
    groups = [{"p": [], "n": [], "i": []} for _ in colors]

    def box(center, size, material):
        g = groups[material]
        # Counter-clockwise faces, separate vertices for hard surface normals.
        for normal, u, v in [
            ((1, 0, 0), (0, 1, 0), (0, 0, 1)),
            ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
            ((0, 1, 0), (0, 0, 1), (1, 0, 0)),
            ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
            ((0, 0, 1), (1, 0, 0), (0, 1, 0)),
            ((0, 0, -1), (0, 1, 0), (1, 0, 0)),
        ]:
            start = len(g["p"])
            for a, b in [(-1, -1), (1, -1), (1, 1), (-1, 1)]:
                g["p"].append(tuple(center[j] + (normal[j] + a * u[j] + b * v[j]) * size[j] / 2 for j in range(3)))
                g["n"].append(normal)
            g["i"].extend((start + i,) for i in (0, 1, 2, 0, 2, 3))

    for side in (-1, 1):
        x = side * 13.3
        # Long low rails and regularly spaced supports give real perspective flow.
        for z in (-9, 9):
            box((x, 0.86, z), (0.32, 0.26, 16.8), 1)
            box((x, 0.43, z), (0.22, 0.16, 16.8), 0)
        for z in (-15, -9, -3, 3, 9, 15):
            box((x, 0.10, z), (0.9, 0.20, 0.9), 0)
            box((x, 0.52, z), (0.28, 1.04, 0.34), 0)
            box((x - side * 0.19, 0.91, z), (0.05, 0.16, 0.48), 2)
            box((side * 11.9, 0.09, z), (0.16, 0.035, 2.9), 1)
        # A slender outer mast crosses the screen edge, never the aiming area.
        z = side * 8.0
        box((side * 17.0, 0.18, z), (1.0, 0.36, 1.0), 0)
        box((side * 17.0, 3.15, z), (0.24, 6.0, 0.24), 0)
        box((side * 16.4, 6.14, z), (1.4, 0.22, 0.34), 1)
        box((side * 16.12, 6.02, z), (0.75, 0.05, 0.25), 2)

    vertices = [p for g in groups for p in g["p"]]
    assert all(abs(p[0]) > 11.5 for p in vertices)
    assert all(abs(p[2]) <= 18 for p in vertices)
    document = {"asset": {"version": "2.0", "generator": "SKYBREAK PrepareFlightTrackside.py"},
                "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0}],
                "meshes": [{"primitives": []}], "materials": [], "accessors": [], "bufferViews": []}
    binary = bytearray()

    def accessor(values, kind, indices=False):
        while len(binary) % 4:
            binary.append(0)
        start = len(binary)
        fmt = "<" + ("I" if indices else "f") * len(values[0])
        for value in values:
            binary.extend(struct.pack(fmt, *value))
        view = len(document["bufferViews"])
        document["bufferViews"].append({"buffer": 0, "byteOffset": start, "byteLength": len(binary) - start})
        index = len(document["accessors"])
        document["accessors"].append({"bufferView": view, "componentType": 5125 if indices else 5126,
            "count": len(values), "type": kind,
            "min": [min(v[i] for v in values) for i in range(len(values[0]))],
            "max": [max(v[i] for v in values) for i in range(len(values[0]))]})
        return index

    for index, g in enumerate(groups):
        document["materials"].append({"name": ("dark_metal", "painted_metal", "amber_reflector")[index],
            "pbrMetallicRoughness": {"baseColorFactor": colors[index], "metallicFactor": 0.1, "roughnessFactor": 0.7}})
        document["meshes"][0]["primitives"].append({"material": index,
            "attributes": {"POSITION": accessor(g["p"], "VEC3"), "NORMAL": accessor(g["n"], "VEC3")},
            "indices": accessor(g["i"], "SCALAR", True)})
    document["buffers"] = [{"uri": "FlightTrackside.bin", "byteLength": len(binary)}]
    (output / "FlightTrackside.bin").write_bytes(binary)
    (output / "FlightTrackside.gltf").write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(f"Trackside: {len(vertices)} vertices, 3 material batches, {len(binary)} bytes, flight corridor clear")


if __name__ == "__main__":
    main()
