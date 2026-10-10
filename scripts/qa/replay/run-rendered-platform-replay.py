#!/usr/bin/env python3
"""Run a rendered native replay in a private virtual display with exact populated state proof."""
# GeneralsX @build Codex 08/10/2026 Keep replay exit and serialized-state evidence together.
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('executable', 'assets', 'replay', 'expected', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--base-assets', type=Path)
    parser.add_argument('--archive-manifest', type=Path)
    parser.add_argument('--timeout', type=int, default=600)
    args = parser.parse_args()
    for name in ('executable', 'assets', 'replay', 'expected', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    expected = sorted(args.expected.glob('state-??????.trace'))
    if len(expected) != 22 or expected[-1].name != 'state-002100.trace':
        parser.error('The reference must contain all 22 samples through frame 2100')
    frame100 = args.expected / 'state-000100.trace'
    labels = sum(line.startswith('LABEL ') for line in frame100.read_text().splitlines())
    if labels < 200:
        parser.error('Reference world is empty or incomplete')
    args.output.mkdir(parents=True, exist_ok=True)
    if list(args.output.glob('state-*.trace')):
        parser.error('Output already contains traces; choose a fresh output directory')
    archives_ok = True
    if args.archive_manifest:
        archives = json.loads(args.archive_manifest.read_text())
        for name, checksum in archives.items():
            path = args.assets / name
            if not path.is_file() or digest(path) != checksum:
                print(f'ASSET MISMATCH: {name}')
                archives_ok = False
        if not archives_ok:
            return 2
    env = os.environ.copy()
    env['GENERALSX_CRC_TRACE'] = str(args.output / 'state')
    env['CNC_GENERALS_ZH_PATH'] = str(args.assets)
    if args.base_assets:
        env['CNC_GENERALS_PATH'] = str(args.base_assets.resolve())
    if sys.platform.startswith('linux'):
        env['XDG_DATA_HOME'] = str(args.output / 'userdata')
        env.setdefault('ALSOFT_DRIVERS', 'null')
    env['DXVK_LOG_LEVEL'] = 'info'
    env['DXVK_LOG_PATH'] = str(args.output)
    env['GENERALSX_USER_DATA_DIR'] = str(args.output / 'profile')
    env['SDL_VIDEODRIVER'] = 'x11'
    env['SDL_AUDIODRIVER'] = 'dummy'
    started = time.monotonic()
    timed_out = False
    with (args.output / 'replay.log').open('wb') as log:
        try:
            result = subprocess.run(
                [str(args.executable), '-win', '-quickstart', '-nologo', '-noshellmap', '-replay', str(args.replay)],
                cwd=args.assets, env=env, stdout=log, stderr=subprocess.STDOUT,
                timeout=args.timeout,
            )
            exit_code = result.returncode
        except subprocess.TimeoutExpired:
            exit_code = None
            timed_out = True
    log_text = (args.output / 'replay.log').read_text(errors='replace')
    samples = []
    first_difference = None
    for reference in expected:
        actual = args.output / reference.name
        matched = actual.is_file() and digest(actual) == digest(reference)
        samples.append({'frame': int(reference.stem.rsplit('-', 1)[1]), 'present': actual.is_file(), 'match': matched})
        if actual.is_file() and not matched and first_difference is None:
            label = 'before first object'
            from itertools import zip_longest
            for line_number, (left, right) in enumerate(zip_longest(reference.read_text().splitlines(), actual.read_text().splitlines()), 1):
                if left and left.startswith('LABEL '):
                    label = left
                if left != right:
                    first_difference = {'frame': samples[-1]['frame'], 'line': line_number, 'label': label, 'reference': left, 'actual': right}
                    break
    mismatch = re.search(r'REPLAY_CRC_MISMATCH[^\r\n]*', log_text)
    populated = (args.output / 'state-000100.trace').is_file() and sum(line.startswith('LABEL ') for line in (args.output / 'state-000100.trace').read_text().splitlines()) >= 200
    completion = bool(re.search(r'Game Time:\s*01:20/01:20', log_text))
    passed = exit_code == 0 and completion and populated and mismatch is None and all(sample['match'] for sample in samples)
    report = {
        'platform': sys.platform, 'rendered_game': True, 'virtual_display_only': True, 'visual_or_audio_approval': False, 'executable_sha256': digest(args.executable),
        'replay_sha256': digest(args.replay), 'exit_code': exit_code,
        'timed_out': timed_out, 'elapsed_seconds': round(time.monotonic() - started, 2),
        'completion_observed': completion, 'populated_world': populated,
        'matching_samples': sum(sample['match'] for sample in samples),
        'required_samples': len(expected), 'passed': passed,
        'crc_mismatch': mismatch.group(0) if mismatch else None,
        'first_difference': first_difference, 'samples': samples,
    }
    (args.output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: value for key, value in report.items() if key not in ('samples', 'first_difference')}, indent=2))
    if first_difference:
        print(json.dumps(first_difference, indent=2))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
