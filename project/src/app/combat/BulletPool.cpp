// ===== BulletPool.cpp =====
// 弾をあらかじめ用意し、発射時に貸し出して、消えた弾を再利用できる状態に戻す。

#include "app/combat/BulletPool.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <utility>

// 使用中・空きの弾を両方解放し、枠の使用状況の集計も初期化する。
void BulletPool::Clear()
{
    active_.clear();
    available_.clear();
    missCount_ = 0;
    peakActiveCount_ = 0;
}

// 準備した弾の所有権を受け取り、発射に使える空き一覧へ登録する。
void BulletPool::Prepare(std::unique_ptr<Bullet> bullet)
{
    assert(bullet);
    available_.push_back(std::move(bullet));
}

// 準備済みの空き枠を使用中に移す。発射のたびに描画オブジェクトを作らずに済む。
Bullet* BulletPool::Acquire()
{
    if (available_.empty()) {
        ++missCount_;
        return nullptr;
    }
    // ノード自体を移すため、射撃時にリストの確保・解放を繰り返さない。
    const auto entry = std::prev(available_.end());
    active_.splice(active_.end(), available_, entry);
    peakActiveCount_ = (std::max)(peakActiveCount_, active_.size());
    return active_.back().get();
}

// 使用中の弾を空き一覧へ移す。ループを続けられるよう、次の使用中の弾を返す。
BulletPool::Iterator BulletPool::Recycle(Iterator position)
{
    const auto next = std::next(position);
    available_.splice(available_.end(), active_, position);
    return next;
}

// 使用中の一覧をまとめて空き一覧へ移す。弾の実体は次の発射まで保持する。
void BulletPool::RecycleAll()
{
    available_.splice(available_.end(), active_);
}
