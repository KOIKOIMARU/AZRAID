"""Sonniss素材から残像連撃のローカル試聴バンクを生成する。公開用WAVは変更しない。

python project/tools/PreparePhantomAudio.py
必要ライブラリ: numpy / scipy / soundfile。出力はGit管理外のgenerated/のみ。
素材は各自が利用条件を確認して取得すること。自動ダウンロード・再配布は行わない。
"""
from pathlib import Path
import hashlib
import json
import math
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'assets_source/audio_cinematic_20261001'
BANK = ROOT / 'generated/audio-phantom-runtime'
REVIEW = ROOT / 'generated/audio-phantom-review-20261001'
ORIGINAL = ROOT / 'project/resources/audio/combat'
RATE = 48000
SOURCES = {
    'slash': ('SWSH_Sword Slash Impact V2 Assorted 18_DDUMAIS_NONE.wav',
              'David Dumais Audio - Melee Weapons Sound Effects Pack 1',
              '8369023f1d186a641c20d0480bd73e1492d9cb9ba4e321cdc83022a9a9aa6d2d'),
    'impact': ('Impact 038.wav', '344 Audio - Epic Impacts Vol. 1',
               '98d4b43b9616826b330b113b058f85e65661acef87114c6df944312a78e4169a'),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    x, rate = sf.read(path, always_2d=True, dtype='float64')
    assert x.shape[1] == 2 and np.isfinite(x).all(), path
    divisor = math.gcd(rate, RATE)
    return resample_poly(x, RATE // divisor, rate // divisor, axis=0)


def speed(x, pitch):
    ratio = round(pitch * 10000)
    divisor = math.gcd(10000, ratio)
    return resample_poly(x, 10000 // divisor, ratio // divisor, axis=0)


def fade(x, release, attack=.001):
    x = x.copy()
    a, r = min(round(attack * RATE), len(x)), min(round(release * RATE), len(x))
    envelope = np.ones(len(x))
    envelope[:a] *= np.linspace(0, 1, a)
    envelope[-r:] *= np.linspace(1, 0, r)
    # 短く切った波形のDC偏りだけを除く。端のゼロと元の強弱を両立する。
    x -= np.average(x, axis=0, weights=envelope)
    x *= envelope[:, None]
    x[0] = x[-1] = 0
    return x


def level(x, peak):
    # 一定倍率だけで音量を合わせ、波形の山を潰さない。
    return x * (peak / max(float(abs(x).max()), 1e-9))


def stats(x):
    rms = float(np.sqrt(np.mean(x*x)))
    peak = float(abs(x).max())
    energy = np.convolve(np.mean(x*x, axis=1), np.ones(144) / 144, 'same')
    return dict(seconds=len(x)/RATE, peak=peak, rms=rms,
                crest_db=20*math.log10(max(peak, 1e-9)/max(rms, 1e-9)),
                energy_peak_ms=int(np.argmax(energy))/RATE*1000,
                mono_ratio=float(np.sqrt(np.mean(x.mean(axis=1)**2)))/max(rms, 1e-9))


def write(path, x):
    assert np.isfinite(x).all() and abs(x).max() < .95, path
    sf.write(path, x, RATE, subtype='PCM_16')
    decoded, rate = sf.read(path, always_2d=True)
    assert rate == RATE and np.max(abs(decoded-x)) < 1/32768 + 1e-9
    assert abs(decoded[[0, -1]]).max() == 0, path
    assert abs(decoded.mean(axis=0)).max() < .002, path
    result = stats(decoded)
    assert result['mono_ratio'] > .70, path
    return dict(file=path.name, sha256=digest(path), **result)


def render(bank, *, count, local, stress=False):
    """本編の0.2秒予備動作・7/60秒間隔・発音枠を使った試聴。実録音ではない。"""
    duration = 4.0
    mix = np.zeros((round(duration*RATE), 2))
    events = []
    activation = .30
    events.append((activation, read(ORIGINAL/'skill_start.wav'), .52))
    voice_until = [0., 0., 0.]
    played = 0
    for i in range(count):
        time = activation + (12 + i*7)/60
        pitch = .98 + .035*i
        suffix = ('', '_02', '_03')[i % 3]
        wave = speed(bank['slash'+suffix], pitch)
        voice = next((j for j, end in enumerate(voice_until) if end <= time), None)
        assert voice is not None, 'Slash tail exceeds the three-voice pool'
        voice_until[voice] = time + len(wave)/RATE
        events.append((time, wave, .66 if local else .57))
        played += 1
    finish = activation + (12 + count*7)/60
    events.append((finish, bank['slash_finish'], .78 if local else .68))
    if stress:
        # BGM上限＋重なる通常SE・被弾を加えて、素材のピークだけでは見えない過大入力を検査。
        music = read(ROOT/'project/resources/audio/music/fever.wav')
        mix += np.tile(music, (math.ceil(len(mix)/len(music)), 1))[:len(mix)] * .32
        for key, interval, volume, duck, voices in (
            ('shot', .065, .38, .38, 4), ('hit', .075, .35, .45, 3),
            ('destroy', .12, .49, .24, 3)):
            wave = read(ORIGINAL/f'{key}.wav')
            busy = [0.]*voices
            for time in np.arange(activation, finish+.55, interval):
                voice = next((j for j, end in enumerate(busy) if end <= time), None)
                if voice is None: continue
                busy[voice] = time + len(wave)/RATE
                events.append((time, wave, volume*duck*1.05))
        events.append((finish, read(ORIGINAL/'damage.wav'), .60*1.015))
    for time, wave, gain in events:
        first = round(time*RATE)
        n = min(len(wave), len(mix)-first)
        mix[first:first+n] += wave[:n] * gain
    mix *= .60  # 現在のSoundManagerのマスター音量。試聴だけの音圧補正はしない。
    assert abs(mix).max() < .90, 'Insufficient headroom in the simulated mix'
    # ループBGMを含む確認用WAVの開始・終了だけをフェードする。
    edge = round(.020*RATE)
    mix[:edge] *= np.linspace(0, 1, edge)[:, None]
    mix[-edge:] *= np.linspace(1, 0, edge)[:, None]
    return mix, dict(slashes=played, finish=1, peak=float(abs(mix).max()))


def main():
    # 元素材・組み込み済みバンク・BGMの保全を生成の前後で検証する。
    preserved = list(ORIGINAL.rglob('*')) + list((ROOT/'project/resources/audio/music').glob('*.wav'))
    before = {p: digest(p) for p in preserved if p.is_file()}
    source = {}
    for key, (name, _, expected) in SOURCES.items():
        path = SOURCE/name
        assert digest(path) == expected, f'Source differs from the reviewed material: {name}'
        source[key] = read(path)
    BANK.mkdir(parents=True, exist_ok=True)
    REVIEW.mkdir(parents=True, exist_ok=True)
    bank, outputs = {}, []
    # 135ms付近の接触を約10msに寄せ、重い中域は最初の70msに残す。
    # 3種は同じ録音の編集違い。別録音のテイクとは扱わない。
    for i, (pitch, seconds) in enumerate(((1., .27), (1.025, .25), (.98, .29))):
        name = 'slash' + ('', '_02', '_03')[i]
        x = speed(source['slash'][round(.125*RATE):], pitch)[:round(seconds*RATE)]
        t = np.arange(len(x))/RATE
        x *= np.exp(-np.maximum(t-.070, 0)/.075)[:, None]
        x = level(fade(x, .050), .76)
        bank[name] = x
        outputs.append(write(BANK/f'{name}.wav', x))
    # 接触の鋭さに、Impact 038に含まれる低域・空間成分を重ねる。合成低音は加えない。
    finish = np.zeros((round(1.35*RATE), 2))
    body = level(source['impact'][:len(finish)], .72)
    finish[:len(body)] += body
    edge = source['slash'][round(.125*RATE):round(.435*RATE)]
    edge = level(fade(edge, .17), .30)
    finish[:len(edge)] += edge
    bank['slash_finish'] = level(fade(finish, .40), .82)
    outputs.append(write(BANK/'slash_finish.wav', bank['slash_finish']))
    original = {key: read(ORIGINAL/f'{key}.wav') for key in bank}
    comparisons, simulations = [], []
    for count, name in ((3, '01_normal_comparison.wav'), (5, '02_fever_comparison.wav')):
        old, _ = render(original, count=count, local=False)
        new, result = render(bank, count=count, local=True)
        stress, stress_result = render(bank, count=count, local=True, stress=True)
        comparisons.append(write(REVIEW/name, np.concatenate((old, np.zeros((RATE, 2)), new))))
        simulations.append(dict(count=count, clean=result, with_music_and_effects=stress_result))
        write(REVIEW/f'{count}_strikes_mix_check.wav', stress)
    assert before == {p: digest(p) for p in before}, 'Existing audio changed'
    report = dict(
        purpose='Private AZRAID phantom-raid runtime audition; not a redistributable sound library.',
        license='https://sonniss.com/gdc-bundle-license/', license_version='2.0',
        checked='2026-10-01', source_bundle='https://sonniss.com/gameaudiogdc/',
        redistribution='Keep source and edited WAVs out of the public repository. Finished-game distribution is a separate workflow.',
        sources={key:dict(file=v[0], author_pack=v[1], sha256=v[2]) for key,v in SOURCES.items()},
        editing='Transient-aligned cuts, modest resampling, tail envelopes, DC offset removal, linear mixing and gain; no saturation, compression or synthetic bass.',
        spatial_audio='Original stereo retained; no positional audio, distance attenuation or HRTF.',
        format='48000 Hz, stereo, PCM16', runtime_outputs=outputs,
        comparison_order='Current in-game CC0 bank, one second silence, local cinematic bank. Same game timing and master gain; simulated, not loopback capture.',
        comparisons=comparisons, simulations=simulations, existing_audio_unchanged=True,
        listening_status='Numerical checks only; subjective headphone evaluation remains with the user.')
    (BANK/'manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    (REVIEW/'review_manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(dict(outputs=outputs, simulations=simulations, existing_audio_unchanged=True), indent=2))


if __name__ == '__main__':
    main()
