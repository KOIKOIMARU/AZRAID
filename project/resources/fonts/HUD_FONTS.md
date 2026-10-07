# タイトル・ゲームHUDのフォント

エディタのメイリオとは独立して使用する。フォントファイルは未改変。

- 和文の見出し・タイトルの読み・メニュー項目: Zen Kaku Gothic New Bold
  - 配布元: https://github.com/google/fonts/tree/main/ofl/zenkakugothicnew
  - ファイル: `ZenKakuGothicNew-Bold.ttf`
  - 著作権・ライセンス: `ZEN_KAKU_GOTHIC_OFL.txt`
- 和文の本文・無線・補助説明: Zen Kaku Gothic New Medium
  - 配布元: https://github.com/google/fonts/tree/main/ofl/zenkakugothicnew
  - ファイル: `ZenKakuGothicNew-Medium.ttf`
  - 著作権・ライセンス: `ZEN_KAKU_GOTHIC_OFL.txt`
- 短い英字・数字・キー名: Chakra Petch SemiBold
  - 配布元: https://github.com/google/fonts/tree/main/ofl/chakrapetch
  - ファイル: `ChakraPetch-SemiBold.ttf`
  - 著作権・ライセンス: `CHAKRA_PETCH_OFL.txt`

タイトルの大きなAZRAIDと各画面の署名は専用のポリゴン字形。
和文は同じファミリーの二つの太さで情報の優先順位を示す。
和文を含む一行はZen Kaku Gothic New、ASCIIだけの短い表記はChakra Petchで組む。
数字は実測幅で配置し、スコアや戦績の固定幅の欄では長い値を縮小して収める。
三つのフォントは同じ参照サイズで、ImGuiの一つのアトラスが所有する。
OSへインストールせずゲームに同梱する。元のフォントファイル自体は未改変。

使用していない旧候補も保管する:
Barlow Condensed SemiBold (`BARLOW_OFL.txt`)、Rajdhani SemiBold (`RAJDHANI_OFL.txt`)、
M PLUS 1p Medium (`MPLUS1P_OFL.txt`)。

更新日: 2026-10-06。追加したBoldとChakra Petchは同日の比較に用いた公式配布ファイル。
使用する書体はいずれもSIL Open Font License 1.1。
ゲームを配布するときは、各フォントと対応する著作権・ライセンスファイルを一緒に含める。
