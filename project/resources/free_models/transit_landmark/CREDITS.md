# Transit landmark

- Source: Quaternius, Modular Sci-Fi MegaKit, Standard (free version).
- Official page: https://quaternius.com/packs/modularscifimegakit.html
- Download: https://quaternius.itch.io/modular-sci-fi-megakit
- License: CC0 1.0 Universal. Original notice: `License_Standard.txt`.
- Downloaded: 2026-09-29.

SKYBREAK uses Column_Large_Straight, Column_Hollow, Column_MetalSupport,
WallAstra_Straight and Platform_Metal2. The modules are assembled offline into
one elevated service link, piers and approach walls, and batched by material.
Only referenced base-color textures are included. No paid/pro/source content
or engine-specific shaders are used.

Rebuild with `project/tools/PrepareTransitLandmark.py` and the extracted
official Standard archive. Source archive and unused models stay outside the
runtime resources. Generated GLTF/BIN files are already supplied for builds.

## ゲーム内での使い方

本編のワールドZ=240mに一度だけ現れる高架施設。接近用の側壁と支柱から
橋の下を抜け、元の市街地へ戻る。中央は自機の移動範囲より十分に広く空け、
背景の施設として使用する（衝突障害物ではない）。チュートリアルには配置しない。
施設の前後端を含めて描画・影の範囲を判定し、画面内でループさせない。

Debugの通しプレイ試験では位置固定・再挑戦を検証する。
`CG2_SCENERY_PREVIEW=2` と `--smoke-playthrough` を併用すると、
接近・通過・退出の4地点で停止して確認できる。次へ進む操作はF8。
この確認機能はReleaseには含まれない。
