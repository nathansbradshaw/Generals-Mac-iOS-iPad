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
    parser.add_argument('--dismiss-replay-score', action='store_true', help='Dismiss the completed score screen only in an isolated Linux virtual display')
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
    score_screen_observed = False
    score_screen_dismissed = False
    score_screen_window = None
    score_screen_input_error = None
    if args.dismiss_replay_score and (not sys.platform.startswith('linux') or not env.get('DISPLAY')):
        parser.error('Score-screen dismissal requires the isolated Linux virtual display')
    with (args.output / 'replay.log').open('wb') as log:
        process = subprocess.Popen(
            [str(args.executable), '-win', '-quickstart', '-nologo', '-noshellmap', '-replay', str(args.replay)],
            cwd=args.assets, env=env, stdout=log, stderr=subprocess.STDOUT,
        )
        score_ready_at = None
        while process.poll() is None and time.monotonic() - started < args.timeout:
            if args.dismiss_replay_score and not score_screen_dismissed and score_screen_input_error is None:
                current_log = (args.output / 'replay.log').read_text(errors='replace')
                marker = "Shell::doPush() called with layoutFile='Menus/ScoreScreen.wnd'"
                score_log = current_log[current_log.rfind(marker):] if marker in current_log else ''
                score_screen_observed = bool(re.search(r'winCreateLayout returned: 0x[1-9a-fA-F][0-9a-fA-F]*', score_log) and 'Shell::doPush() completed successfully' in score_log)
                exact = score_screen_observed and all((args.output / ref.name).is_file() and digest(args.output / ref.name) == digest(ref) for ref in expected)
                if exact and 'REPLAY_CRC_MISMATCH' not in current_log:
                    if score_ready_at is None:
                        score_ready_at = time.monotonic()
                    elif time.monotonic() - score_ready_at >= 2:
                        # GeneralsX @test Codex 09/10/2026 ReplaySimulation waits for ScoreScreenInput's Escape/OK before returning.
                        # Target only this test process's window after the score layout and all exact states exist.
                        try:
                            windows = subprocess.run(['xdotool', 'search', '--pid', str(process.pid)], env=env, capture_output=True, text=True, check=False, timeout=5)
                            ids = windows.stdout.split()
                            if windows.returncode == 0 and len(ids) == 1 and ids[0].isdigit():
                                score_screen_window = ids[0]
                                subprocess.run(['xdotool', 'windowfocus', '--sync', score_screen_window], env=env, check=True, timeout=5)
                                subprocess.run(['xdotool', 'key', '--window', score_screen_window, 'Escape'], env=env, check=True, timeout=5)
                                score_screen_dismissed = True
                        except (OSError, subprocess.SubprocessError) as error:
                            score_screen_input_error = type(error).__name__
            time.sleep(0.5)
        if process.poll() is None:
            timed_out = True
            process.kill()
        process.wait()
        exit_code = None if timed_out else process.returncode
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
    # GeneralsX @test Codex 09/10/2026 The rendered ReplaySimulation branch emits no headless progress line.
    # The rendered game waits on the completed score screen; the optional fixture dismisses only that screen.
    completion = exit_code == 0 and not timed_out
    passed = exit_code == 0 and completion and populated and mismatch is None and all(sample['match'] for sample in samples)
    if args.dismiss_replay_score:
        passed = passed and score_screen_observed and score_screen_dismissed
    report = {
        'platform': sys.platform, 'rendered_game': True, 'virtual_display_only': True, 'visual_or_audio_approval': False, 'executable_sha256': digest(args.executable),
        'replay_sha256': digest(args.replay), 'exit_code': exit_code,
        'timed_out': timed_out, 'elapsed_seconds': round(time.monotonic() - started, 2),
        'completion_observed': completion, 'completion_basis': 'normal rendered ReplaySimulation exit after completed score-screen dismissal' if args.dismiss_replay_score else 'normal rendered ReplaySimulation executor exit', 'populated_world': populated,
        'score_screen_observed': score_screen_observed, 'score_screen_dismissed': score_screen_dismissed, 'score_screen_window': score_screen_window, 'score_screen_input_error': score_screen_input_error,
        'matching_samples': sum(sample['match'] for sample in samples),
        'required_samples': len(expected), 'passed': passed,
        'crc_mismatch': mismatch.group(0) if mismatch else None,
        'first_difference': first_difference, 'samples': samples,
    }
    # GeneralsX @test Codex 09/10/2026 Preserve a private native crash backtrace without changing the failed gate.
    if sys.platform.startswith('linux') and exit_code is not None and exit_code < 0 and not timed_out:
        diagnostic_env = env.copy()
        diagnostic_env['GENERALSX_USER_DATA_DIR'] = str(args.output / 'diagnostic-profile')
        diagnostic_env['XDG_DATA_HOME'] = str(args.output / 'diagnostic-userdata')
        diagnostic_env['GENERALSX_CRC_TRACE'] = str(args.output / 'diagnostic-state')
        diagnostic_env['VK_LOADER_DEBUG'] = 'error,warn'
        # This native CI renderer targets x86_64 Linux; record the allocator ABI arguments before its crash.
        debugger_script = args.output / 'allocation-breakpoints.gdb'
        debugger_script.write_text('set pagination off\nset breakpoint pending on\nbreak _ZnwmSt11align_val_t\ncommands\nsilent\nprintf "GENERALSX_ALIGNED_NEW size=%lu alignment=%lu\\n", $rdi, $rsi\ncontinue\nend\nrun\nthread apply all bt 16\n')
        with (args.output / 'gdb-backtrace.log').open('wb') as diagnostic_log:
            try:
                diagnostic = subprocess.run(
                    ['gdb', '--batch', '--command', str(debugger_script), '--args',
                     str(args.executable), '-win', '-quickstart', '-nologo', '-noshellmap', '-replay', str(args.replay)],
                    cwd=args.assets, env=diagnostic_env, stdout=diagnostic_log, stderr=subprocess.STDOUT,
                    timeout=min(args.timeout, 120),
                )
                report['crash_diagnostic_returncode'] = diagnostic.returncode
            except (FileNotFoundError, subprocess.TimeoutExpired) as error:
                report['crash_diagnostic_error'] = type(error).__name__
    (args.output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: value for key, value in report.items() if key not in ('samples', 'first_difference')}, indent=2))
    if first_difference:
        print(json.dumps(first_difference, indent=2))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
