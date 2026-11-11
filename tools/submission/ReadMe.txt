SKYBREAK
小泉 羚 / 日本工学院専門学校 ゲームクリエイター科

■ 起動
ZIPをすべて展開し、このフォルダの SKYBREAK.exe を起動してください。
resources・shaders・DLLは移動せず、実行ファイルと一緒に置いてください。
Windows 11 64bit / DirectX 12対応GPUの環境で動作確認しています。
キーボードとマウスを使用します。初めての方は「チュートリアル」から遊べます。

■ 主な操作
WASD / 方向キー : 移動
マウス          : 照準
SPACE           : 射撃（離すとチャージ、満タンで押すとチャージショット）
A / D + Shift   : 回避
Q               : 残像連撃
Esc             : ポーズ
F11             : 全画面 / ウィンドウ切り替え
フィーバーはゲージが満タンになると自動で発動します。

■ ソースコード
このフォルダ内の src・include・shaders がソースコードです。
CG2.sln を Visual Studio で開き、x64 / Release または Debug でビルドできます。
必要な構成: C++によるデスクトップ開発、MSVC v145、Windows SDK 10.0.26100.0。
確認環境: Visual Studio 2026 Community。
ビルド結果は一つ上のフォルダの generated/outputs に出力されます。
Visual Studioからデバッグ実行する場合、作業ディレクトリを $(ProjectDir) に設定してください。
配布用の SKYBREAK.exe は、Release構成の CG2.exe を改名したものです。

■ 使用ライブラリ・素材
権利表記と配布元は licenses/ThirdParty.txt を参照してください。
