"""CC0素材を編集・重ね合わせして、SKYBREAKの戦闘SEへ仕上げる。

python GenerateCombatSounds.py <旧素材フォルダ> --new-source <追加素材フォルダ> [--preview <試聴wav>]
オフライン専用: numpy / scipy / soundfile。ゲームは完成済みPCMだけを使う。
素材の取得先は resources/audio/combat/CREDITS.md を参照。
"""
import argparse
import hashlib
import json
import math
import shutil
from pathlib import Path
import numpy as np
import soundfile as sf
from scipy.signal import butter, resample_poly, sosfilt

RATE = 48000
OUTPUT = Path(__file__).resolve().parents[1] / 'resources/audio/combat'
PACKS = {'sci': 'kenney_scifi/Audio', 'metal': 'kenney_impact/Audio',
         'rpg': 'kenney_rpg/Audio', 'ui': 'kenney_interface/Audio', 'duck': 'rubberduck_scifi',
         'weapon': 'lentikula', 'blade': 'sword/sword - StarNinjas', 'clash': 'clash'}


class Editor:
    def __init__(self, source, new_source):
        self.source, self.new_source = source, new_source
        self.sources, self.recipes, self.current = {}, {}, None

    def clip(self, pack, name, *, speed=1.0, duration=None, start=0.0,
             high=45.0, low=12000.0, width=0.6, transient=False, decay=None, reverse=False):
        extension = 'wav' if pack == 'weapon' else 'ogg'
        root = self.new_source if pack in ('weapon', 'blade', 'clash') else self.source
        relative = f'{root.name}/{PACKS[pack]}/{name}.{extension}'
        path = root / PACKS[pack] / f'{name}.{extension}'
        if relative not in self.sources:
            self.sources[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
        self.recipes[self.current].append(relative)
        data, rate = sf.read(path, always_2d=True, dtype='float64')
        if data.shape[1] == 1:
            data = np.repeat(data, 2, axis=1)
        assert data.shape[1] == 2 and np.isfinite(data).all()
        gcd = math.gcd(RATE, rate)
        data = resample_poly(data, RATE // gcd, rate // gcd, axis=0)
        # 先頭の録音待ちだけ除去し、風切り音などの立ち上がりは残す。
        threshold = max(0.0001, float(abs(data).max()) * 0.006)
        active = np.flatnonzero(abs(data).max(axis=1) >= threshold)
        if not len(active):
            raise ValueError(f'Silent source: {path}')
        data = data[max(0, int(active[0]) - 96):]
        if transient:
            # 素材の長い予備音を避け、画面の発射/接触から8ms以内にアタックを置く。
            peak_index = int(np.argmax(np.max(abs(data[:round(RATE * 0.65)]), axis=1)))
            data = data[max(0, peak_index - round(RATE * 0.008)):]
        data = data[round(start * RATE):]
        data = resample_poly(data, 1000, round(speed * 1000), axis=0)
        if duration is not None:
            data = data[:round(duration * RATE)]
        assert len(data) > 64
        data = sosfilt(butter(2, high, 'highpass', fs=RATE, output='sos'), data, axis=0)
        data = sosfilt(butter(2, low, 'lowpass', fs=RATE, output='sos'), data, axis=0)
        middle = data.mean(axis=1, keepdims=True)
        data = middle + (data - middle) * width
        data /= max(float(abs(data).max()), 0.001)
        if decay:
            data *= np.exp(-np.arange(len(data)) / (RATE * decay))[:, None]
        if reverse:
            data = data[::-1].copy()
        attack, tail = min(96, len(data) // 4), min(round(0.025 * RATE), len(data) // 4)
        data[:attack] *= np.linspace(0, 1, attack)[:, None]
        data[-tail:] *= np.linspace(1, 0, tail)[:, None]
        return data

    def begin(self, name):
        self.current = name
        self.recipes[name] = []

    def finish(self, name, seconds, layers, *, peak=0.63, room=0.0):
        result = np.zeros((round(seconds * RATE), 2))
        for clip, gain, offset in layers:
            first = round(offset * RATE)
            count = min(len(clip), len(result) - first)
            result[first:first + count] += gain * clip[:count]
        # 連射は乾いた短音、撃破・決め技だけに短い反射音を足す。
        if room:
            dry = result.copy()
            for delay, level in ((0.029, 0.23), (0.047, 0.15), (0.079, 0.08)):
                n = round(delay * RATE)
                result[n:] += dry[:-n, ::-1] * room * level
        result = sosfilt(butter(2, 35, 'highpass', fs=RATE, output='sos'), result, axis=0)
        # 強い部分だけを穏やかに丸める。小さなピークのために全体が痩せるのを防ぐ。
        result /= max(float(abs(result).max()), 1e-9)
        result = np.tanh(result * 1.25) / np.tanh(1.25) * peak
        result[:96] *= np.linspace(0, 1, 96)[:, None]
        tail = min(round(0.060 * RATE), len(result) // 4)
        result[-tail:] *= np.linspace(1, 0, tail)[:, None]
        result[0] = result[-1] = 0
        assert np.isfinite(result).all() and abs(result).max() <= 0.72
        OUTPUT.mkdir(parents=True, exist_ok=True)
        sf.write(OUTPUT / f'{name}.wav', result, RATE, subtype='PCM_16')
        print(f'{name:14s} {seconds:.3f}s peak={abs(result).max():.3f} rms={np.sqrt(np.mean(result ** 2)):.3f}')
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--new-source', type=Path, required=True)
    parser.add_argument('--preview', type=Path)
    args = parser.parse_args()
    e, results = Editor(args.source, args.new_source), {}
    c = e.clip
    for take in range(3):
        suffix = '' if take == 0 else f'_{take + 1:02d}'
        name = 'shot' + suffix
        e.begin(name)
        results[name] = e.finish(name, 0.22, [
            (c('weapon', 'Laser Pistol 1', speed=1.06 + take * 0.025, transient=True,
               duration=0.21, low=9200, high=100, width=0.25, decay=0.065), 0.88, 0),
            (c('metal', f'impactMetal_heavy_{[0,2,4][take]:03d}', transient=True,
               duration=0.07, low=1700, width=0.15, decay=0.025), 0.14, 0),
        ], peak=0.58)
        name = 'hit' + suffix
        e.begin(name)
        results[name] = e.finish(name, 0.19, [
            (c('clash', f'sword_clash.{[2,4,7][take]}', speed=1.15, transient=True,
               duration=0.17, high=550, low=8500, decay=0.055), 0.72, 0),
            (c('metal', f'impactMetal_medium_{[1,2,4][take]:03d}', transient=True,
               duration=0.09, high=100, low=2400, decay=0.035), 0.32, 0),
        ], peak=0.57)
        name = 'destroy' + suffix
        e.begin(name)
        results[name] = e.finish(name, 0.63, [
            (c('sci', f'explosionCrunch_{[0,2,3][take]:03d}', speed=1.04 + 0.025 * take,
               transient=True, duration=0.50, high=90, low=5600, decay=0.19), 0.50, 0),
            (c('sci', 'lowFrequency_explosion_001', speed=1.12, transient=True,
               duration=0.37, high=45, low=600, decay=0.15), 0.50, 0),
            (c('metal', f'impactGlass_medium_{take:03d}', speed=1.05, transient=True,
               duration=0.41, high=1700, low=8800, width=0.9, decay=0.18), 0.28, 0.035),
        ], peak=0.67, room=0.25)
        name = 'slash' + suffix
        e.begin(name)
        results[name] = e.finish(name, 0.25, [
            (c('blade', f'sword.{[6,4,3][take]}', speed=1.13, transient=True,
               duration=0.23, high=550, low=10500, width=0.7, decay=0.085), 0.82, 0),
            (c('weapon', 'Laser Pistol 4', speed=1.40, transient=True,
               duration=0.13, high=450, low=4600, decay=0.043), 0.19, 0),
        ], peak=0.64, room=0.12)
    e.begin('charge')
    results['charge'] = e.finish('charge', 0.60, [
        (c('weapon', 'Laser Rifle 2', speed=0.95, transient=True, duration=0.55,
           high=70, low=9000, decay=0.20), 0.83, 0),
        (c('sci', 'lowFrequency_explosion_001', speed=1.15, transient=True,
           duration=0.24, low=480, decay=0.10), 0.30, 0),
    ], peak=0.67, room=0.18)
    e.begin('skill_start')
    results['skill_start'] = e.finish('skill_start', 0.29, [
        (c('blade', 'sword.6', speed=1.6, duration=0.16, high=850, low=9500,
           reverse=True, width=0.9), 0.30, 0),
        (c('clash', 'sword_clash.6', speed=1.25, transient=True, duration=0.16,
           high=1800, low=10000, decay=0.065), 0.66, 0.09),
    ], peak=0.60, room=0.22)
    e.begin('damage')
    results['damage'] = e.finish('damage', 0.42, [
        (c('sci', 'impactMetal_003', speed=0.80, transient=True, duration=0.30, high=90, low=4700), 0.65, 0),
        (c('metal', 'impactPlate_heavy_001', speed=0.85, transient=True, duration=0.25, low=3500), 0.38, 0),
    ], peak=0.68)
    e.begin('dodge')
    results['dodge'] = e.finish('dodge', 0.29, [
        (c('rpg', 'knifeSlice', start=0.08, speed=1.15, duration=0.28, high=350, low=5500, width=0.9), 0.40, 0),
        (c('sci', 'thrusterFire_002', start=0.03, speed=1.25, duration=0.24, high=250, low=4800), 0.60, 0),
    ], peak=0.56)
    e.begin('slash_finish')
    results['slash_finish'] = e.finish('slash_finish', 0.95, [
        (c('blade', 'sword.7', speed=0.78, transient=True, duration=0.64,
           high=450, low=10000, width=0.9, decay=0.30), 0.72, 0),
        (c('sci', 'lowFrequency_explosion_000', speed=1.10, transient=True,
           duration=0.65, high=45, low=900, width=0.5, decay=0.24), 0.48, 0.012),
        (c('metal', 'impactGlass_medium_002', speed=0.90, transient=True,
           duration=0.62, high=2100, low=10000, width=0.9, decay=0.26), 0.26, 0.06),
    ], peak=0.70, room=0.55)
    e.begin('fever')
    results['fever'] = e.finish('fever', 1.05, [
        (c('weapon', 'Laser Beam 1', start=0.08, speed=1.3, duration=0.16,
           high=600, low=7000, reverse=True, width=0.9), 0.26, 0),
        (c('sci', 'forceField_004', speed=1.1, transient=True, duration=0.78,
           high=500, low=8000, width=0.95, decay=0.34), 0.52, 0.14),
        (c('sci', 'lowFrequency_explosion_001', speed=0.9, transient=True,
           duration=0.52, low=600, decay=0.23), 0.42, 0.14),
        (c('clash', 'sword_clash.6', speed=1.5, transient=True,
           duration=0.46, high=2600, low=10000, width=0.9, decay=0.20), 0.20, 0.15),
    ], peak=0.68, room=0.55)
    e.begin('skill_ready')
    results['skill_ready'] = e.finish('skill_ready', 0.34, [
        (c('clash', 'sword_clash.6', speed=1.70, transient=True, duration=0.30,
           high=2500, low=8500, decay=0.085), 0.68, 0),
    ], peak=0.43, room=0.25)
    e.begin('clear')
    results['clear'] = e.finish('clear', 1.00, [
        (c('ui', 'confirmation_004', speed=0.85, duration=0.75, high=120, low=7500), 0.70, 0),
        (c('ui', 'glass_004', speed=0.80, duration=0.65, high=250, low=7200), 0.26, 0.09),
    ], peak=0.61, room=0.8)
    e.begin('fail')
    results['fail'] = e.finish('fail', 0.85, [
        (c('ui', 'error_004', speed=0.75, duration=0.72, high=100, low=3600), 0.56, 0),
        (c('sci', 'forceField_000', speed=0.80, duration=0.55, high=100, low=1300), 0.22, 0.04),
    ], peak=0.57)
    licenses = OUTPUT / 'licenses'
    licenses.mkdir(exist_ok=True)
    for pack in ('kenney_scifi', 'kenney_impact', 'kenney_rpg', 'kenney_interface'):
        shutil.copyfile(args.source / pack / 'License.txt', licenses / f'{pack}.txt')
    shutil.copyfile(args.new_source / 'lentikula/Readme.txt', licenses / 'lentikula.txt')
    (OUTPUT / 'source_manifest.json').write_text(json.dumps({
        'sources_sha256': e.sources, 'output_sources': e.recipes,
        'format': '48000 Hz, stereo, PCM16',
        'revision': '2026-10-01 SF / blade',
    }, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if args.preview:
        order = ['shot', 'shot_02', 'shot_03', 'hit', 'destroy', 'charge', 'damage', 'dodge',
                 'skill_start', 'slash', 'slash_02', 'slash_03', 'slash_finish', 'fever', 'skill_ready', 'clear', 'fail']
        sequence = []
        for name in order:
            sequence.extend([results[name] * 0.6, np.zeros((round(RATE * 0.24), 2))])
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        sf.write(args.preview, np.concatenate(sequence), RATE, subtype='PCM_16')


if __name__ == '__main__':
    main()
