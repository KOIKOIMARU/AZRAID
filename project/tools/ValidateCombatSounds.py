"""戦闘SEのPCM・出典・モノ互換性・重複発音時の余裕を検査し、短い試聴を作る。"""
from pathlib import Path
import hashlib
import json
import math
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

ROOT = Path(__file__).resolve().parents[2]
BANK = ROOT / 'project/resources/audio/combat'
OUT = ROOT / 'generated/audio-review-20261001'
RATE = 48000
# GameRuntimeのロード時設定と同じ。枠数・最短間隔もミックス検査に反映する。
SETTINGS = {
    'shot': (4, .38, .065), 'charge': (2, .51, .16), 'skill_start': (1, .52, .4),
    'hit': (3, .35, .075), 'destroy': (3, .49, .12), 'damage': (2, .60, .15),
    'dodge': (2, .40, .12), 'slash': (3, .57, .06), 'slash_finish': (1, .68, .4),
    'fever': (1, .59, .5), 'skill_ready': (1, .29, .5),
    'clear': (1, .54, 1.), 'fail': (1, .48, 1.),
}
VARIED = ('shot', 'hit', 'destroy', 'slash')
MUSIC_HASHES = {
    'boss': '40edd5cb7dd41a8063614d62188b8d58203132b091eb12c700c6cf6d2cb77f05',
    'fever': '4160e07b8dd5e21399c01577f0c9c3702be7fe2a5bc910392d6181da47343fc8',
    'stage': '1857e2d861d98ac11566425febb6ab6b13690f5d6f7378d9a0977d70fd28321a',
}


def render(events, seconds, *, music=None, priority=True, seed=9):
    """音程変更も含む発音枠の模擬。素材に隠れたクリップを正規化でごまかさない。"""
    result = np.zeros((round(seconds * RATE), 2))
    if music is not None:
        data, rate = sf.read(ROOT / f'project/resources/audio/music/{music}.wav', always_2d=True)
        divisor = math.gcd(rate, RATE)
        data = resample_poly(data, RATE // divisor, rate // divisor, axis=0)
        result += np.resize(data, result.shape) * .32  # BGM側の最大設定値を使う。
    busy = {k: [0.] * v[0] for k, v in SETTINGS.items()}
    last = {k: -10. for k in SETTINGS}
    rng = np.random.default_rng(seed)
    accent_until = 0.
    for time, key, pitch, skill in sorted(events):
        voices, volume, interval = SETTINGS[key]
        if time - last[key] < interval:
            continue
        free = next((i for i, end in enumerate(busy[key]) if end <= time), None)
        if free is None:
            continue
        suffix = ('', '_02', '_03')[rng.integers(3)] if key in VARIED else ''
        data, rate = sf.read(BANK / f'{key}{suffix}.wav', always_2d=True)
        ratio = round(pitch * 10000)
        divisor = math.gcd(10000, ratio)
        data = resample_poly(data, 10000 // divisor, ratio // divisor, axis=0)
        gain = 1.
        if priority:
            accent = time < accent_until
            if key == 'shot': gain = .38 if skill else .58 if accent else 1.
            if key == 'hit': gain = .45 if skill else .68 if accent else 1.
            if key == 'destroy': gain = .24 if skill else .56 if accent else 1.
        hold = {'slash_finish': .55, 'fever': .48, 'skill_start': .25, 'damage': .22, 'charge': .18}.get(key, 0.)
        accent_until = max(accent_until, time + hold)
        first = round(time * RATE)
        length = min(len(data), len(result) - first)
        result[first:first+length] += data[:length] * volume * gain * 1.05
        busy[key][free] = time + len(data) / RATE
        last[key] = time
    return result * .60  # 既存のMaster音量。BGM設定は変えない。


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    report = []
    for path in sorted(BANK.glob('*.wav')):
        data, rate = sf.read(path, always_2d=True)
        assert rate == RATE and data.shape[1] == 2 and sf.info(path).subtype == 'PCM_16'
        assert np.isfinite(data).all() and .03 < len(data)/rate < 1.1
        peak = float(abs(data).max())
        rms = float(np.sqrt(np.mean(data**2)))
        assert peak <= .701 and rms > .02, (path.name, peak, rms)
        assert abs(data[[0, -1]]).max() == 0, f'click edge: {path.name}'
        assert abs(data.mean(axis=0)).max() < .001, f'DC bias: {path.name}'
        mono = float(np.sqrt(np.mean(data.mean(axis=1)**2))) / rms
        assert mono > .70, f'phase cancellation: {path.name}'
        report.append(dict(name=path.name, seconds=len(data)/rate, peak=peak, rms=rms, mono_ratio=mono))
    assert len(report) == 21 and sum(s[0] for s in SETTINGS.values()) == 25
    for key in VARIED:
        assert len({hashlib.sha256((BANK/f'{key}{s}.wav').read_bytes()).hexdigest()
                    for s in ('', '_02', '_03')}) == 3
    manifest = json.loads((BANK/'source_manifest.json').read_text(encoding='utf-8'))
    for relative, digest in manifest['sources_sha256'].items():
        assert hashlib.sha256((ROOT/'assets_source'/relative).read_bytes()).hexdigest() == digest
    for key, digest in MUSIC_HASHES.items():
        assert hashlib.sha256((ROOT/f'project/resources/audio/music/{key}.wav').read_bytes()).hexdigest() == digest

    events = [(float(t), key, 1., 4.9 < t < 7.)
              for key, start, interval in [('shot', .1, .067), ('hit', .2, .078), ('destroy', .5, .13)]
              for t in np.arange(start, 11., interval)]
    events += [(5., 'skill_start', 1., True), (6.9, 'slash_finish', 1., True),
               (8., 'fever', 1., False), (3., 'damage', 1., False), (9., 'charge', 1., False)]
    events += [(float(t), 'slash', .98 + (i % 5) * .035, True) for i, t in enumerate(np.arange(5.2, 6.8, .117))]
    stress_peaks = []
    for music in ('stage', 'boss', 'fever'):
        for priority in (False, True):
            mix = render(events, 12., music=music, priority=priority)
            peak = float(abs(mix).max())
            assert peak < .95, (music, priority, peak)
            stress_peaks.append(dict(music=music, priority=priority, peak=peak))
    sf.write(OUT/'combat_mix_stress.wav', mix, RATE, subtype='PCM_16')
    # 0秒: 通常射撃、2秒: チャージ、3.5秒: 残像連撃、6秒: フィーバー。
    demo = [(t, 'shot', 1., False) for t in (0., .18, .36, .54)]
    demo += [(t + .08, 'hit', 1., False) for t in (0., .18, .36)]
    demo += [(1., 'destroy', 1., False), (2., 'charge', 1., False), (3.5, 'skill_start', 1., True)]
    demo += [(3.7 + i*7/60, 'slash', .98 + i*.035, True) for i in range(5)]
    demo += [(4.45, 'slash_finish', 1., True), (6., 'fever', 1., False)]
    preview = render(demo, 7.4)
    sf.write(OUT/'Combat_preview.wav', preview, RATE, subtype='PCM_16')
    (OUT/'validation.json').write_text(json.dumps(dict(files=report, stress=stress_peaks,
        music_unchanged=True, sfx_voices=25, music_voices=3), indent=2)+'\n', encoding='utf-8')
    print(f'AUDIO_QA_PASS files=21 source_hashes={len(manifest["sources_sha256"])} '
          f'stress_peak={max(x["peak"] for x in stress_peaks):.3f} music_unchanged=1 voices=28')


if __name__ == '__main__':
    main()
