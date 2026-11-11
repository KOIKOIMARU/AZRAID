# 効果音の配布元

確認日: 2026-10-01。以下はいずれも配布ページでCC0を確認。

| 作者・パック | 配布ページ | 使用用途 |
|---|---|---|
| lentikula / Sci-Fi Weapon Shots SFX | https://lentikula.itch.io/sci-fi-weapon-shots-sfx-freecc0 | 通常射撃、チャージ、斬撃のエネルギー成分、フィーバーの立ち上がり |
| StarNinjas / 20 Sword Sound Effects (Attacks and Clashes) | https://opengameart.org/content/20-sword-sound-effects-attacks-and-clashes | 着弾、斬撃、決め技、スキル発動、準備完了 |
| Kenney / Sci-fi Sounds | https://kenney.nl/assets/sci-fi-sounds | 爆発、被弾、噴射、決め技の低域、フィーバー |
| Kenney / Impact Sounds | https://kenney.nl/assets/impact-sounds | 発射の芯、命中、爆発、被弾、斬撃の金属音 |
| Kenney / RPG Audio | https://kenney.nl/assets/rpg-audio | 回避の風切り音 |
| Kenney / Interface Sounds | https://kenney.nl/assets/interface-sounds | クリア、失敗 |

CC0 1.0: https://creativecommons.org/publicdomain/zero/1.0/
Kenney・lentikulaの同梱ライセンスは `licenses/` に原文を保存。
StarNinjas: https://opengameart.org/users/starninjas
StarNinjasの配布ページにはCC0表記があり、クレジットは任意。ここでは作者を明記する。

加工: 必要部分の切り出し、リサンプリング、再生速度の調整、ハイパス／ローパス、
ステレオ幅の調整、アタック位置の揃え、複数素材の合成、反転、短い反射音、
ソフト飽和、減衰包絡、ピーク制限、端のフェード。
各WAVに使った元ファイル名と原素材SHA-256は `source_manifest.json`、編集レシピは
`project/tools/GenerateCombatSounds.py` に記録している。

通常射撃・命中・撃破・斬撃は各3テイク。スキル発動とチャージ弾は別素材。
ゲーム側では連撃の進行に応じた音程変化と、強い演出中の通常SEの音量抑制を行う。
既存アニメ・ゲームからの音声抽出や公式音源の流用はしていない。BGMは変更していない。

素材作者が本作品や応募先を推奨していることを示すものではない。
