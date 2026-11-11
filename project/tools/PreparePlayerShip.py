"""QuaterniusのOmenを、自機用サイズ・外部DDS参照へ変換する。

元データは変更しない。頂点の縦横比を保ち、埋め込み画像をランタイムから除く。
Usage: python PreparePlayerShip.py <download-dir> <output-dir>
"""
import base64
import copy
import json
import struct
import sys
from pathlib import Path


def convert(source: Path, output: Path, name: str) -> None:
    model = json.loads((source / f"{name}.gltf").read_text(encoding="utf-8"))
    assert len(model["nodes"]) == 1 and "mesh" in model["nodes"][0]
    assert not model.get("animations") and not model.get("skins")
    raw = base64.b64decode(model["buffers"][0]["uri"].split(",", 1)[1])
    position_ids = {p["attributes"]["POSITION"] for m in model["meshes"] for p in m["primitives"]}
    mins = [min(model["accessors"][i]["min"][a] for i in position_ids) for a in range(3)]
    maxs = [max(model["accessors"][i]["max"][a] for i in position_ids) for a in range(3)]
    center = [(lo + hi) / 2 for lo, hi in zip(mins, maxs)]
    # Playerの既存スケール1.26を掛けた後、全長4.2以下・全幅3.6以下に収める。
    scale = min(4.2 / (maxs[2] - mins[2]), 3.6 / (maxs[0] - mins[0])) / 1.26
    raw = bytearray(raw)
    for i in position_ids:
        accessor = model["accessors"][i]
        assert accessor["componentType"] == 5126 and accessor["type"] == "VEC3"
        view = model["bufferViews"][accessor["bufferView"]]
        start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        for vertex in range(accessor["count"]):
            offset = start + vertex * view.get("byteStride", 12)
            position = struct.unpack_from("<3f", raw, offset)
            struct.pack_into("<3f", raw, offset, *[(position[a] - center[a]) * scale for a in range(3)])
        accessor["min"] = [(accessor["min"][a] - center[a]) * scale for a in range(3)]
        accessor["max"] = [(accessor["max"][a] - center[a]) * scale for a in range(3)]
    # アクセサが使う頂点・索引だけを再梱包。PNGを含む旧バッファは持ち込まない。
    used_views = sorted({a["bufferView"] for a in model["accessors"]})
    result = bytearray()
    views = []
    remap = {}
    for old in used_views:
        view = copy.deepcopy(model["bufferViews"][old])
        result.extend(bytes((-len(result)) % 4))
        old_start = view.get("byteOffset", 0)
        view["byteOffset"] = len(result)
        result.extend(raw[old_start:old_start + view["byteLength"]])
        remap[old] = len(views)
        views.append(view)
    for accessor in model["accessors"]:
        accessor["bufferView"] = remap[accessor["bufferView"]]
    model["bufferViews"] = views
    model["buffers"] = [{"byteLength": len(result), "uri": f"{name}.bin"}]
    model["images"] = [{"uri": f"{name}_Blue.dds"}]
    model["textures"] = [{"source": 0}]
    for material in model["materials"]:
        material["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
    output.mkdir(parents=True, exist_ok=True)
    (output / f"{name}.bin").write_bytes(result)
    (output / f"{name}.gltf").write_text(json.dumps(model, indent=2) + "\n", encoding="utf-8")
    print(name, "world-scale", scale * 1.26, "source-center", center, "mesh-bytes", len(result))


if __name__ == "__main__":
    convert(Path(sys.argv[1]), Path(sys.argv[2]), "Omen")
