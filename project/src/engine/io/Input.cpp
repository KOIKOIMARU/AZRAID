// ===== Input.cpp =====
// キーボードとマウスの状態を毎フレーム取得し、押しっぱなしと押した瞬間を判定する

#include "engine/io/Input.h"
#include <cassert>
#include <algorithm>


#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

//using namespace Microsoft::WRL;

// DirectInputでキーボードを作成し、ゲームウィンドウを基準に入力を受け取るよう設定する
void Input::Initialize(WinApp* winApp) {
	// メンバ変数にWinAppをセット
	this->winApp = winApp;

	// 初期化処理
	HRESULT result;

	// DirectInputの初期化
	result = DirectInput8Create(
		winApp->GetHInstance(), // ← これで現在のインスタンスハンドルを取得
		DIRECTINPUT_VERSION, IID_IDirectInput8,
		(void**)&directInput, nullptr);
	assert(SUCCEEDED(result));

	// キーボードデバイスの生成
	//IDirectInputDevice8* keyboard = nullptr;
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));

	// 入力データ形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(result));

	// 排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(
		winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));
}

// 前回のキー状態を控え、現在のキーとマウスを読む　自動検証の入力はキー判定側で切り替える
void Input::Update() {
	playbackActive_ = false; // 次のフレームで指定されなければ、実際の操作へ戻す
	// 更新処理
	keyboard->Acquire();
	// キーの状態
	memcpy(keyPre, key, sizeof(key)); // 前の状態を保存
	keyboard->GetDeviceState(sizeof(key), key);

	POINT cursorPosition{};
	if (winApp && GetCursorPos(&cursorPosition)) {
		ScreenToClient(winApp->GetHwnd(), &cursorPosition);
		RECT clientRect{};
		GetClientRect(winApp->GetHwnd(), &clientRect);
		const float clientWidth = static_cast<float>(
			(std::max)(clientRect.right - clientRect.left, 1L));
		const float clientHeight = static_cast<float>(
			(std::max)(clientRect.bottom - clientRect.top, 1L));
		mousePosition_.x = std::clamp(
			static_cast<float>(cursorPosition.x),
			0.0f,
			clientWidth);
		mousePosition_.y = std::clamp(
			static_cast<float>(cursorPosition.y),
			0.0f,
			clientHeight);
	}
}

// キーが押され続けているか調べる　入力再生中は記録された状態を使う
bool Input::PushKey(BYTE keyNumber)
{
    if (playbackActive_) { return playbackKeys_[keyNumber] != 0; }
	// 指定キーを押していればtrueを返す
	if (key[keyNumber]) {
		return true;
	}
	return false;
}

// 前フレームで離していて、今回押されたキーだけを成立とする
bool Input::TriggerKey(BYTE keyNumber)
{
    if (playbackActive_) { return playbackKeys_[keyNumber] && !playbackPrevious_[keyNumber]; }
	// 指定キーがトリガーならtrueを返す
	if (key[keyNumber] && !keyPre[keyNumber]) {
		return true;
	}
	return false;
}
