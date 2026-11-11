"""Build a small, baked landmark from Quaternius CC0 modules (no runtime assembly).

Usage: python PrepareTransitLandmark.py <extracted Modular SciFi MegaKit directory>
Only used base-color textures are copied. Geometry is batched by material so the
number of source modules does not become the number of gameplay draw calls.
"""
import argparse
import json
import math
from pathlib import Path
import shutil
import struct


def rotate(v, angles):
    x, y, z = v
    for axis, angle in enumerate(angles):
        c, s = math.cos(angle), math.sin(angle)
        if axis == 0:
            y, z = y * c - z * s, y * s + z * c
        elif axis == 1:
            x, z = x * c + z * s, -x * s + z * c
        else:
            x, y = x * c - y * s, x * s + y * c
    return x, y, z


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    output = Path(__file__).resolve().parents[1] / "resources/free_models/transit_landmark"
    output.mkdir(parents=True, exist_ok=True)
    groups, cached, textures = {}, {}, set()

    def load(name):
        if name in cached:
            return cached[name]
        matches = list((args.source / "glTF").rglob(name + ".gltf"))
        assert len(matches) == 1, name
        path = matches[0]
        data = json.loads(path.read_text(encoding="utf-8"))
        assert all(set(n) <= {"mesh", "name"} for n in data["nodes"])
        buffers = [(path.parent / b["uri"]).read_bytes() for b in data["buffers"]]

        def read(index):
            a = data["accessors"][index]
            view = data["bufferViews"][a["bufferView"]]
            count = {"SCALAR": 1, "VEC2": 2, "VEC3": 3}[a["type"]]
            fmt = "<" + {5123: "H", 5125: "I", 5126: "f"}[a["componentType"]] * count
            stride = view.get("byteStride", struct.calcsize(fmt))
            offset = view.get("byteOffset", 0) + a.get("byteOffset", 0)
            return [struct.unpack_from(fmt, buffers[view["buffer"]], offset + i * stride)
                    for i in range(a["count"])]

        parts = []
        for mesh in data["meshes"]:
            for p in mesh["primitives"]:
                attributes = p["attributes"]
                material = data["materials"][p["material"]]
                color_texture = material["pbrMetallicRoughness"]["baseColorTexture"]["index"]
                uri = data["images"][data["textures"][color_texture]["source"]]["uri"]
                parts.append((read(attributes["POSITION"]), read(attributes["NORMAL"]),
                              read(attributes["TEXCOORD_0"]), read(p["indices"]), uri))
        vertices = [v for part in parts for v in part[0]]
        center = [(min(v[i] for v in vertices) + max(v[i] for v in vertices)) / 2 for i in range(3)]
        cached[name] = parts, center
        return cached[name]

    def add(name, position, scale, angles=(0, 0, 0), accent=False):
        assert all(s > 0 for s in scale), "Mirrored geometry needs winding correction"
        parts, center = load(name)
        for positions, normals, uvs, indices, texture in parts:
            if not accent:
                texture = texture.replace("_BaseColor_Red", "_BaseColor")
            textures.add(texture)
            g = groups.setdefault(texture, {"p": [], "n": [], "uv": [], "i": []})
            offset = len(g["p"])
            for p, n, uv in zip(positions, normals, uvs):
                v = rotate([(p[i] - center[i]) * scale[i] for i in range(3)], angles)
                g["p"].append(tuple(v[i] + position[i] for i in range(3)))
                v = rotate([n[i] / scale[i] for i in range(3)], angles)
                length = math.sqrt(sum(c * c for c in v))
                g["n"].append(tuple(c / length for c in v))
                g["uv"].append(uv)
            g["i"].extend((i[0] + offset,) for i in indices)

    half_pi = math.pi / 2
    # Four load-bearing piers: clear flight envelope is |x| < 22 m, y < 17 m.
    for x in (-27, 27):
        for z in (-6, 6):
            add("Column_Large_Straight", (x, 12.5, z), (3.4, 2.5, 2.4))

    # One enclosed elevated service link, not a repeated sequence of floating gates.
    for x in (-30, -18, -6, 6, 18, 30):
        for z, yaw in ((-8, half_pi), (8, -half_pi)):
            add("WallAstra_Straight", (x, 23.0, z), (1.3, 1.5, 3), (0, yaw, 0), accent=True)
            add("Column_MetalSupport", (x, 19.0, z), (2.2, 32, 3), (half_pi, 0, half_pi))
        add("Platform_Metal2", (x, 20.5, 0), (3, 1, 4))
        add("Platform_Metal2", (x, 20.45, 0), (3, 1, 4), (math.pi, 0, 0))
        add("Platform_Metal2", (x, 25.3, 0), (3, 1, 4))

    # Low service walls establish the approach without filling the combat screen.
    for side in (-1, 1):
        for z in (-42, -22, 22, 42):
            add("WallAstra_Straight", (side * 28.5, 2.2, z), (1.8, 1.45, 5),
                (0, 0 if side < 0 else math.pi, 0))
        for z in (-52, 52):
            add("Column_Hollow", (side * 29, 7.0, z), (2.8, 2.8, 2.8), accent=True)

    # Validate entire triangles, including a face spanning between two outside vertices.
    for g in groups.values():
        assert len(g["i"]) % 3 == 0
        for start in range(0, len(g["i"]), 3):
            triangle = [g["p"][i[0]] for i in g["i"][start:start + 3]]
            overlaps_flight_x = min(v[0] for v in triangle) < 15 and max(v[0] for v in triangle) > -15
            assert not (overlaps_flight_x and min(v[1] for v in triangle) < 16)

    document = {"asset": {"version": "2.0", "generator": "SKYBREAK PrepareTransitLandmark.py"},
                "scene": 0, "scenes": [{"nodes": [0]}],
                "nodes": [{"name": "TransitLandmark", "mesh": 0}],
                "meshes": [{"primitives": []}], "materials": [], "images": [],
                "textures": [], "samplers": [{"magFilter": 9729, "minFilter": 9987,
                                               "wrapS": 10497, "wrapT": 10497}],
                "accessors": [], "bufferViews": []}
    binary = bytearray()

    def accessor(values, kind, component=5126):
        while len(binary) % 4:
            binary.append(0)
        offset = len(binary)
        fmt = "<" + ("f" if component == 5126 else "I") * len(values[0])
        for value in values:
            binary.extend(struct.pack(fmt, *value))
        view = len(document["bufferViews"])
        document["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": len(binary) - offset})
        index = len(document["accessors"])
        document["accessors"].append({"bufferView": view, "componentType": component,
                                     "count": len(values), "type": kind,
                                     "min": [min(v[i] for v in values) for i in range(len(values[0]))],
                                     "max": [max(v[i] for v in values) for i in range(len(values[0]))]})
        return index

    for texture, g in sorted(groups.items()):
        index = len(document["materials"])
        document["images"].append({"uri": texture})
        document["textures"].append({"sampler": 0, "source": index})
        document["materials"].append({"name": Path(texture).stem, "pbrMetallicRoughness": {
            "baseColorTexture": {"index": index}, "metallicFactor": 0.22, "roughnessFactor": 0.62}})
        document["meshes"][0]["primitives"].append({
            "attributes": {"POSITION": accessor(g["p"], "VEC3"),
                           "NORMAL": accessor(g["n"], "VEC3"),
                           "TEXCOORD_0": accessor(g["uv"], "VEC2")},
            "indices": accessor(g["i"], "SCALAR", 5125), "material": index})
    document["buffers"] = [{"uri": "TransitLandmark.bin", "byteLength": len(binary)}]
    (output / "TransitLandmark.bin").write_bytes(binary)
    (output / "TransitLandmark.gltf").write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    for texture in sorted(textures):
        shutil.copyfile(args.source / "Textures" / texture, output / texture)
    shutil.copyfile(args.source / "License_Standard.txt", output / "License_Standard.txt")
    vertices = [v for g in groups.values() for v in g["p"]]
    print(f"Landmark: {len(vertices)} vertices, {len(groups)} material batches, {len(textures)} textures")
    print("Bounds:", [min(v[i] for v in vertices) for i in range(3)],
          [max(v[i] for v in vertices) for i in range(3)])


if __name__ == "__main__":
    main()
