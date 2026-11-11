# 残像連撃のローカル効果音

現在は音の方向性を見直すため、タイトルを含む全効果音を停止している。BGMは継続。
`SoundManager::kSoundEffectsEnabled` が `false` の間、以下のバンクを生成・読み込みしてもゲーム内の効果音は鳴らない。
以下は再検討用に保持している生成・検証手順であり、再有効化は別途判断する。

最新版の素材試聴から、斬撃と決め技だけを本編のテンポに合わせて編集した試聴用バンク。
通常射撃・通知音・BGM・位置音響は変更しない。

## 利用条件と配置

2026-10-01に [Sonniss GDC Bundle License v2.0](https://sonniss.com/gdc-bundle-license/) を確認。
ゲームなど自身の作品への同期・加工利用と、音源そのものの再配布は別扱い。
加工音もライセンス対象のため、元素材・生成WAVを公開リポジトリへ追加しない。
完成ゲームとしての配布は、このローカル試聴とは別に配布構成を確認する。

- 元素材: `assets_source/audio_cinematic_20261001/`
- バンク: `generated/audio-phantom-runtime/`
- 比較試聴: `generated/audio-phantom-review-20261001/`

いずれも既存の `.gitignore` により管理対象外。元素材の取得は各自が利用条件を確認して行う。
使用するのは David Dumais Audio の Sword Slash Impact V2 Assorted 18 と
344 Audio の Impact 038。ファイル名・SHA-256・加工内容は生成スクリプトと出力manifestに記録する。

## 生成とゲーム内確認

`numpy`、`scipy`、`soundfile` を使えるPythonで、リポジトリのルートから実行する。

```powershell
python project/tools/PreparePhantomAudio.py
```

いつもどおり作業ディレクトリを `project` にしてゲームを起動すると、生成した4つのWAVを入場時に読み込む。
必要ファイルが欠けている場合、またはどれかのデコードに失敗した場合は、斬撃と決め技を両方とも従来のCC0音へ戻す。
音声データと再生ボイスは入場時に確保する。連撃中の追加確保はなく、SE25枠＋BGM3枠を維持する。

従来音だけで起動したい場合は、起動するプロセスの環境変数 `AZRAID_PHANTOM_AUDIO` を `original` にする。
比較試聴WAVは「従来音 → 1秒の無音 → 新版」の順。ゲームの0.2秒の予備動作・7/60秒の斬撃間隔・マスター音量を再現した合成で、実機の録音ではない。

ゲーム内の自動確認:

```powershell
& .\project\tools\RunSmokeTest.ps1 -Configuration Debug -GameplaySeconds 60 -Phantom
```

`AUDIO_BANK` で読み込み先、`AUDIO_NORMAL_OK` と `AUDIO_FEVER_OK` で3連撃・5連撃の発音成功を確認できる。
元のステレオを保持しているが、HRTFや位置・距離に応じた立体音響の実装ではない。
ピーク・DC・端のクリック・モノ互換性・発音枠は数値検査し、音色と映像の気持ちよさはヘッドホンで判断する。
