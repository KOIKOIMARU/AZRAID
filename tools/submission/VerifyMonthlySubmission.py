"""Verify the hand-in snapshot and create an explicitly incomplete video draft ZIP."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
from urllib.parse import unquote
import xml.etree.ElementTree as ET
import zipfile

WORKSPACE = Path(__file__).resolve().parents[2]


def sha256(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--document", type=Path, required=True)
    parser.add_argument("--portfolio", type=Path, required=True)
    args = parser.parse_args()
    stage = args.stage.resolve()
    contents = stage / "提出内容"
    config = json.loads((stage / "submission.json").read_text(encoding="utf-8"))
    account = config["account"]
    individual = contents / "01_個人作品"
    source = individual / "01_プロジェクト一式" / "CG2_project"
    runtime = individual / "02_exeセット"
    docs = individual / "03_プログラム説明資料"
    document_name = account + "_プログラム説明資料.pdf"
    shutil.copy2(args.document, docs / document_name)
    shutil.copy2(args.document, stage / "ZIP外提出" / document_name)
    assert sha256(args.portfolio) == sha256(contents / "00_ポートフォリオ" / (account + "_ポートフォリオ.pdf"))

    checked = 0
    for folder in ("src", "shaders", "resources", "externals", "tools"):
        for file in (source / "project" / folder).rglob("*"):
            if not file.is_file():
                continue
            relative = file.relative_to(source / "project")
            assert "enc_temp_folder" not in relative.parts
            original = WORKSPACE / "project" / relative
            assert original.is_file(), relative
            assert sha256(original) == sha256(file), f"Source mismatch: {relative}"
            checked += 1
    for name in ("CG2.sln", "CG2.vcxproj", "CG2.vcxproj.filters", "CG2.rc", "resource.h"):
        assert sha256(WORKSPACE / "project" / name) == sha256(source / "project" / name)

    xml = ET.parse(source / "project" / "CG2.vcxproj")
    namespace = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
    for tag in ("ClCompile", "ClInclude", "ResourceCompile", "ProjectReference", "Text", "None"):
        for entry in xml.findall(".//m:" + tag, namespace):
            include = entry.get("Include")
            if include and "$" not in include:
                assert (source / "project" / include).is_file(), f"Missing project item: {include}"
    for file in (runtime / "resources").rglob("*.gltf"):
        model = json.loads(file.read_text(encoding="utf-8-sig"))
        for group in ("buffers", "images"):
            for entry in model.get(group, []):
                uri = entry.get("uri", "")
                if not uri or uri.startswith("data:"):
                    continue
                dependency = (file.parent / unquote(uri)).resolve()
                assert dependency.is_relative_to(runtime) and dependency.is_file(), dependency
    assert sha256(WORKSPACE / "generated/outputs/Release/CG2.exe") == sha256(runtime / "AZRAID.exe")
    for dll in ("dxcompiler.dll", "dxil.dll"):
        assert sha256(WORKSPACE / "generated/outputs/Release" / dll) == sha256(runtime / dll)

    files = sorted(file for file in contents.rglob("*") if file.is_file())
    forbidden = {".git", ".vs", "enc_temp_folder", "__pycache__", "generated"}
    for file in files:
        relative = file.relative_to(contents)
        assert not any(part in forbidden for part in relative.parts), relative
        assert file.suffix.lower() not in (".pdb", ".ilk", ".idb", ".user", ".pyc"), relative
        assert len(str(file)) < 260, f"Legacy Windows path too long: {file}"
    assert not list((individual / "04_作品紹介動画").iterdir()), "Do not substitute an old video."

    archive_path = stage / (account + "_動画追加前.zip")
    if archive_path.exists():
        raise SystemExit("Draft ZIP already exists; verify a new stage instead of overwriting.")
    print(f"MONTHLY_CONTENTS_VERIFIED files={len(files)} original_source_files={checked}", flush=True)
    with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=6, allowZip64=True) as archive:
        for file in files:
            archive.write(file, file.relative_to(contents).as_posix())
        video_folder = individual / "04_作品紹介動画"
        archive.write(video_folder, video_folder.relative_to(contents).as_posix() + "/")
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
        entries = {entry.filename: entry for entry in archive.infolist() if not entry.is_dir()}
        assert len(entries) == len(files)
        for file in files:
            relative = file.relative_to(contents).as_posix()
            assert entries[relative].file_size == file.stat().st_size
            with archive.open(relative) as stream:
                digest = hashlib.sha256()
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(block)
                assert digest.hexdigest() == sha256(file), relative

    manifest = {
        "account": account, "month": config["month"], "video_pending": True,
        "source_files_verified": checked, "content_files": len(files),
        "portfolio_sha256": sha256(args.portfolio), "program_pdf_sha256": sha256(args.document),
        "draft_zip_sha256": sha256(archive_path), "draft_zip_bytes": archive_path.stat().st_size,
    }
    verification = WORKSPACE / "generated/verification/monthly-202610"
    verification.mkdir(parents=True, exist_ok=True)
    (verification / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"MONTHLY_DRAFT_ZIP_OK files={len(files)} bytes={archive_path.stat().st_size} crc_and_sha256=1", flush=True)


if __name__ == "__main__":
    main()
