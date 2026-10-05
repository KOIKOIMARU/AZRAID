#include "engine/scene/TitleScene.h"
#include "app/CombatHud.h"
#include "app/MenuUi.h"
#include "engine/3d/Camera.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/audio/SoundManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/io/Input.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/SceneManager.h"
#include <dinput.h>
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

namespace {
constexpr const char* kTitle = "AZRAID";
constexpr const char* kTitleJapanese = "アズレイド";
constexpr const char* kShip = "free_models/player_candidates/Omen.gltf";
constexpr const char* kSky = "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";
constexpr float kShipScale = 1.90f;
constexpr float kIdleShipScale = 1.50f;
constexpr float kDepartureDuration = 1.70f;
constexpr float kFlybyStart = 0.72f;
constexpr float kFlybyDuration = 1.18f;
constexpr float kFlybyLoop = 5.80f;
constexpr float kClosestFlight = 0.42f;


float Smooth(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

struct TitleFlyby {
    float phase = 0.0f;
    float flight = 0.0f;
    float energy = 0.0f;
    float lightX = -50.0f;
};

float TitleCutY(float screenX)
{
    // ProjectLogoCanvasの切断線を逆算する。飛行と反射は同じ傾き・透視を使う。
    constexpr float cosine = 0.9950042f, sine = 0.0998334f;
    const float q = (screenX - 195.0f) / 1.75f - 255.0f;
    const float x = (q - 1.95f * sine) / (cosine - 0.11f * sine - q * 0.00042f);
    const float y = 1.95f - 0.11f * x;
    return 247.0f + (50.0f + (-x * sine + y * cosine) / (1.0f + x * 0.00042f)) * 1.75f;
}

float Hermite(float start, float end, float startVelocity, float endVelocity, float t)
{
    const float t2 = t * t, t3 = t2 * t;
    return (2.0f * t3 - 3.0f * t2 + 1.0f) * start + (t3 - 2.0f * t2 + t) * startVelocity +
        (-2.0f * t3 + 3.0f * t2) * end + (t3 - t2) * endVelocity;
}

Math::Vector3 GetTitleFlightPosition(float flight)
{
    // 手前の旋回点を滑らかに通過する。距離と接線の変化で速度・姿勢を作る。
    const bool approach = flight < kClosestFlight;
    const float duration = approach ? kClosestFlight : 1.0f - kClosestFlight;
    const float t = approach ? flight / duration : (flight - kClosestFlight) / duration;
    const float x = approach ? Hermite(-44.0f,0.0f,90.0f * duration,36.0f * duration,t) :
        Hermite(0.0f,55.0f,36.0f * duration,100.0f * duration,t);
    const float z = approach ? Hermite(52.0f,14.0f,-60.0f * duration,8.0f * duration,t) :
        Hermite(14.0f,88.0f,8.0f * duration,120.0f * duration,t);
    const float projection = 360.0f / std::tan(0.31f);
    const float screenX = 640.0f + x / z * projection;
    const float arc = std::exp(-std::pow((flight - kClosestFlight) / 0.24f,2.0f));
    const float screenY = TitleCutY(screenX) - 84.0f - 18.0f * arc;
    return {x,(360.0f - screenY) * z / projection,z};
}

TitleFlyby GetTitleFlyby(float clock, bool ready)
{
    TitleFlyby motion;
    if (!ready) { return motion; }
    motion.phase = std::fmod(clock, kFlybyLoop);
    motion.flight = std::clamp((motion.phase - kFlybyStart) / kFlybyDuration, 0.0f, 1.0f);
    motion.energy = Smooth((motion.phase - kFlybyStart) / 0.10f) *
        (1.0f - Smooth((motion.phase - kFlybyStart - kFlybyDuration) / 0.60f));
    const auto position = GetTitleFlightPosition(motion.flight);
    motion.lightX = 640.0f + position.x / position.z * 360.0f / std::tan(0.31f);
    return motion;
}

ImVec2 ProjectLogoCanvas(ImVec2 local, float separation, int half, float depth)
{
    const float side = half == 0 ? 1.0f : -1.0f;
    const float x = local.x - 255.0f + side * separation * 0.55f + depth * 3.6f;
    const float y = local.y - 50.0f - side * separation * 0.38f + depth * 6.0f;
    const float perspective = 1.0f / (1.0f + x * 0.00042f);
    constexpr float cosine = 0.9950042f, sine = 0.0998334f;
    return { 255.0f + (x * cosine + y * sine) * perspective,
        50.0f + (-x * sine + y * cosine) * perspective };
}

Math::Vector3 RotateVector(const Math::Vector3& value, const Math::Matrix4x4& matrix)
{
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] };
}

Math::Vector3 FlightRotation(const Math::Vector3& direction, const Math::Vector3& referenceUp, float bank)
{
    const auto cross = [](const Math::Vector3& a, const Math::Vector3& b) {
        return Math::Vector3{a.y * b.z - a.z * b.y,a.z * b.x - a.x * b.z,a.x * b.y - a.y * b.x};
    };
    const auto forward = Math::Normalize(direction);
    const auto right = Math::Normalize(cross(referenceUp,forward));
    const auto up = cross(forward,right);
    const float cosine = std::cos(bank), sine = std::sin(bank);
    const Math::Vector3 bankedRight{right.x * cosine + up.x * sine,
        right.y * cosine + up.y * sine,right.z * cosine + up.z * sine};
    const Math::Vector3 bankedUp{up.x * cosine - right.x * sine,
        up.y * cosine - right.y * sine,up.z * cosine - right.z * sine};
    // 機首を接線へ向けたまま、その軸を中心にバンクする。エンジンも同じ姿勢を使う。
    const float yaw = std::asin(std::clamp(-bankedRight.z,-1.0f,1.0f));
    if (std::abs(std::cos(yaw)) < 0.0001f) {
        return {0.0f,yaw,std::atan2(-bankedUp.x,bankedUp.y)};
    }
    return {std::atan2(bankedUp.z,forward.z),yaw,std::atan2(bankedRight.y,bankedRight.x)};
}

ImVec2 ProjectTitleCanvas(const Math::Vector3& point, const Camera& camera)
{
    const auto& matrix = camera.GetViewProjectionMatrix();
    const float w = (std::max)(0.001f, point.x * matrix.m[0][3] + point.y * matrix.m[1][3] +
        point.z * matrix.m[2][3] + matrix.m[3][3]);
    const float x = (point.x * matrix.m[0][0] + point.y * matrix.m[1][0] +
        point.z * matrix.m[2][0] + matrix.m[3][0]) / w;
    const float y = (point.x * matrix.m[0][1] + point.y * matrix.m[1][1] +
        point.z * matrix.m[2][1] + matrix.m[3][1]) / w;
    const float fit = (std::min)(camera.GetAspectRatio() / (1280.0f / 720.0f), 1.0f);
    return { 640.0f + x * 360.0f * camera.GetAspectRatio() / fit, 360.0f - y * 360.0f / fit };
}

struct LogoPiece {
    std::array<ImVec2, 16> points{};
    int count = 0;
    int half = 0;
};
struct LogoGeometry {
    std::array<LogoPiece, 64> pieces{};
    size_t count = 0;
};

LogoGeometry CreateLogoGeometry()
{
    LogoGeometry geometry;
    float pen = 0.0f;
    const auto polygon = [&](std::initializer_list<ImVec2> shape) {
        std::array<ImVec2, 12> source{};
        int count = 0;
        for (const auto point : shape) {
            source[count++] = { pen + point.x + (100.0f - point.y) * 0.19f, point.y };
        }
        for (int half = 0; half < 2; ++half) {
            LogoPiece piece;
            piece.half = half;
            const auto distance = [&](ImVec2 point) {
                const float cut = point.y - 80.0f + point.x * 0.11f;
                return half == 0 ? -cut - 0.65f : cut - 0.65f;
            };
            for (int index = 0; index < count; ++index) {
                const auto a = source[index], b = source[(index + 1) % count];
                const float da = distance(a), db = distance(b);
                if (da >= 0.0f) { piece.points[piece.count++] = a; }
                if ((da >= 0.0f) != (db >= 0.0f)) {
                    const float t = da / (da - db);
                    piece.points[piece.count++] = { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
                }
            }
            if (piece.count >= 3) { geometry.pieces[geometry.count++] = piece; }
        }
    };
    for (const char* letter = kTitle; *letter; ++letter) {
        switch (*letter) {
        case 'A':
            polygon({ {0,100}, {35,0}, {55,0}, {26,100} });
            polygon({ {43,0}, {58,0}, {90,100}, {65,100} });
            polygon({ {25,59}, {64,53}, {70,72}, {19,79} });
            pen += 93.0f;
            break;
        case 'Z':
            polygon({ {0,0}, {97,-5}, {78,19}, {0,19} });
            polygon({ {55,18}, {82,18}, {26,83}, {0,83} });
            polygon({ {0,81}, {83,81}, {67,105}, {-12,105} });
            pen += 90.0f;
            break;
        case 'R':
            polygon({ {0,0}, {22,0}, {22,100}, {0,100} });
            polygon({ {21,0}, {65,0}, {82,18}, {21,18} });
            polygon({ {61,17}, {82,17}, {82,43}, {61,43} });
            polygon({ {21,42}, {82,42}, {63,61}, {21,61} });
            polygon({ {36,58}, {62,58}, {90,100}, {64,100} });
            pen += 96.0f;
            break;
        case 'I':
            polygon({ {3,0}, {27,0}, {21,100}, {-3,100} });
            pen += 37.0f;
            break;
        case 'D':
            polygon({ {0,0}, {22,0}, {22,100}, {0,100} });
            polygon({ {21,0}, {62,0}, {84,20}, {63,20}, {21,18} });
            polygon({ {63,19}, {84,20}, {84,78}, {63,82} });
            polygon({ {21,81}, {84,78}, {62,100}, {21,100} });
            pen += 92.0f;
            break;
        }
    }
    return geometry;
}

void DrawWordmark(ImDrawList* draw, ImVec2 origin, float scale, float elapsed, const TitleFlyby& flyby)
{
    static const LogoGeometry geometry = CreateLogoGeometry();
    const float idleTime = (std::max)(0.0f, elapsed - 2.10f);
    const float idle = Smooth(idleTime / 1.40f);
    constexpr float separation = 3.5f;
    const float lightX = -160.0f + 1600.0f * std::fmod(idleTime / 10.0f + 0.35f, 1.0f);
    const auto cutLight = [&](ImVec2 screen) {
        // 投影後の画面位置を使い、実機の通過と背景・文字の反射を揃える。
        const float canvasX = 195.0f + (screen.x - origin.x) / scale * 1.75f;
        const float distance = (canvasX - lightX) / 130.0f;
        const float passed = flyby.lightX - canvasX;
        const float flybyDistance = passed / 95.0f;
        const float wake = passed > 0.0f ? std::exp(-passed / 410.0f) * 0.50f : 0.0f;
        return std::clamp(std::exp(-distance * distance) * idle * 0.10f +
            (std::exp(-flybyDistance * flybyDistance) + wake) * flyby.energy, 0.0f, 1.0f);
    };
    // 側面を先にまとめて描く。隣り合う文字ストロークの側面が正面へ重ならない。
    for (int pass = 0; pass < 3; ++pass) {
        for (size_t index = 0; index < geometry.count; ++index) {
            const auto& piece = geometry.pieces[index];
            const auto position = [&](ImVec2 local, int half, float depth) {
                const auto canvas = ProjectLogoCanvas(local, separation, half, depth);
                return ImVec2{ origin.x + canvas.x * scale, origin.y + canvas.y * scale };
            };
            float centerX = 0.0f;
            for (int vertex = 0; vertex < piece.count; ++vertex) { centerX += piece.points[vertex].x; }
            centerX /= static_cast<float>(piece.count);
            const float reveal = Smooth((elapsed - 0.28f - centerX * 0.0013f) / 0.25f);
            if (reveal <= 0.001f) { continue; }
            const auto color = [&](ImU32 value) {
                const int alpha = static_cast<int>(static_cast<float>((value >> IM_COL32_A_SHIFT) & 255) * reveal);
                return CombatHud::SurfaceColor((value & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT));
            };
            std::array<ImVec2, 16> front{};
            for (int vertex = 0; vertex < piece.count; ++vertex) {
                front[vertex] = position(piece.points[vertex], piece.half, 0.0f);
            }
            if (pass == 0) {
                std::array<ImVec2, 16> back{};
                for (int vertex = 0; vertex < piece.count; ++vertex) {
                    back[vertex] = position(piece.points[vertex], piece.half, 1.0f);
                }
                draw->AddConvexPolyFilled(back.data(), piece.count, color(IM_COL32(17,31,54,255)));
                for (int vertex = 0; vertex < piece.count; ++vertex) {
                    const int next = (vertex + 1) % piece.count;
                    const std::array<ImVec2, 4> side{ front[vertex], back[vertex], back[next], front[next] };
                    const bool upperEdge = piece.points[next].x > piece.points[vertex].x;
                    draw->AddConvexPolyFilled(side.data(), 4, color(upperEdge ?
                        IM_COL32(114,137,163,255) : IM_COL32(40,58,82,255)));
                }
            } else if (pass == 1) {
                for (int vertex = 0; vertex < piece.count; ++vertex) {
                    const int next = (vertex + 1) % piece.count;
                    const auto onCut = [&](ImVec2 point) {
                        return std::abs(std::abs(point.y - 80.0f + point.x * 0.11f) - 0.65f) < 0.005f;
                    };
                    if (!onCut(piece.points[vertex]) || !onCut(piece.points[next])) { continue; }
                    const std::array<ImVec2, 4> cut{ front[vertex],
                        position(piece.points[vertex], piece.half, 0.72f),
                        position(piece.points[next], piece.half, 0.72f), front[next] };
                    const int first = draw->VtxBuffer.Size;
                    draw->AddConvexPolyFilled(cut.data(), 4, IM_COL32_WHITE);
                    for (int cutVertex = first; cutVertex < draw->VtxBuffer.Size; ++cutVertex) {
                        auto& value = draw->VtxBuffer[cutVertex];
                        const ImU32 tint = CombatHud::Mix(IM_COL32(77,96,124,255),
                            IM_COL32(242,248,255,255), cutLight(value.pos));
                        const ImU32 aa = (value.col >> IM_COL32_A_SHIFT) & 255;
                        const ImU32 alpha = static_cast<ImU32>(static_cast<float>(aa) * reveal);
                        value.col = (CombatHud::SurfaceColor(tint) & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
                    }
                    const ImVec2 midpoint{ (front[vertex].x + front[next].x) * 0.5f,
                        (front[vertex].y + front[next].y) * 0.5f };
                    draw->AddLine(front[vertex], front[next], color(CombatHud::Mix(
                        IM_COL32(133,155,184,255), IM_COL32(255,255,255,255), cutLight(midpoint))), 1.0f * scale);
                }
            } else {
                const int first = draw->VtxBuffer.Size;
                draw->AddConvexPolyFilled(front.data(), piece.count, IM_COL32_WHITE);
                for (int vertex = first; vertex < draw->VtxBuffer.Size; ++vertex) {
                    auto& value = draw->VtxBuffer[vertex];
                    const float y = std::clamp((value.pos.y - origin.y) / (100.0f * scale), 0.0f, 1.0f);
                    ImU32 tint = CombatHud::Mix(IM_COL32(253,252,249,255),
                        IM_COL32(176,199,227,255), y * 0.62f);
                    const float x = (value.pos.x - origin.x) / scale - 255.0f;
                    const float relativeY = (value.pos.y - origin.y) / scale - 50.0f;
                    const float uprightX = x * 0.9950042f - relativeY * 0.0998334f;
                    const float inverse = 1.0f / (1.0f - uprightX * 0.00042f);
                    const float localX = 255.0f + uprightX * inverse;
                    const float localY = 50.0f + (x * 0.0998334f + relativeY * 0.9950042f) * inverse;
                    const float cutDistance = std::abs(localY - 80.0f + localX * 0.11f);
                    tint = CombatHud::Mix(tint,IM_COL32(255,255,255,255),
                        std::exp(-cutDistance / 10.0f) * cutLight(value.pos) * 0.72f);
                    const ImU32 aa = (value.col >> IM_COL32_A_SHIFT) & 255;
                    const ImU32 alpha = static_cast<ImU32>(static_cast<float>(aa) * reveal);
                    value.col = (CombatHud::SurfaceColor(tint) & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
                }
            }
        }
    }
}


}

// タイトル専用の断面と航跡。画像・中間描画面・追加SRVを使わず直接描く。
class TitleRift {
public:
    void Initialize(DirectXCommon* dxCommon)
    {
        dxCommon_ = dxCommon;
        D3D12_ROOT_PARAMETER parameter{};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        parameter.Constants.Num32BitValues = 64;
        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = 1;
        rootDesc.pParameters = &parameter;
        Microsoft::WRL::ComPtr<ID3DBlob> signature, error;
        if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
            FAILED(dxCommon_->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(),
                signature->GetBufferSize(), IID_PPV_ARGS(&root_)))) {
            throw std::runtime_error("Title rift root signature");
        }
        const auto vs = dxCommon_->CompileShader(L"shaders/Fullscreen.VS.hlsl", L"vs_6_0");
        const auto ps = dxCommon_->CompileShader(L"shaders/TitleRift.PS.hlsl", L"ps_6_0");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = root_.Get();
        desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
        desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.DepthStencilState.DepthEnable = FALSE;
        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline_)))) {
            throw std::runtime_error("Title rift pipeline");
        }
        auto& blend = desc.BlendState.RenderTarget[0];
        blend.BlendEnable = TRUE;
        blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&wakePipeline_)))) {
            throw std::runtime_error("Title flight wake pipeline");
        }
    }

    void Draw(float elapsed, float departure, const TitleFlyby& flyby,
        const std::array<Math::Vector4, 2>& engines,
        const std::array<Math::Vector4, 12>& wakePoints, bool wake = false)
    {
        const auto& viewport = dxCommon_->GetViewport();
        std::array<float, 64> parameters{ viewport.Width, viewport.Height, elapsed, departure,
            flyby.lightX, flyby.energy, flyby.flight, wake ? 1.0f : 0.0f,
            engines[0].x, engines[0].y, engines[0].z, engines[0].w,
            engines[1].x, engines[1].y, engines[1].z, engines[1].w };
        for (size_t index = 0; index < wakePoints.size(); ++index) {
            const auto& point = wakePoints[index];
            const size_t offset = 16 + index * 4;
            parameters[offset] = point.x; parameters[offset + 1] = point.y;
            parameters[offset + 2] = point.z; parameters[offset + 3] = point.w;
        }
        auto* command = dxCommon_->GetCommandList();
        command->SetGraphicsRootSignature(root_.Get());
        command->SetPipelineState(wake ? wakePipeline_.Get() : pipeline_.Get());
        command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command->SetGraphicsRoot32BitConstants(0, static_cast<UINT>(parameters.size()), parameters.data(), 0);
        command->DrawInstanced(3, 1, 0, 0);
    }
private:
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> wakePipeline_;
};

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize()
{
    elapsed_ = departureTime_ = departureFade_ = departureAcceleration_ = 0.0f;
    presentationRate_ = 1.0f;
    flybyElapsed_ = 0.0f;
    previewFlybyPhase_ = -1.0f;
    std::array<char, 32> previewRate{};
    const DWORD previewLength = GetEnvironmentVariableA("AZRAID_TITLE_PREVIEW_RATE",
        previewRate.data(), static_cast<DWORD>(previewRate.size()));
    if (previewLength > 0 && previewLength < previewRate.size()) {
        const char* value = previewRate.data();
        char* end = nullptr;
        const float rate = std::strtof(value, &end);
        if (end != value && *end == '\0' && std::isfinite(rate) && rate >= 0.10f && rate <= 1.0f) {
            presentationRate_ = rate;
        }
    }
    std::array<char, 32> previewPhase{};
    const DWORD phaseLength = GetEnvironmentVariableA("AZRAID_TITLE_PREVIEW_PHASE",
        previewPhase.data(), static_cast<DWORD>(previewPhase.size()));
    if (phaseLength > 0 && phaseLength < previewPhase.size()) {
        const char* value = previewPhase.data();
        char* end = nullptr;
        const float phase = std::strtof(value, &end);
        if (end != value && *end == '\0' && std::isfinite(phase) && phase >= 0.0f && phase < kFlybyLoop) {
            previewFlybyPhase_ = phase;
        }
    }
    selectedItem_ = 0;
    menuEmphasis_ = { 1.0f, 0.0f, 0.0f };
    showControls_ = startRequested_ = false;
    requestedScene_ = SceneType::Game;
    PrepareTitleComposition();
}

void TitleScene::RequestStart(bool tutorial)
{
    if (startRequested_) { return; }
    requestedScene_ = tutorial ? SceneType::Tutorial : SceneType::Game;
    startRequested_ = true;
    showControls_ = false;
    if (sound_) { sound_->Play("confirm"); }
}

void TitleScene::PrepareBackdrop()
{
    if (objectCommon_ || GameScene::GetResourcePreloadStep() < 8) { return; }
    Model* model = ModelManager::GetInstance()->FindModel(kShip);
    if (!model || model->GetVertices().empty()) { return; }
    // 自機と環境光は本編と共有し、タイトルのカメラ・姿勢は独立させる。
    camera_ = std::make_unique<Camera>();
    camera_->SetFovY(0.56f);
    camera_->SetFarClip(200.0f);
    objectCommon_ = std::make_unique<Object3dCommon>();
    objectCommon_->Initialize(dxCommon_, srvManager_);
    objectCommon_->SetDefaultCamera(camera_.get());
    objectCommon_->SetEnvironmentTexturePath(kSky);

    Math::Vector3 min{ FLT_MAX, FLT_MAX, FLT_MAX }, max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (const auto& vertex : model->GetVertices()) {
        min.x = (std::min)(min.x, vertex.position.x); max.x = (std::max)(max.x, vertex.position.x);
        min.y = (std::min)(min.y, vertex.position.y); max.y = (std::max)(max.y, vertex.position.y);
        min.z = (std::min)(min.z, vertex.position.z); max.z = (std::max)(max.z, vertex.position.z);
    }
    modelCenter_ = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
    ship_ = std::make_unique<Object3d>();
    ship_->Initialize(objectCommon_.get());
    ship_->SetModel(model);
    ship_->SetLightingMode(1);
    ship_->SetColor({ 0.46f, 0.58f, 0.72f, 1.0f });
    ship_->SetDirectionalLightDirection({ -0.30f, -0.72f, 0.55f });
    ship_->SetDirectionalLightIntensity(1.40f);
    ship_->SetEnvironmentCoefficient(0.11f);
    ship_->SetShininess(160.0f);
    ship_->SetRoughness(0.23f);
    ship_->SetMetallic(0.58f);
    ship_->SetSpecularColor({ 0.72f, 0.82f, 0.94f });
    ship_->SetShadowReceiveStrength(0.0f);

    // 本編で読み込み済みのエフェクトを再利用。汎用GPUパーティクルには接続しない。
    for (size_t index = 0; index < exhaust_.size(); ++index) {
        auto& effect = exhaust_[index];
        effect = std::make_unique<Object3d>();
        effect->Initialize(objectCommon_.get());
        effect->SetModel(ModelManager::GetInstance()->FindModel(index / 2 == 2 ?
            "effect_player_bullet_core" : "effect_player_bullet_trail"));
        effect->SetLightingMode(0);
        effect->SetEnvironmentCoefficient(0.0f);
        effect->SetAlphaReference(0.003f);
    }
    sound_ = std::make_unique<SoundManager>();
    if (sound_->Initialize()) {
        sound_->Load("confirm", "resources/audio/combat/skill_ready.wav", 1, 0.22f);
        sound_->Load("launch", "resources/audio/combat/dodge.wav", 1, 0.48f);
    }
}

void TitleScene::PrepareTitleComposition()
{
    rift_ = std::make_unique<TitleRift>();
    rift_->Initialize(dxCommon_);
}

void TitleScene::UpdateBackdrop()
{
    if (!camera_) { return; }
    const float flight = std::clamp((departureTime_ - 0.12f) / 1.18f, 0.0f, 1.0f);
    departureAcceleration_ = flight * flight;
    camera_->SetAspectRatio(dxCommon_->GetPresentationAspectRatio());
    const float titleFov = 0.62f + 0.12f * departureAcceleration_;
    camera_->SetFovY(titleFov);
    camera_->SetTranslate({ 0, 0, -18 });
    camera_->SetRotate({ 0.02f, 0.0f, -0.10f });
    camera_->Update();
    const auto inView = [&](Math::Vector3 local) {
        const auto offset = RotateVector(local, camera_->GetWorldMatrix());
        const auto& position = camera_->GetTranslate();
        return Math::Vector3{ position.x + offset.x, position.y + offset.y, position.z + offset.z };
    };
    if (departureTime_ == 0.0f) {
        const TitleFlyby flyby = GetTitleFlyby(flybyElapsed_, !showControls_);
        const float fit = (std::min)(camera_->GetAspectRatio() / (1280.0f / 720.0f), 1.0f);
        struct Pose { Math::Vector3 center, rotation; };
        const auto poseAt = [&](float progress) {
            const auto local = GetTitleFlightPosition(progress);
            const auto before = GetTitleFlightPosition((std::max)(0.0f,progress - 0.005f));
            const auto after = GetTitleFlightPosition((std::min)(1.0f,progress + 0.005f));
            const auto direction = RotateVector({(after.x - before.x) * fit,
                (after.y - before.y) * fit,after.z - before.z},camera_->GetWorldMatrix());
            const float bank = 0.24f - 1.02f * Smooth((progress - 0.12f) / 0.26f) +
                1.06f * Smooth((progress - 0.46f) / 0.34f);
            return Pose{inView({local.x * fit,local.y * fit,local.z}),
                FlightRotation(direction,RotateVector({0,1,0},camera_->GetWorldMatrix()),bank)};
        };
        const auto pose = poseAt(flyby.flight);
        const auto& center = pose.center;
        const auto& rotation = pose.rotation;
        const float shipScale = kIdleShipScale * fit;
        const auto rotationMatrix = Math::MakeAffineMatrix({ 1,1,1 }, rotation, {});
        const auto matrix = Math::MakeAffineMatrix({shipScale,shipScale,shipScale},rotation,{});
        const auto offset = RotateVector(modelCenter_, matrix);
        ship_->SetScale({shipScale,shipScale,shipScale});
        ship_->SetRotate(rotation);
        ship_->SetTranslate({ center.x - offset.x, center.y - offset.y, center.z - offset.z });
        ship_->Update();
        const float proximity = std::exp(-std::pow((flyby.flight - kClosestFlight) / 0.18f,2.0f));
        UpdateFlightEffects(center,rotationMatrix,1.4f + 1.6f * proximity,shipScale);
        const float ratio = shipScale / 1.26f;
        for (size_t sample = 0; sample < 6; ++sample) {
            const float age = static_cast<float>(sample) * 0.075f;
            const float pastTime = flyby.phase - kFlybyStart - age;
            const float progress = std::clamp(pastTime / kFlybyDuration,0.0f,1.0f);
            const auto past = poseAt(progress);
            const auto matrixPast = Math::MakeAffineMatrix({1,1,1},past.rotation,{});
            const float pastProximity = std::exp(-std::pow((progress - kClosestFlight) / 0.18f,2.0f));
            const float alpha = pastTime >= 0.0f && pastTime <= kFlybyDuration ?
                std::exp(-age / 0.23f) : 0.0f;
            for (size_t nozzle = 0; nozzle < 2; ++nozzle) {
                const auto offsetPast = RotateVector({(nozzle == 0 ? -0.48f : 0.48f) * ratio,
                    -0.28f * ratio,-1.70f * ratio},matrixPast);
                const auto point = ProjectTitleCanvas({past.center.x + offsetPast.x,
                    past.center.y + offsetPast.y,past.center.z + offsetPast.z},*camera_);
                wakePoints_[nozzle * 6 + sample] = {point.x,point.y,1.0f + pastProximity * 2.0f,alpha};
            }
        }
        return;
    }
    // 出撃用の飛行。待機演出の途中で決定しても、従来の加速カットへ移る。
    const float x = -13.0f + 49.0f * std::pow(flight, 1.55f);
    const Math::Vector3 center = inView({ x, -1.1f + x * 0.18f, 16.0f + 28.0f * flight * flight });
    const auto cameraRotation = camera_->GetRotate();
    const Math::Vector3 rotation{
        cameraRotation.x - 1.05f,
        cameraRotation.y + 0.80f - 0.10f * departureAcceleration_,
        cameraRotation.z - 0.77f + 0.10f * departureAcceleration_ };
    const auto rotationMatrix = Math::MakeAffineMatrix({ 1, 1, 1 }, rotation, {});
    const auto matrix = Math::MakeAffineMatrix({ kShipScale, kShipScale, kShipScale }, rotation, {});
    const auto offset = RotateVector(modelCenter_, matrix);
    ship_->SetScale({ kShipScale, kShipScale, kShipScale });
    ship_->SetRotate(rotation);
    ship_->SetTranslate({ center.x - offset.x, center.y - offset.y, center.z - offset.z });
    ship_->Update();
    UpdateFlightEffects(center, rotationMatrix, 2.0f + 4.0f * departureAcceleration_, kShipScale);
}

void TitleScene::UpdateFlightEffects(const Math::Vector3& center, const Math::Matrix4x4& rotationMatrix, float thrust, float shipScale)
{
    const auto tail = RotateVector({ 0, 0, -1 }, rotationMatrix);
    const auto tailView = RotateVector(tail, camera_->GetViewMatrix());
    Math::Vector3 billboard = camera_->GetRotate();
    billboard.z += std::atan2(-tailView.x, tailView.y);
    const float pulse = 1.0f + 0.045f * std::sin(elapsed_ * 29.0f) + 0.025f * std::sin(elapsed_ * 43.0f);
    for (size_t index = 0; index < exhaust_.size(); ++index) {
        const int layer = static_cast<int>(index / 2);
        // 本編で確認済みのノズル位置をタイトルの表示倍率へ換算する。
        const float ratio = shipScale / 1.26f;
        const auto nozzle = RotateVector({ (index % 2 == 0 ? -0.48f : 0.48f) * ratio,
            -0.28f * ratio, -1.70f * ratio }, rotationMatrix);
        if (index < engines_.size()) {
            const Math::Vector3 position{ center.x + nozzle.x, center.y + nozzle.y, center.z + nozzle.z };
            const auto head = ProjectTitleCanvas(position, *camera_);
            const auto end = ProjectTitleCanvas({ position.x + tail.x * 8.0f,
                position.y + tail.y * 8.0f, position.z + tail.z * 8.0f }, *camera_);
            const float length = (std::max)(0.001f, std::hypot(end.x - head.x, end.y - head.y));
            engines_[index] = { head.x, head.y, (end.x - head.x) / length, (end.y - head.y) / length };
        }
        const float length = (layer == 0 ? 1.25f : 0.78f) * thrust * pulse;
        const float centerOffset = layer == 2 ? 0.025f : length * 0.78f;
        auto& effect = exhaust_[index];
        effect->SetTranslate({ center.x + nozzle.x + tail.x * centerOffset,
            center.y + nozzle.y + tail.y * centerOffset, center.z + nozzle.z + tail.z * centerOffset });
        effect->SetRotate(billboard);
        if (layer == 2) {
            effect->SetScale({ 0.15f * pulse, 0.15f * pulse, 1.0f });
            effect->SetColor({ 0.72f, 0.84f, 1.0f, 0.52f });
        } else {
            effect->SetScale({ (layer == 0 ? 0.20f : 0.095f) * pulse, length, 1.0f });
            effect->SetColor(layer == 0 ? Math::Vector4{ 0.22f, 0.43f, 0.86f, 0.48f } : Math::Vector4{ 0.91f, 0.96f, 1.0f, 0.85f });
        }
        effect->Update();
    }
}

void TitleScene::Update()
{
    const float delta = (dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 0.05f) :
        1.0f / 60.0f) * presentationRate_;
    elapsed_ += delta;
    if (input_ && !startRequested_) {
        if (showControls_) {
            if (MenuUi::Pressed(input_, DIK_ESCAPE) || MenuUi::Pressed(input_, DIK_H) ||
                MenuUi::Pressed(input_, DIK_RETURN)) { showControls_ = false; }
        } else {
            if (MenuUi::Pressed(input_, DIK_UP) || MenuUi::Pressed(input_, DIK_W) || MenuUi::Pressed(input_, DIK_LEFT) || MenuUi::Pressed(input_, DIK_A)) { selectedItem_ = (selectedItem_ + 2) % 3; }
            if (MenuUi::Pressed(input_, DIK_DOWN) || MenuUi::Pressed(input_, DIK_S) || MenuUi::Pressed(input_, DIK_RIGHT) || MenuUi::Pressed(input_, DIK_D)) { selectedItem_ = (selectedItem_ + 1) % 3; }
            if (MenuUi::Pressed(input_, DIK_RETURN)) {
                if (selectedItem_ == 2) { showControls_ = true; }
                else { RequestStart(selectedItem_ == 1); }
            } else if (input_->TriggerKey(DIK_T)) { RequestStart(true); }
            else if (input_->TriggerKey(DIK_H)) { showControls_ = true; }
        }
    }
    // 初回のモデル・シェーダー読み込みは導入後へ回し、一閃の2秒を間延びさせない。
    const bool resourcesReady = elapsed_ >= 2.10f && GameScene::PreloadResourcesStep(dxCommon_, srvManager_);
    PrepareBackdrop();
    if (resourcesReady && sceneManager_ && !sceneManager_->IsScenePrepared(requestedScene_)) {
        sceneManager_->PrepareScene(requestedScene_);
    }
    const bool gameReady = sceneManager_ && sceneManager_->IsScenePrepared(requestedScene_);
    if (gameReady && ship_ && !startRequested_ && !showControls_) {
        flybyElapsed_ = previewFlybyPhase_ >= 0.0f ? previewFlybyPhase_ : std::fmod(flybyElapsed_ + delta, kFlybyLoop);
    }
    if (startRequested_ && gameReady && sceneManager_) {
        if (departureTime_ == 0.0f && sound_) { sound_->Play("launch"); }
        departureTime_ = (std::min)(kDepartureDuration, departureTime_ + delta);
        departureFade_ = Smooth((departureTime_ - 1.37f) / (kDepartureDuration - 1.37f));
        if (departureTime_ >= kDepartureDuration) { sceneManager_->SetNextScene(requestedScene_); }
    }
    UpdateBackdrop();
    DrawMenu(gameReady);
}

void TitleScene::DrawMenu(bool gameReady)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = (std::max)(0.1f, (std::min)(viewport->Size.x / 1280.0f, viewport->Size.y / 720.0f));
    const ImVec2 origin(viewport->Pos.x + (viewport->Size.x - 1280.0f * scale) * 0.5f,
        viewport->Pos.y + (viewport->Size.y - 720.0f * scale) * 0.5f);
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * scale, origin.y + y * scale); };
    const ImVec2 end(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##Title", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (!rift_) { draw->AddRectFilled(viewport->Pos, end, CombatHud::SurfaceColor(IM_COL32(9,17,31,255))); }

    if (!showControls_) {
        const float exit = 1.0f - Smooth(departureTime_ / 0.20f);
        const int firstLogoVertex = draw->VtxBuffer.Size;
        DrawWordmark(draw, p(195, 247), 1.75f * scale, elapsed_,
            GetTitleFlyby(flybyElapsed_, ship_ != nullptr));
        const float subtitleAlpha = Smooth((elapsed_ - 1.00f) / 0.40f);
        const float subtitleSize = 26.0f * scale;
        MenuUi::Text(draw, { p(640, 479).x - MenuUi::Width(kTitleJapanese, subtitleSize) * 0.5f, p(640,479).y },
            subtitleSize, IM_COL32(201,213,230,static_cast<int>(255.0f * subtitleAlpha)), kTitleJapanese);
        for (int index = firstLogoVertex; index < draw->VtxBuffer.Size; ++index) {
            ImU32& color = draw->VtxBuffer[index].col;
            const auto alpha = static_cast<ImU32>(static_cast<float>((color >> IM_COL32_A_SHIFT) & 255) * exit);
            color = (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
        }

        const float menuAlpha = Smooth((elapsed_ - 1.35f) / 0.45f) * exit;
        const int firstMenuVertex = draw->VtxBuffer.Size;
        constexpr std::array<float, 3> positions{ 277.0f, 541.0f, 805.0f };
        const char* labels[] = { "出撃", "チュートリアル", "操作方法" };
        const float blend = 1.0f - std::exp(-12.0f * std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f));
        for (int index = 0; index < 3; ++index) {
            const float x = positions[index], y = 608.0f;
            ImGui::SetCursorScreenPos(p(x, y));
            ImGui::BeginDisabled(startRequested_ || menuAlpha < 0.05f);
            const bool clicked = ImGui::InvisibleButton(labels[index], { 236.0f * scale, 48.0f * scale });
            if (ImGui::IsItemHovered() && ImGui::IsMousePosValid() &&
                (ImGui::GetIO().MouseDelta.x != 0.0f || ImGui::GetIO().MouseDelta.y != 0.0f)) { selectedItem_ = index; }
            ImGui::EndDisabled();
            if (clicked) {
                selectedItem_ = index;
                if (index == 2) { showControls_ = true; }
                else { RequestStart(index == 1); }
            }
            const float target = selectedItem_ == index ? 1.0f : 0.0f;
            menuEmphasis_[index] += (target - menuEmphasis_[index]) * blend;
            const float emphasis = menuEmphasis_[index];
            const std::array<ImVec2, 4> button{ p(x + 6,y), p(x + 236,y), p(x + 230,y + 48), p(x,y + 48) };
            draw->AddConvexPolyFilled(button.data(), 4,
                CombatHud::SurfaceColor(IM_COL32(244,245,247,static_cast<int>(245.0f * emphasis))));
            const float fontSize = 22.0f * scale;
            const float textX = p(x + 118, y + 11).x - MenuUi::Width(labels[index], fontSize) * 0.5f;
            MenuUi::Text(draw, { textX, p(x,y + 11).y }, fontSize,
                CombatHud::Mix(IM_COL32(184,200,219,255), MenuUi::Ink, emphasis), labels[index]);
        }
        if (!gameReady) {
            MenuUi::Text(draw, p(1195,680), 16.0f * scale, IM_COL32(113,133,159,255), "読み込み中", true);
        }
        for (int index = firstMenuVertex; index < draw->VtxBuffer.Size; ++index) {
            ImU32& color = draw->VtxBuffer[index].col;
            const auto alpha = static_cast<ImU32>(static_cast<float>((color >> IM_COL32_A_SHIFT) & 255) * menuAlpha);
            color = (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
        }
    }
    if (departureFade_ > 0.0f) {
        draw->AddRectFilled(viewport->Pos, end,
            CombatHud::SurfaceColor(IM_COL32(247,248,250,static_cast<int>(255.0f * departureFade_))));
    }
    ImGui::End();
    ImGui::PopStyleVar();
    if (showControls_ && MenuUi::Controls(viewport->Pos, viewport->Size)) { showControls_ = false; }
}

void TitleScene::Draw()
{
    TitleFlyby flyby = GetTitleFlyby(flybyElapsed_, ship_ && !showControls_);
    flyby.energy *= 1.0f - Smooth(departureTime_ / 0.20f);
    if (rift_) { rift_->Draw(elapsed_, departureTime_, flyby, engines_, wakePoints_); }
    const bool idleFlight = departureTime_ == 0.0f && !showControls_ && flyby.flight > 0.005f && flyby.flight < 0.995f;
    const bool departing = departureTime_ >= 0.12f && departureTime_ <= 1.45f;
    if (ship_ && (idleFlight || departing)) {
        objectCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
        objectCommon_->CommonDrawSetting();
        ship_->Draw();
        objectCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
        objectCommon_->CommonDrawSetting();
        objectCommon_->SetBlendMode(BlendMode::Add);
        objectCommon_->CommonDrawSetting();
        for (auto& effect : exhaust_) { effect->Draw(); }
        objectCommon_->SetBlendMode(BlendMode::Normal);
        objectCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
    }
    if (rift_ && flyby.energy > 0.001f) { rift_->Draw(elapsed_, departureTime_, flyby, engines_, wakePoints_, true); }
}

void TitleScene::Finalize()
{
    // フレーム終端のGPU完了待ち後にシーンが切り替わる。共有モデル・空は解放しない。
    ship_.reset();
    rift_.reset();
    for (auto& effect : exhaust_) { effect.reset(); }
    sound_.reset();
    if (objectCommon_ && srvManager_) { srvManager_->Free(objectCommon_->GetShadowMapSrvIndex()); }
    objectCommon_.reset();
    camera_.reset();
}
