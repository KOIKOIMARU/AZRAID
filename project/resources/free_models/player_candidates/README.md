# 自機モデル Omen

作者: Quaternius

配布元: https://quaternius.com/packs/ultimatespaceships.html

ライセンス: CC0 1.0 Universal（配布元の表示を2026-09-15に確認）
https://creativecommons.org/publicdomain/zero/1.0/

## 使用モデル

- Omen: 後退翼を持つ機体。本編・チュートリアル共通の自機として使用。

同パックのBlueテクスチャを使用。テクスチャ自体の塗り替えはしていない。
glTFの頂点を縦横比を保持して縮小・原点へ移動し、画像は既存のTextureConverterで
ミップ付きBC7/sRGB DDSへ事前変換。埋め込み画像を除き、頂点・索引は外部binへ分離。
全長4.2以下・全幅3.6以下のワールドサイズ。操作性能や衝突球は元の自機と共通。

変換ツール: `project/tools/PreparePlayerShip.py`

元データの公式公開ファイルID（作者サイトからリンクされたGoogle Drive）:

| モデル | glTF | Blue PNG |
|---|---|---|
| Omen | 1AI4LO8e8-CRd9z9fm4h8XPG90OekGdZb | 1cG90y7uo48I_is8puZRAoSRxSw3O7EW8 |

## 採用後の整理

2026-09-15、ユーザーの選択でOmenを採用。
ほかの比較用モデルと比較UIは削除し、起動時はOmenだけを読み込む。
左右ノズルの外炎・コアと既存6層の噴射を維持。操作性能・当たり判定は変更しない。
