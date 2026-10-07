#pragma once
#include "engine/scene/BaseScene.h"
#include "engine/scene/SceneType.h"
#include <memory>
#include <array>
#include "engine/base/Math.h"

class Camera;
class Object3d;
class Object3dCommon;
class SoundManager;
class TitleRift;
class GameScene;

class TitleScene : public BaseScene {
public:
    TitleScene();
    ~TitleScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void RequestStart(bool tutorial = false);
    GameScene* GetDeparturePreview() const;
    void DrawDepartureOverlay();
    void DrawFlightOverlay(); // 切断中の機体はロゴの手前で見せる。

private:
    void PrepareBackdrop();
    void PrepareTitleComposition();
    void UpdateBackdrop();
    void UpdateFlightEffects(const Math::Vector3& center, const Math::Matrix4x4& rotation, float thrust,
        float shipScale, float fade = 1.0f);
    void DrawMenu(bool gameReady);
    bool IsForegroundFlight() const;

    std::unique_ptr<Camera> camera_; // タイトル専用。本編のカメラ・機体状態は変更しない。
    std::unique_ptr<Object3dCommon> objectCommon_;
    std::unique_ptr<TitleRift> rift_; // ロゴの切断面の反射と、自機が残す航跡のタイトル専用描画。
    std::unique_ptr<Object3d> ship_; // 本編の自機と同じモデルを使う、タイトル専用の1機。
    std::array<std::unique_ptr<Object3d>, 6> exhaust_; // 左右ノズルの外炎・内炎・発光。初期化時だけ確保する。
    std::array<Math::Vector4, 2> engines_{}; // ノズルの画面位置と噴射方向。航跡を実モデルへ合わせる。
    std::array<Math::Vector4, 12> wakePoints_{}; // 近距離は左右6点、遠方の周回は1本12点。固定領域を共有する。
    std::unique_ptr<SoundManager> sound_; // 出撃の決定音・加速音。本編の再生状態とは分離する。
    Math::Vector3 modelCenter_{};
    float elapsed_ = 0.0f; // 導入はモデル読み込みと独立し、最初からロゴを見せる。
    float presentationRate_ = 1.0f; // 確認用環境変数でのみ変更。通常のゲームは常に等速。
    float arrivalElapsed_ = 0.0f; // モデルが準備できてから遠方より登場。読み込みで途中へ飛ばさない。
    float flightOpacity_ = 0.0f; // 遠方で光へ移る機体の透明度。出撃時も直前の状態を保つ。
    float flybyElapsed_ = 0.0f; // 遠方での周回と、手前への切断突入の周期。出撃カットとは独立。
    float previewFlybyPhase_ = -1.0f; // 確認用環境変数で指定した場合だけ、待機演出の一瞬を固定。
    float departureTime_ = 0.0f; // 溜め、一閃、上下の完全切断。本編は開き切ってから進める。
    int selectedItem_ = 0;
    std::array<float, 3> menuEmphasis_{ 1.0f, 0.0f, 0.0f }; // 各項目の選択色。位置・文字サイズは動かさない。
    bool showControls_ = false;
    bool startRequested_ = false;
    SceneType requestedScene_ = SceneType::Game;
};
