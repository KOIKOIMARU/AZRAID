// シーン管理が使う画面の種類　タイトル、出撃、本編、練習、編集の切り替えを識別する

#pragma once

enum class SceneType {
    Title,
    Game,
    Editor,
    Tutorial,
    StageSelect,
    Departure, // 練習終了から、通常の出撃演出を経て本編へ入る
    Count,
};
