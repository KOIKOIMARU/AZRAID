# アズレイド / AZRAID

個人制作の3Dレールシューティングゲーム。射撃・回避・スキルを使って戦います。
Windows向けに、C++ / DirectX 12で開発しています。

リポジトリ: [KOIKOIMARU/AZRAID](https://github.com/KOIKOIMARU/AZRAID)

## 開発・起動

- ソリューション: `project/CG2.sln`
- プロジェクト: `project/CG2.vcxproj`
- ビルド環境: Visual StudioのC++開発環境、MSVC v145、Windows SDK 10.0.26100.0
- 構成は `Debug / x64`、`Release / x64`、`Development / x64` です。DebugとDevelopmentでは開発用の表示を使えます。

既存のビルド構成を維持するため、フォルダ・ソリューション・実行ファイルの内部名は `CG2` のままです。
ゲームの正式名称は「アズレイド」、英字表記は「AZRAID」です。

Releaseの出力先は `generated/outputs/Release/CG2.exe` です。
実行ファイルをダブルクリックして起動できます。
実行用パッケージでは `AZRAID.exe` を起動します。パッケージ内の `resources`、`shaders`、DLLは実行ファイルと同じフォルダに置きます。

Debug版では、本編中にF1を押し「デバッグ操作」の「フェーズ移動」から確認したい地点に移れます。
起動時から特定の地点を確認するには、Debug / x64でビルドして `project/tools/RunDebugScene.cmd` を開き、
1〜5を選びます。必要な素材の読み込みが終わると、選んだ地点からすぐ操作できます。
この起動方法では、戦績の保存を無効にしています。

起動から本編までの動作は、リポジトリのルートから `& .\project\tools\RunSmokeTest.ps1 -Configuration Release -GameplaySeconds 15` で確認できます。

## 操作

通常起動ではタイトルを表示します。「航空都市」へ出撃し、敵編隊を突破してボスを撃破するとクリアです。
結果画面から再挑戦・タイトルへの移動ができます。クリア時のベストスコアと最高ランクは、
ユーザーごとの `%LOCALAPPDATA%\AZRAID\record-v3.txt` へ保存し、次回起動でも引き継ぎます。
チュートリアルや自動検証の記録は、通常プレイの成績に混ぜません。
ランクは得点で決まり、Aは80,000点、Sは150,000点からです。以前の得点ルールの記録は元のファイルに残しています。
リングはステージの進行に合わせてコース上に出現します。くぐると短時間加速し、得点が入り、チャージが補充されます。通常時はフィーバーゲージが増え、発動中は残り時間が延びます。
射撃を止めてチャージをためると、次に撃つときにチャージ弾を出せます。密集した編隊には爆風でまとめてダメージを与えられます。
フィーバーゲージが満タンになると、自動でフィーバーが発動します。
現在の設定では効果音をオフにしています。BGMは通常戦闘・ボス戦・フィーバーに合わせて切り替わります。

| 操作 | 入力 |
|---|---|
| 移動 | W / A / S / D（矢印キーも使用可能） |
| 照準 | マウス |
| 射撃・連射 | Space（長押しで連射） |
| 回避 | Shift（A / D または ← / → で方向を指定。左右の入力がないときは最後に入力した方向） |
| スキル | Q |
| ポーズ | Esc |
| 操作方法を開く | H（プレイ中） |
| 再挑戦 | R（リザルト表示後） |
| 全画面切替 | F11 / Alt + Enter |

タイトル画面からチュートリアルと操作方法を確認できます。
チュートリアルは移動・射撃・回避・チャージ・スキル・リング・フィーバーの7項目です。
各課題は実際の操作の達成で進み、リングの通過失敗やフィーバーの時間切れでも繰り返せます。
練習中は撃墜されず、次の課題でHPを戻します。本編の成績には記録しません。

## ソースの構成

ゲームの開始処理は `main.cpp` → `MyGame` → `SceneManager` の順に進みます。
`GameScene` は `GameRuntime` を更新し、再挑戦やタイトルへ戻る操作を受け取ると、`SceneManager` に次のシーンを指定します。

| ファイル・クラス | 担当 |
|---|---|
| `MyGame` | 起動時の準備、フレームの更新・描画、終了時の解放 |
| `GameResources` | タイトルと本編で共有するモデル・画像・シェーダーの事前読み込み |
| `ModelManager` | 読み込み済みモデルの管理、別スレッドでのモデル解析、GPU用データと材質画像の準備 |
| `TitleScene` | タイトルとステージ選択の入力、出撃への切り替え |
| `TitleMotion` / `TitleWordmark` / `TitleRift` | 飛行と切断の時刻・位置の計算、ロゴの描画、背景の断面と航跡の描画 |
| `StageSelectionPresentation` | 選択画面の裂け目、待機中の時計、機体の戻りから本編へ出撃するまでの時刻 |
| `StageEntryTiming` | タイトルと本編で使う出撃演出の時間設定 |
| `Enemy` / `Player` | 各機体の位置、姿勢、HP。敵の飛行は `EnemyFlight` が計算する |
| `EnemyFlight` | 機種ごとの進入・飛行・離脱、狙撃の構え、ボスの飛行先と反動 |
| `PlayerDodge` | 回避の先行入力・再使用時間・横移動・成功判定の受付 |
| `GameplaySmokeTest` | 明示的に起動した自動テストの操作、計測、終了判定 |

タイトルからステージ選択へ移るときは、カメラの位置・向きと切断線を引き継ぎます。
選択画面では準備済みの本編を描き、出撃後はそのシーンを本編として使います。自機のモデルはタイトルと本編で共有し、機体の位置や向きはそれぞれの `Object3d` が持ちます。

`GameRuntime` が使う主なクラスは次のとおりです。

| クラス | 担当 |
|---|---|
| `CombatProgress` | スコア、連続撃破の猶予時間、倍率、プレイ中の最大チェイン数 |
| `CombatRewards` | リング通過・ジャスト回避の報酬と、フィーバー中のチャージ補充 |
| `StageRecords` | クリア時のベストスコア・最高ランク・クリア回数の保存と読込み |
| `FeverState` | ゲージ、発動・終了、残り時間、発動回数 |
| `PlayerShooting` | 連射間隔、入力の先行受付、チャージ量、射撃調整値 |
| `PlayerControl` | 移動・回避・射撃の入力、照準の更新、弾の発射と発射数 |
| `PlayerSkill` | スキルの対象選択、連続攻撃の進行、再使用時間、命中による回復、集計 |
| `PlayerSkillPresentation` | スキルの残像・斬撃・照準に重ねる画面効果の再生と描画 |
| `PlayerFlightEffects` | 自機のエンジン噴射、チャージの光、回避の残像と寿命 |
| `GameplayHud` | HP・チャージ・照準・フィーバーなどの計器と、命中・被弾の短い表示 |
| `TutorialProgress` | 練習課題の開始条件・成功判定・進行、回避練習の射撃時刻と操作案内 |
| `GameplayCamera` | 追従の基準姿勢・ばねの速度・画角・揺れ・ブラーの強さ |
| `GameplaySceneSetup` | ステージ準備の順序と進捗、描画オブジェクト・弾・演出用リソースの準備と解放 |
| `ClearResultPresentation` | クリア後に映す機体・カメラ・噴射の準備、更新、描画 |
| `PlayerDefeatPresentation` | 自機の失速・墜落、破片・炎・煙の準備と描画、敗北画面を開く時刻 |
| `StageEntryPresentation` | 選択画面での周回、出撃・再挑戦時の機体とカメラの姿勢、目標表示 |
| `FlightSpeedEffects` | 飛行中に流れる光と画面端の風の線の配置、更新、描画 |
| `FlightMotionBlur` | ブラー用の画面座標とカメラの履歴、比較表示 |
| `GameplayAudio` | 本編の音声読み込み、通常・ボス・フィーバー間のBGM切り替えと音量調整 |
| `GameplayShadows` | ステージ中の影を投影するモデルの選別と接地影の描画 |
| `GameplayMenu` | ポーズ、操作説明、メニュー決定から画面切り替えまでの状態 |
| `StageResultMenu` | クリア・敗北・練習終了画面の表示、詳細切り替えと項目選択 |
| `StageResults` | リザルトを開くタイミング、クリア画面の演出更新、クリア記録の確定 |
| `StageDirector` | 配置の進行、編隊を出せる空き枠、出現済みの状態、区間の判定 |
| `StageFlight` | レールの位置・速度とステージ進行、区間ごとの速度・カメラ設定、リングの配置位置 |
| `StageEnemyFactory` | 機種に合うモデルとテクスチャの選択、敵機の生成 |
| `StageEnemies` | ステージ内の敵機の所有、通常編隊・ボス・練習標的の生成、更新・撤退・参照解除 |
| `EnemyAttackPatterns` | 通常敵の射撃間隔・連射の並び、観測した自機の移動を使う予測照準 |
| `GameplayAim` | 照準の位置、ロック候補、画面上の機体に照準が重なるかの判定 |
| `RuntimeSceneObjects` | シーンに配置した物体と保存用の情報、JSONの読み書き |
| `BossAttackController` | ボスの攻撃順、予備動作、照準の固定、攻撃後の反撃時間、HP半分での段階移行 |
| `BossPresentation` | ボス接近時の通信、登場・形態移行の待ち時間、撃破時の閃光と連続爆発 |
| `ProjectileSystem` | 自機弾と敵弾の発射・描画・回収、追尾先と回避済みの記録 |
| `CombatResolver` | 弾の球判定、チャージの爆風、弾とスキルのダメージ、撃破報酬と命中・被弾・撃破の集計 |
| `BulletPool` | 弾の所有、使用中と待機中の切り替え、再利用、使用数の記録 |
| `CombatEffects` | 命中・撃破・発射・被弾ごとの色、形、出現する順番 |
| `CombatEffectPlayback` | 演出の時間と描画順、起動時に確保した描画物体の貸し出しと回収 |
| `CombatOverlay` | フィーバー中の画面効果と、敵の攻撃予告・シールド・支援線の描画 |
| `PlayerCombatFeedback` | ジャスト回避・命中の演出時間、カメラの揺れとスローの強さ |
| `RailScenery` | 背景の生成、街区の配置、可視判定、不透明・透明部分の描画 |
| `RingGates` | リングの生成、寿命、移動経路との通過判定、描画物体の再利用 |

`BossAttackController` はHPと攻撃可能かどうかを受け取り、発射要求を固定長の配列で返します。
`GameRuntime` は発射要求を受けて `ProjectileSystem` から弾を出します。形態変更時は `BossPresentation` の演出を開始し、前の形態の敵弾を回収します。
HUDとダメージ計算は同じ反撃時間を読み、表示と実際の倍率がずれないようにしています。
待機・予備動作・連射・反撃時間は名前付きの状態で追えます。初期化と攻撃中断もクラスへ集約しています。
攻撃時刻・弾数・照準の固定・段階移行・再挑戦時のリセットは `project/tests/BossAttackControllerTests.vcxproj` で確認できます。

`ProjectileSystem` は自機用と敵用の `BulletPool` を保持し、弾の回収と同時に追尾先や回避済みの記録を解除します。
シーンの準備中に弾とリストのノードを用意し、射撃・回収時はノードを移して使い回します。
弾の移動は `Bullet`、当たり判定とダメージの適用は `CombatResolver` が担当します。`GameRuntime::UpdateGameplayCollisions()` が自機弾、敵弾の順に判定を呼びます。

`PlayerControl` はキー入力を読み、移動方向と回避入力を `Player` に渡します。照準と射撃状態は内部で管理します。
`PlayerShooting` は射撃キーを押下中・押した瞬間の2つの値として受け取り、発射要求を返します。
弾を生成できたときだけ `OnShotFired()` でチャージを消費します。弾を確保できなかった場合も連射間隔は守ります。
フィーバー・リング・回避による補充は専用の操作で反映し、表示側は蓄積率と完了状態を読み取ります。
発射の光や画面の揺れは `GameRuntime` が担当するため、射撃ルール自体は描画・入力クラスに依存しません。
連射間隔、先行入力の期限、補充、再挑戦時の初期化は `project/tests/PlayerShootingTests.vcxproj` で確認できます。

`PlayerSkill` は画面内の候補を照準に近い順へ並べ、通常は3機、フィーバー中は5機まで選びます。
選択した敵は所有せず、敵を破棄する前に参照を外します。対象が消えても別の敵へ勝手に切り替えません。
`PlayerSkill` は攻撃の進行と再使用までの時間を管理し、無敵やスローが有効な区間も判定します。`GameRuntime` が攻撃のタイミングを受け取り、ダメージ処理を `CombatResolver` に渡します。
対象選択、攻撃時刻、再使用の境界、対象の消滅、再挑戦時のリセットは `project/tests/PlayerSkillTests.vcxproj` で確認できます。

`PlayerFlightEffects` は演出用の値を受け取り、自機の噴射・チャージの光・残像を更新します。
入力、回避の成否、得点、カメラの揺れは決めません。回避開始だけを呼び出し側へ返します。
自機の噴射には `Object3d` を使います。噴射や残像の描画物体はシーンの準備中に用意して再利用します。演出の時間は更新処理で進めるため、ポーズ中は同じ状態を保ちます。

`GameplayHud` はアクターやゲーム進行クラスを参照せず、`GameRuntime::BuildHudFrame` が集めた表示値を読みます。
命中・被弾・チャージの閃光の時間はHUD内に持ち、更新と描画を分けています。
敵のワールド座標を使う予告・マーカー、チュートリアルの課題、ポーズと結果のメニューはそれぞれの担当に残しています。

`GameplayCamera` は本編のばね追従と練習用の補間を分け、位置・画角・揺れの履歴を管理します。
アクターやDirectXには依存せず、演出の値を受け取って最終姿勢を返します。行列への反映は `GameRuntime` が担当します。
出撃・死亡の経路は演出側で求め、完成した姿勢と自機の位置・時計を明示的に引き継ぎます。
引き継ぎ、更新間隔、揺れの寿命、停止中の参照、再挑戦時のリセットは `project/tests/GameplayCameraTests.vcxproj` で確認できます。

航空都市の敵・区間・カメラ演出の配置は `MainStage.cpp` にまとめています。
`StageDirector` は生存敵の数と攻撃の負荷を見て、次の編隊の残りが出現上限に収まるかを確認します。
条件を満たしたら配置を1件ずつ取り出し、出現済みとして記録します。1フレームに生成する通常敵の数にも上限があります。
`StageFlight` は背景の飛行距離と速度を管理し、敵配置の進行値と待ち時間は内部の `StageDirector` が管理します。
`StageEnemies` が敵を生成し、`GameRuntime` がカメラ演出とボス撃破後の遷移を調整します。
出現順、編隊の上限、進行の境界、再挑戦時のリセット、練習との分離は `project/tests/StageDirectorTests.vcxproj` で確認できます。

`StageRecords` は `MyGame` が所有し、シーン生成時にタイトルと本編へ参照を渡します。
戦闘中の得点は `CombatProgress` が管理し、`StageResults` がクリア演出後に `StageRecords` へ渡します。
記録の読込み・保存失敗・破損データの退避は `project/tests/StageRecordsTests.vcxproj` で確認できます。

`ClearResultPresentation` はクリア画面の専用カメラと機体・噴射をシーンの準備中に確保します。
`StageResults` が結果画面の経過時間と縦横比を渡して演出を更新します。クリア画面の描画では専用カメラの投影行列を使います。
戦績の保存は `StageRecords`、結果画面の選択は `StageResultMenu` が担当します。
`GameplayMenu` が決定後の演出と遷移要求を管理し、`GameScene` が次のシーンを指定します。

`RingGates` は前後の自機位置を受け取り、その間に通過したリングの個数を返します。
リングの面を横切った瞬間の位置を補間し、その点が開口部の楕円に入っているかを判定します。
`GameRuntime::ApplyRingPassReward()` が加速を開始し、得点・チャージ・フィーバーの報酬は `CombatRewards` が反映します。

`CombatEffects` は `GameRuntime` や `CombatResolver` から発生地点と演出の種類を受け取り、色・大きさ・速度などを決めます。
`CombatEffectPlayback` はシーンの準備中に描画物体を用意し、寿命が終わった物体を再利用枠へ戻します。

ゲーム固有の処理は `project/src/app` にあり、タイトルやシーン管理、共通の描画・入力・音声処理は `project/src/engine` にあります。ヘッダーと実装は機能ごとに同じフォルダに置いています。
Visual Studioでは、ゲーム固有の処理は `app`、エンジン側は `engine` のフィルタから開けます。

| ファイル | 主な内容 |
|---|---|
| `project/src/app/runtime/GameRuntime.cpp` | 初期化、各担当クラスの呼び出し、敵の出現、戦闘演出とHUD |
| `project/src/app/runtime/GameRuntimeCollision.cpp` | 戦闘判定への参照の受け渡し、自機弾と敵弾の判定順、ボス撃破からクリアへの切り替え |
| `project/src/app/combat/CombatResolver.cpp` | 弾の命中判定、チャージ弾の爆風、ダメージ、撃破報酬、ジャスト回避の判定 |
| `project/src/app/runtime/GameRuntimeEntry.cpp` | ステージ選択中の飛行、出撃、再挑戦時の導入 |
| `project/src/app/runtime/GameRuntimeDefeat.cpp` | 敗北時の戦闘終了、敵の離脱と自機の墜落、リザルト表示のタイミング |
| `project/src/app/debug/GameRuntimeEditor.cpp` | 開発用の編集画面・性能表示。`ENABLE_DEBUG_GUI` 有効時だけ使用 |
| `project/src/app/runtime/GameRuntimePhantom.cpp` | Qキーで使うスキルの対象選択、連撃、演出、効果音 |
| `project/src/app/runtime/GameRuntimeTutorial.cpp` | チュートリアルの進行と説明表示 |
| `project/src/app/debug/GameRuntimeProbe.cpp` | Debug版での自動動作確認 |
| `project/src/app/player/Player.cpp` | 自機の動作 |
| `project/src/app/enemy/Enemy.cpp` | 敵の動作 |
| `project/src/app/combat/Bullet.cpp` | 弾の動作 |
| `project/src/app/runtime/GameRuntimeTuning.h` | 複数の処理から参照する調整値 |
| `project/src/app/runtime/GameRuntimeMath.h` | 距離、補間、座標変換などの共通計算 |

`GameRuntime*.cpp` は同じ `GameRuntime` クラスの実装を、機能別に分けたものです。

ゲーム中の更新は `GameRuntime::Update()` が入口です。レール、自機、カメラ、照準の順で更新し、自機と敵の発射を処理します。敵と弾を動かしたあとに当たり判定を取り、照準表示やHUDに戦闘結果を反映します。
当たり判定を確認するときは `UpdateGameplayCollisions()` から `CombatResolver::ResolvePlayerShots()` と `ResolveEnemyShots()` へ進みます。

## 素材

第三者素材の利用条件は各素材のクレジット・ライセンスを参照してください。
自機モデルの出典と加工内容は [OmenのREADME](project/resources/free_models/player_candidates/README.md) に記載しています。
音源の配布元は [戦闘効果音のクレジット](project/resources/audio/combat/CREDITS.md) と [BGMのクレジット](project/resources/audio/music/CREDITS.md) を参照してください。
開発用の元素材と試聴データは `assets_source/` と `generated/` に置き、Gitの管理対象から除外しています。
