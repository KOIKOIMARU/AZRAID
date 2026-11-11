"""Create a fresh monthly hand-in copy without changing the development tree."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


WORKSPACE = Path(__file__).resolve().parents[2]
PROJECT = WORKSPACE / "project"


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--portfolio", type=Path, required=True)
    parser.add_argument("--account", required=True)
    args = parser.parse_args()
    stage = args.stage.resolve()
    if stage.exists():
        raise SystemExit(f"Destination already exists: {stage}")
    if not stage.is_relative_to(WORKSPACE):
        raise SystemExit("Prepare within the workspace; publish only after verification.")

    contents = stage / "提出内容"
    individual = contents / "01_個人作品"
    source = individual / "01_プロジェクト一式" / "CG2_project"
    runtime = individual / "02_exeセット"
    documents = individual / "03_プログラム説明資料"
    videos = individual / "04_作品紹介動画"
    for folder in (source, documents, videos, stage / "ZIP外提出"):
        folder.mkdir(parents=True, exist_ok=True)
    shutil.copytree(args.runtime, runtime)

    # Start only from approved source/data directories. The protected temporary
    # directory is never traversed, even when untracked files are included.
    command = [
        "rg", "--files", "--hidden", "--no-ignore",
        "src", "include", "shaders", "resources", "externals", "tools",
        "-g", "!**/enc_temp_folder/**", "-g", "!**/.git/**",
        "-g", "!**/.vs/**", "-g", "!**/generated/**", "-g", "!**/x64/**",
        "-g", "!**/__pycache__/**", "-g", "!**/Shaders/Compiled/**",
        "-g", "!*.user", "-g", "!*.pyc", "-g", "!*.pdb", "-g", "!*.ilk",
        "-g", "!*.idb", "-g", "!*.tlog", "-g", "!*.lastbuildstate",
    ]
    listing = subprocess.run(command, cwd=PROJECT, check=True,
                             capture_output=True, encoding="utf-8").stdout
    count = 0
    for line in listing.splitlines():
        relative = Path(line)
        if "enc_temp_folder" in relative.parts:
            raise SystemExit("Protected path returned by the source inventory.")
        if relative.parts[0] in ("externals", "tools") and relative.suffix.lower() in (
                ".obj", ".exe", ".log", ".pch"):
            continue
        destination = source / "project" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(PROJECT / relative, destination)
        count += 1
    for name in ("CG2.sln", "CG2.vcxproj", "CG2.vcxproj.filters", "CG2.rc",
                 "resource.h", ".editorconfig"):
        shutil.copy2(PROJECT / name, source / "project" / name)
    shutil.copy2(WORKSPACE / "README.md", source / "README.md")
    shutil.copytree(WORKSPACE / "tools" / "submission" / "licenses",
                    source / "licenses")
    for license_file in (runtime / "licenses").iterdir():
        if license_file.is_file():
            shutil.copy2(license_file, source / "licenses" / license_file.name)

    build_notes = """アズレイド / AZRAID ソースコード一式

project/CG2.slnをVisual Studioで開き、Debug/x64またはRelease/x64でビルドします。
必要な環境 : Visual Studio C++開発環境、MSVC v145、Windows SDK 10.0.26100.0。
外部ライブラリのヘッダー・必要なライブラリファイルと、ゲーム素材を同梱しています。
DirectXTexはソリューション内の参照プロジェクトとしてビルドされます。
成果物は、このフォルダ内のgenerated/outputs/DebugまたはReleaseに生成されます。
開発時の中間ファイル、個人設定、Git履歴は含めていません。

通常起動ではタイトル画面が開き、ステージを選んで出撃します。
詳しいソース構成と操作はREADME.mdを参照してください。
"""
    write_text(source / "ビルド方法.txt", build_notes)

    portfolio_name = args.account + "_ポートフォリオ.pdf"
    portfolio = contents / "00_ポートフォリオ" / portfolio_name
    portfolio.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(args.portfolio, portfolio)
    shutil.copy2(args.portfolio, stage / "ZIP外提出" / portfolio_name)
    original_hash = hashlib.sha256(args.portfolio.read_bytes()).hexdigest()
    assert hashlib.sha256(portfolio.read_bytes()).hexdigest() == original_hash

    config = {"account": args.account, "month": "2026-10", "messages": {
        "videoMissing": "04_作品紹介動画に今回のMP4を1本入れてから、もう一度実行してください。",
        "complete": "提出用ZIPを作成しました：",
        "upload": "完成したZIPと、ZIP外提出フォルダ内のPDF・動画を提出してください。",
    }}
    write_text(stage / "submission.json", json.dumps(config, ensure_ascii=False, indent=2))
    shutil.copy2(WORKSPACE / "tools" / "submission" / "FinalizeMonthlySubmission.ps1",
                 stage / "FinalizeMonthlySubmission.ps1")
    write_text(stage / "動画を入れたらZIPを作成.cmd",
               '@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass '
               '-File "%~dp0FinalizeMonthlySubmission.ps1"\r\npause\r\n')
    write_text(stage / "提出前に読む.txt", f"""2026年10月 月次提出

提出内容には、前回のポートフォリオ、最新のソース・素材一式、
Release実行版、更新したプログラム説明資料をまとめています。

残りは今回の作品紹介動画です。
「提出内容/01_個人作品/04_作品紹介動画」に動画を1本入れて、
「動画を入れたらZIPを作成.cmd」をダブルクリックしてください。
動画は「{args.account}_作品紹介動画.mp4」としてZIP内・ZIP外にそろいます。

提出するもの
1. {args.account}.zip
2. ZIP外提出フォルダ内のポートフォリオPDF
3. ZIP外提出フォルダ内のプログラム説明資料PDF
4. ZIP外提出フォルダ内の作品紹介動画MP4

「動画追加前.zip」は準備用です。完成した提出用ZIPと間違えないでください。
この説明文・検証用ファイル・ZIP作成ツールは提出用ZIPには入りません。
学校へのアップロード操作はまだ行っていません。
""")
    print(f"MONTHLY_STAGE_OK source_files={count} portfolio_unchanged=1")
    print(f"STAGE={stage}")
    print(f"SOURCE={source}")


if __name__ == "__main__":
    main()
