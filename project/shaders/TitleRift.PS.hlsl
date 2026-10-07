#include "Fullscreen.hlsli"

cbuffer TitleComposition : register(b0)
{
    float2 resolution;
    float elapsed;
    float departure;
    float4 flyby; // 光位置、航跡の余韻、3パターンの時計（+24:横断面）、航跡モード（1:左右、2:遠方）。
    float4 engines[2]; // 実モデルの左右ノズルの画面位置と、噴射方向。
    float4 wakePoints[12]; // 近距離は左右6点、遠方は1本12点の実飛行位置、幅、強度。
};

float Ease(float t)
{
    t = saturate(t);
    return t * t * (3.0f - 2.0f * t);
}

float CutY(float x)
{
    bool alternate = flyby.z >= 24.0f;
    float slope = alternate ? 0.15f : -0.11f;
    float intercept = (alternate ? 12.0f : 80.0f) + slope * 255.0f - 50.0f;
    float q = (x - 195.0f) / 1.75f - 255.0f;
    float localX = (q - intercept * 0.0998334f) / (0.9950042f + slope * 0.0998334f - q * 0.00042f);
    float localY = intercept + slope * localX;
    return 247.0f + (50.0f + (-localX * 0.0998334f + localY * 0.9950042f) /
        (1.0f + localX * 0.00042f)) * 1.75f;
}

float DepartureCutDistance(float2 screen, float2 offset, float2 drift, float angle, float zoom)
{
    float2 local = screen - offset - float2(641.25f,334.5f) - drift;
    float2 neutral = float2(641.25f,334.5f) + float2(local.x * cos(angle) + local.y * sin(angle),
        -local.x * sin(angle) + local.y * cos(angle)) / zoom;
    return neutral.y - CutY(neutral.x);
}

float2 WakeCurve(float2 previous, float2 start, float2 end, float2 next, float t)
{
    float2 tangentStart = (end - previous) * 0.5f;
    float2 tangentEnd = (next - start) * 0.5f;
    float t2 = t * t;
    float t3 = t2 * t;
    return (2.0f * t3 - 3.0f * t2 + 1.0f) * start + (t3 - 2.0f * t2 + t) * tangentStart +
        (-2.0f * t3 + 3.0f * t2) * end + (t3 - t2) * tangentEnd;
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float fit = min(resolution.x / 1280.0f, resolution.y / 720.0f);
    float2 canvas = (input.texcoord * resolution - (resolution - float2(1280,720) * fit) * 0.5f) / fit;
    float2 screenCanvas = canvas;
    float2 centered = (canvas - float2(640,340)) / float2(760,440);
    float vignette = saturate(1.0f - dot(centered, centered) * 0.38f);
    float3 color = lerp(float3(0.0014f,0.0030f,0.0080f),
        float3(0.0050f,0.0150f,0.0400f), vignette);

    // TitleScene.cppのロゴと同じ移動・反動を逆算し、断面の光を追従させる。
    float phase = fmod(flyby.z,8.0f);
    int pattern = int(flyby.z / 8.0f) % 3;
    float dashDirection = pattern == 1 ? -1.0f : 1.0f;
    float dashOrigin = pattern == 1 ? 1263.0f : 17.0f;
    float firstContact = pattern == 1 ? 1110.0f : 175.0f;
    float dashTime = phase - 3.45f;
    float strikeTime = dashTime - ((firstContact - dashOrigin) * dashDirection - 100.0f) / 1415.0f * 0.24f;
    float responseTime = max(0.0f,strikeTime);
    float impulse = Ease(responseTime / 0.035f) * exp(-max(0.0f,responseTime - 0.085f) / 0.60f) *
        (1.0f + sin(responseTime * 20.0f) * 0.05f);
    float charge = Ease((phase - 2.95f) / 0.50f) * (1.0f - Ease(dashTime / 0.025f));
    float dash = dashTime >= 0.0f ? Ease(dashTime / 0.010f) *
        (1.0f - Ease((dashTime - 0.24f) / 0.12f)) : 0.0f;
    float flash = strikeTime >= 0.0f ? Ease(responseTime / 0.012f) * exp(-responseTime / 0.11f) : 0.0f;
    float afterglow = strikeTime >= 0.0f ? Ease(responseTime / 0.015f) * exp(-responseTime / 1.00f) : 0.0f;
    float ready = Ease((elapsed - 0.35f) / 1.20f);
    float angle = sin(elapsed * 0.54f) * 0.020f * ready;
    float zoom = 1.0f + ready * (sin(elapsed * 0.58f) * 0.012f + charge * 0.016f + impulse * 0.026f);
    float shock = sin(responseTime * 42.0f) * exp(-responseTime / 0.16f) * 7.0f;
    float2 drift = float2(sin(elapsed * 0.43f) * 12.0f * ready + shock,
        sin(elapsed * 0.67f) * 6.0f * ready - impulse * 10.0f + shock * 0.22f);
    float2 moving = canvas - float2(641.25f,334.5f) - drift;
    canvas = float2(641.25f,334.5f) + float2(moving.x * cos(angle) + moving.y * sin(angle),
        -moving.x * sin(angle) + moving.y * cos(angle)) / zoom;
    float neutralX = canvas.x;
    // ロゴと同じ透視・傾きへ戻す。
    float2 relative = canvas - float2(641.25f,334.5f);
    float2 upright = float2(relative.x * 0.9950042f - relative.y * 0.0998334f,
        relative.x * 0.0998334f + relative.y * 0.9950042f);
    canvas = float2(641.25f,334.5f) + upright / max(1.0f - upright.x * 0.00024f, 0.30f);
    bool alternateCut = flyby.z >= 24.0f;
    float cutBase = alternateCut ? 12.0f : 80.0f;
    float cutSlope = alternateCut ? 0.15f : -0.11f;
    float signedDistance = canvas.y - (247.0f + cutBase * 1.75f + (canvas.x - 195.0f) * cutSlope);
    // 溜めは次の一閃の角度を使う。残している前回の断面で予告すると、
    // 横切りへ変わる直前に別の斜線が点灯してしまう。
    float attackBase = pattern == 2 ? 12.0f : 80.0f;
    float attackSlope = pattern == 2 ? 0.15f : -0.11f;
    float attackDistance = canvas.y - (247.0f + attackBase * 1.75f + (canvas.x - 195.0f) * attackSlope);

    if (flyby.w > 0.5f)
    {
        float alpha = 0.0f;
        float white = 0.0f;
        float proximity = dash;
        bool orbitalWake = flyby.w > 1.5f;
        int pointCount = orbitalWake ? 12 : 6;
        [loop]
        for (int nozzle = 0; nozzle < (orbitalWake ? 1 : 2); ++nozzle)
        {
            float2 delta = screenCanvas - engines[nozzle].xy;
            float2 direction = engines[nozzle].zw;
            float behind = dot(delta,direction);
            float distance = dot(delta,float2(-direction.y,direction.x));
            float taper = 1.0f + saturate(behind / 220.0f) * 0.65f;
            float core = exp(-pow(distance / ((1.0f + proximity * 1.4f) * taper),2.0f));
            float halo = exp(-pow(distance / ((5.0f + proximity * 3.0f) * taper),2.0f));
            float wake = smoothstep(3.0f,18.0f,behind) *
                (1.0f - smoothstep(60.0f,115.0f,behind)) * exp(-max(behind,0.0f) / 70.0f);
            alpha = max(alpha,saturate((core * 0.88f + halo * 0.18f) * wake * flyby.y *
                (0.65f + proximity * 0.35f)));
            white = max(white,core);
            float nearestSquared = 100000000.0f;
            float nearestWidth = 1.0f;
            float nearestStrength = 0.0f;
            float travel = 0.0f;
            float nearestTravel = 0.0f;
            int base = nozzle * 6;
            [loop]
            for (int segment = 0; segment < pointCount - 1; ++segment)
            {
                float4 start = wakePoints[base + segment];
                float4 end = wakePoints[base + segment + 1];
                float2 previous = segment > 0 ? wakePoints[base + segment - 1].xy :
                    start.xy * 2.0f - end.xy;
                float2 next = segment < pointCount - 2 ? wakePoints[base + segment + 2].xy :
                    end.xy * 2.0f - start.xy;
                float2 a = start.xy;
                [unroll]
                for (int stepIndex = 0; stepIndex < 3; ++stepIndex)
                {
                    float progress = float(stepIndex + 1) / 3.0f;
                    float2 b = WakeCurve(previous,start.xy,end.xy,next,progress);
                    float2 edge = b - a;
                    float lengthSquared = dot(edge,edge);
                    float t = saturate(dot(screenCanvas - a,edge) / max(lengthSquared,0.01f));
                    float2 toWake = screenCanvas - lerp(a,b,t);
                    float distanceSquared = dot(toWake,toWake);
                    float segmentLength = sqrt(lengthSquared);
                    if (distanceSquared < nearestSquared)
                    {
                        nearestSquared = distanceSquared;
                        float sampleProgress = (float(stepIndex) + t) / 3.0f;
                        nearestWidth = lerp(start.z,end.z,sampleProgress);
                        nearestStrength = lerp(start.w,end.w,sampleProgress) * step(1.0f,lengthSquared);
                        nearestTravel = travel + segmentLength * t;
                    }
                    travel += segmentLength;
                    a = b;
                }
            }
            // 最も近い一片だけ発光を評価。光の帯をノズルから後方へ流し、点列にしない。
            float width = max(nearestWidth,0.5f);
            float historyCore = exp(-nearestSquared / (width * width));
            float haloWidth = width * 4.5f + 7.0f;
            float historyHalo = exp(-nearestSquared / (haloWidth * haloWidth));
            float stream = 0.66f + 0.34f * pow(0.5f + 0.5f * sin(nearestTravel * 0.008f - elapsed * 9.0f),2.0f);
            float strength = nearestStrength * flyby.y * stream;
            alpha = max(alpha,saturate((historyCore * 0.76f + historyHalo * 0.16f) * strength));
            white = max(white,historyCore * strength);
        }
        return float4(lerp(float3(0.07f,0.18f,0.48f),float3(0.92f,0.97f,1.0f),white),alpha);
    }

    float opening = Ease((elapsed - 0.32f) / 0.72f);
    // 機体の背後だけを持ち上げ、白い装甲と暗い空間の輪郭を読ませる。
    float2 heroLight = (screenCanvas - float2(914.0f,143.0f)) / float2(280.0f,145.0f);
    color += float3(0.012f,0.023f,0.047f) * exp(-dot(heroLight,heroLight)) * opening;
    float idleTime = max(elapsed - 2.10f, 0.0f);
    float idle = Ease(idleTime / 1.40f) * (1.0f - Ease(departure / 0.20f));
    float envelope = pow(saturate(1.0f - abs(canvas.x - 640.0f) / 690.0f), 0.58f);
    // 機首が届いた場所から開く。起動直後や通過前には面を動かさない。
    float contactElapsed = dashTime - ((neutralX - dashOrigin) * dashDirection - 100.0f) / 1415.0f * 0.24f;
    float cutPresence = Ease(contactElapsed / 0.012f) * (1.0f - Ease((contactElapsed - 0.85f) / 0.70f));
    float separationTime = max(0.0f,dashTime - (((pattern == 1 ? 175.0f : 1110.0f) - dashOrigin) *
        dashDirection - 100.0f) / 1415.0f * 0.24f);
    float separationPresence = Ease(strikeTime / 0.012f) * (1.0f - Ease((strikeTime - 0.85f) / 0.70f));
    float separationStrength = pattern == 2 ? 88.0f : pattern == 1 ? 70.0f : 49.0f;
    float separation = Ease(separationTime / 0.060f) * exp(-max(0.0f,separationTime - 0.115f) /
        (pattern == 2 ? 0.72f : 0.60f)) * separationStrength * separationPresence;
    // TitleScene.cppのLogoSeparationAxisと同じ移動を、断面の法線へ投影する。
    // 画面端のフェードは明るさだけに使い、縁の位置を曲げない。
    float2 separationAxis = pattern == 2 ? float2(-0.08f,-0.70f) :
        float2(pattern == 1 ? -0.62f : 0.55f,-0.38f);
    float gapScale = abs(separationAxis.y - cutSlope * separationAxis.x) * 1.75f;
    float halfGap = 1.14f * cutPresence + separation * gapScale;
    float aa = max(fwidth(signedDistance), 0.8f);
    float inside = 1.0f - smoothstep(halfGap - aa, halfGap + aa, abs(signedDistance));
    color = lerp(color,float3(0.0007f,0.0015f,0.0040f),inside * opening * cutPresence);

    float upperFace = exp(-abs(signedDistance + halfGap + 38.0f) / 135.0f) * envelope;
    color += float3(0.008f,0.019f,0.042f) * upperFace * opening * cutPresence;
    float edgeDistance = abs(signedDistance + halfGap);
    float edge = 1.0f - smoothstep(0.6f,0.6f + aa,edgeDistance);
    float reflection = exp(-edgeDistance / 15.0f);
    float extent = smoothstep(30.0f,140.0f,canvas.x) * (1.0f - smoothstep(1110.0f,1250.0f,canvas.x));
    color += float3(0.025f,0.039f,0.061f) * edge * extent * opening * cutPresence;

    // 白い噴射の反射が切断面を走る。静かな時間は弱い反射だけを残す。
    float lightX = -220.0f + 1720.0f * frac(idleTime / 3.60f + 0.35f);
    float passed = (flyby.x - screenCanvas.x) * dashDirection;
    float wake = passed > 0.0f ? exp(-passed / 410.0f) * 0.50f : 0.0f;
    float travelingLight = saturate(exp(-pow((canvas.x - lightX) / 130.0f,2.0f)) * idle * 0.32f +
        (exp(-pow(passed / 95.0f,2.0f)) + wake) * dash +
        (charge * 0.18f + flash * 0.72f + afterglow * 0.16f) * idle);
    color += float3(0.65f,0.77f,0.92f) * edge * extent * travelingLight * cutPresence;
    color += float3(0.042f,0.074f,0.150f) * reflection * envelope * travelingLight * cutPresence;
    // 通過後の反射が上下面を押し広げる。中央を白く潰す全画面フラッシュは使わない。
    color += float3(0.010f,0.021f,0.051f) * exp(-abs(signedDistance) / 58.0f) *
        envelope * impulse;
    // 溜めは断面の細い光へ集め、一撃の反射はロゴ周辺だけに残す。
    color += float3(0.015f,0.033f,0.071f) * exp(-abs(attackDistance) / 7.0f) *
        extent * charge * charge * idle;
    color += float3(0.042f,0.061f,0.088f) * exp(-abs(signedDistance) / 32.0f) *
        envelope * flash * idle;

    // 切断で押し出された光が面に沿って抜ける。中心を空け、速さを周辺の遠近差で見せる。
    // 6本は互いに離れているため、各画素で最も近い一本だけを評価する。
    // テクスチャ・粒子・ランタイム確保は増やさない。
    float laneIndex = clamp(round((abs(signedDistance) - 165.0f) / 47.5f),0.0f,2.0f);
    float depth = laneIndex * 0.5f;
    float lane = laneIndex + (signedDistance >= 0.0f ? 3.0f : 0.0f);
    float head = -500.0f + 2400.0f * frac(elapsed * (0.36f + depth * 0.28f) + lane * 0.173f);
    float behind = head - canvas.x;
    float across = abs(signedDistance) - (165.0f + depth * 95.0f);
    float wakeLength = 150.0f + depth * 170.0f + impulse * 240.0f;
    float filament = exp(-pow(across / (0.75f + depth * 1.20f),2.0f));
    float trail = Ease(behind / 24.0f) * (1.0f - Ease(behind / wakeLength));
    float outsideLogo = smoothstep(125.0f,220.0f,abs(signedDistance));
    float flow = filament * trail * outsideLogo * (0.35f + depth * 0.65f);
    color += float3(0.038f,0.053f,0.084f) * flow * idle;
    // 反動の光はロゴ背後から外へ進み、次の溜めへ消える。
    float expanding = abs(signedDistance) - 35.0f - responseTime * 180.0f;
    float pressure = exp(-pow(expanding / 34.0f,2.0f)) * afterglow * envelope * idle;
    color += float3(0.018f,0.030f,0.065f) * pressure;

    if (departure > 0.0f)
    {
        // TitleScene.cppのDepartureOffsetと同じ移動。半透明の隙間には本編を描く。
        float cutTime = departure - 0.24f;
        float chargeUp = Ease(departure / 0.24f) * (1.0f - Ease(cutTime / 0.035f));
        float hit = cutTime >= 0.0f ? Ease(cutTime / 0.014f) * exp(-cutTime / 0.14f) : 0.0f;
        float openTime = max(0.0f,departure - 0.35f);
        float travel = 740.0f * pow(saturate(openTime / 0.65f),1.40f);
        float recoil = Ease(openTime / 0.025f) * exp(-openTime / 0.12f) * 26.0f;
        float2 offset = float2((travel + recoil) * 0.30f,-travel - recoil);
        float cutDistance = DepartureCutDistance(screenCanvas,float2(0,0),drift,angle,zoom);
        float upperDistance = DepartureCutDistance(screenCanvas,offset,drift,angle,zoom);
        float lowerDistance = DepartureCutDistance(screenCanvas,-offset,drift,angle,zoom);
        float gap = max(1.0f,(1.14f + separation * gapScale - chargeUp * 1.4f) * envelope);
        float cutAa = max(fwidth(cutDistance),0.8f);
        float upperPanel = 1.0f - smoothstep(-gap - cutAa,-gap + cutAa,upperDistance);
        float lowerPanel = smoothstep(gap - cutAa,gap + cutAa,lowerDistance);
        float alpha = departure > 0.35f ? saturate(upperPanel + lowerPanel) : 1.0f;
        float movingEdge = min(abs(upperDistance + gap),abs(lowerDistance - gap));
        float cutHead = -120.0f + 1520.0f * Ease(cutTime / 0.11f);
        float completed = Ease((cutHead - screenCanvas.x) / 65.0f);
        float cutActive = cutTime >= 0.0f ? 1.0f - Ease((cutTime - 0.11f) / 0.12f) : 0.0f;
        color *= 1.0f - chargeUp * 0.22f;
        color += float3(0.26f,0.37f,0.56f) * exp(-abs(cutDistance) / 3.0f) * chargeUp * chargeUp;
        color += float3(0.94f,0.98f,1.0f) * exp(-pow(cutDistance / 2.6f,2.0f)) * completed * cutActive;
        color += float3(0.12f,0.21f,0.40f) * exp(-abs(cutDistance) / 34.0f) * hit;
        color += float3(0.70f,0.82f,1.0f) * exp(-movingEdge / 2.0f) * hit;
        color += float3(0.045f,0.078f,0.15f) * exp(-movingEdge / 30.0f) * hit;
        return float4(color,alpha);
    }
    return float4(color,1.0f);
}
