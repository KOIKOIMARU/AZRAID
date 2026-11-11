#pragma once

#include "app/combat/Bullet.h"

#include <cstddef>
#include <list>
#include <memory>

// 弾の所有と再利用を担当する。移動・命中・描画の判断はここに混ぜない。
class BulletPool {
public:
    using Bullets = std::list<std::unique_ptr<Bullet>>;
    using Iterator = Bullets::const_iterator;

    void Clear(); // シーン終了時に全ての弾を破棄し、使用数の記録も初期化する。
    // 初期化中だけ呼び、弾とリストのノードをまとめて用意する。
    void Prepare(std::unique_ptr<Bullet> bullet);
    // 返すポインタは非所有。弾は使用中リストへ移り、Clearまでアドレスを保つ。
    Bullet* Acquire();
    // 追尾対象や回避済み判定など、弾に関連づけた情報を解除してから戻す。
    Iterator Recycle(Iterator position);
    void RecycleAll();

    const Bullets& Active() const { return active_; }
    size_t AvailableCount() const { return available_.size(); }
    size_t MissCount() const { return missCount_; }
    size_t PeakActiveCount() const { return peakActiveCount_; }

private:
    Bullets active_;
    Bullets available_;
    size_t missCount_ = 0;
    size_t peakActiveCount_ = 0;
};
