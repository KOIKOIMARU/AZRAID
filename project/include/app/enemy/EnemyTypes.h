#pragma once

// 配置データと敵の挙動で共用する種類。モデルや描画のクラスには依存しない。
enum class EnemyBehavior {
    Formation,
    Swoop,
    StrafeShooter,
    DiveBomber,
    Crossfire,
    Sniper,
    Shield,
    Support,
    Boss
};

enum class EnemyEntryStyle {
    Direct,
    VFormation,
    TightFormation, // 同時に入場し、同じ揺れで小型編隊を保つ。
    LeftSweep,
    RightSweep,
    PopShooter
};
