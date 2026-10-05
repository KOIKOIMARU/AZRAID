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

class TitleScene : public BaseScene {
public:
    TitleScene();
    ~TitleScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void RequestStart(bool tutorial = false);

private:
    void PrepareBackdrop();
    void PrepareTitleComposition();
    void UpdateBackdrop();
    void UpdateFlightEffects(const Math::Vector3& center, const Math::Matrix4x4& rotation, float thrust, float shipScale);
    void DrawMenu(bool gameReady);

    std::unique_ptr<Camera> camera_; // タイトル専用。本編のカメラ・機体状態は変更しない。
    std::unique_ptr<Object3dCommon> objectCommon_;
    std::unique_ptr<TitleRift> rift_; // ロゴの切断面の反射と、自機が残す航跡のタイトル専用描画。
    std::unique_ptr<Object3d> ship_; // 本編の自機と同じモデルを使う、タイトル専用の1機。
    std::array<std::unique_ptr<Object3d>, 6> exhaust_; // 左右ノズルの外炎・内炎・発光。初期化時だけ確保する。
    std::array<Math::Vector4, 2> engines_{}; // ノズルの画面位置と噴射方向。航跡を実モデルへ合わせる。
    std::array<Math::Vector4, 12> wakePoints_{}; // 左右6点ずつの過去のノズル位置、幅、残光。固定領域を使う。
    std::unique_ptr<SoundManager> sound_; // 出撃の決定音・加速音。本編の再生状態とは分離する。
    Math::Vector3 modelCenter_{};
    float elapsed_ = 0.0f; // 導入はモデル読み込みと独立し、最初からロゴを見せる。
    float presentationRate_ = 1.0f; // 確認用環境変数でのみ変更。通常のゲームは常に等速。
    float flybyElapsed_ = 0.0f; // 準備後に始まる短い通過と航跡の余韻。出撃カットとは独立。
    float previewFlybyPhase_ = -1.0f; // 確認用環境変数で指定した場合だけ、待機演出の一瞬を固定。
    float departureTime_ = 0.0f; // 出撃確定後の加速カット。秒単位で進める。
    float departureFade_ = 0.0f;
    float departureAcceleration_ = 0.0f;
    int selectedItem_ = 0;
    std::array<float, 3> menuEmphasis_{ 1.0f, 0.0f, 0.0f }; // 各項目の選択色。位置・文字サイズは動かさない。
    bool showControls_ = false;
    bool startRequested_ = false;
    SceneType requestedScene_ = SceneType::Game;
};
