#!/usr/bin/env python3
"""Observe an allocated native Windows test; export only whitelisted metadata."""
# GeneralsX @build Codex 10/10/2026 Keep fresh replay, traces, raw logs and credentials private on the owned VM.
import argparse
import base64
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
import zlib


def sha(data):
    return hashlib.sha256(data).hexdigest()


def alive(pid):
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes = (wintypes.HANDLE, wintypes.DWORD)
    kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
    handle = kernel.OpenProcess(0x100000, False, pid)
    if not handle:
        return False
    try:
        return kernel.WaitForSingleObject(handle, 0) == 258
    finally:
        kernel.CloseHandle(handle)


def recording(path):
    data = path.read_bytes()
    if len(data) < 28 or data[:6] != b'GENREP':
        return None
    start, end, frames = struct.unpack_from('<iiI', data, 6)
    flags = list(data[18:28])
    return {'sha256': sha(data), 'bytes': len(data), 'start_time': start, 'end_time': end,
            'frame_count': frames, 'desync': bool(flags[0]), 'quit_early': bool(flags[1]),
            'player_disconnects': [bool(x) for x in flags[2:]],
            'clean_header': end > start > 0 and frames > 0 and all(x == 0 for x in flags)}


def snapshot(root, process):
    log = (root / 'native.log').read_text(errors='replace')
    samples = []
    for path in sorted(root.glob('state-??????.trace')):
        data = path.read_bytes()
        samples.append({'frame': int(path.stem.rsplit('-', 1)[1]), 'sha256': sha(data),
                        'bytes': len(data), 'labels': sum(x.startswith(b'LABEL ') for x in data.splitlines())})
    peer_handles = {}
    connected = set()
    for line in log.splitlines():
        match = re.search(r'#(\d+) P2P[^\]]*?str:(34631|34632|34634)\b', line)
        if match:
            handle, user = int(match[1]), int(match[2])
            peer_handles[user] = handle
            if re.search(r'\] connected\b', line):
                connected.add(user)
    selected = {}
    for line in log.splitlines():
        if 'ICE selected candidate ' in line:
            route = line.split('ICE selected candidate ', 1)[1].strip()
            hops = route.count(' -> ')
            if hops in (1, 2):
                selected[sha(route.encode())] = {'evidence_sha256': sha(route.encode()),
                                               'route_class': 'relay' if hops == 2 else 'direct',
                                               'hop_count': hops}
    replays = []
    for path in (root / 'profile').rglob('*.rep'):
        try:
            header = recording(path)
            if header:
                replays.append(header)
        except (OSError, ValueError, struct.error):
            pass  # A file may still be exclusively open; no completion inference.
    return {'native_windows_os': True, 'account': 34633, 'case': process['case'],
            'binary_sha256': process['binary_sha256'], 'binary_crc32': process['binary_crc32'],
            'headless': True, 'rendered_game': False, 'game_process_alive': alive(process['pid']),
            'normal_process_exit_status': None, 'joined_lobby_observed': '[NGMP] Joined lobby' in log,
            'peer_handles': [{'account': user, 'handle': value, 'connected_observed': user in connected}
                             for user, value in sorted(peer_handles.items())],
            'directed_peer_handle_count': len(peer_handles),
            'selected_route_samples': list(selected.values()),
            'route_evidence_scope': 'Actual SDK selected candidate paths; aggregate, no assumed per-peer association',
            'state_samples': samples, 'recordings': replays,
            'command_smoke': {'production_queued': 'COMMAND_SMOKE queued' in log,
                              'movement_passed': 'COMMAND_SMOKE movement passed' in log,
                              'normal_surrender_sent': 'COMMAND_SMOKE surrender' in log},
            'crc_mismatch_observed': 'REPLAY_CRC_MISMATCH' in log,
            'disconnect_end_reasons': [int(x) for x in re.findall(r'DISCONNECTED OR PROBLEM DETECTED (\d+)', log)],
            'raw_replays_traces_logs_exported': False,
            'full_match_completion_proven': False}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('phase', choices=['start', 'lobby', 'match', 'release', 'local-replay', 'keepalive'])
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--plan', type=Path, required=True)
    p.add_argument('--client', type=Path)
    p.add_argument('--assets', type=Path)
    p.add_argument('--base-assets', type=Path)
    p.add_argument('--seconds', type=int, default=600)
    a = p.parse_args()
    if sys.platform != 'win32':
        p.error('The live worker must run on actual Windows')
    assert 1 <= a.seconds <= 1800
    root = a.root.resolve()
    plan = json.loads(a.plan.read_text())
    assert plan['authorized'] is True and plan['account'] == 34633
    assert re.fullmatch(r'[A-Za-z0-9_-]+', plan['case'])
    assert plan['end_frame'] in (2100, 30000)
    if a.phase == 'start':
        config = json.loads(os.environ['GENERALSX_WINDOWS_LIVE_CONFIG'])
        assert config['account'] == 34633 and config['case'] == plan['case']
        assert config['binary_sha256'] == plan['binary_sha256']
        assert a.client and a.assets and a.base_assets
        binary = (a.client / 'generalszh.exe').resolve()
        data = binary.read_bytes()
        assert sha(data) == plan['binary_sha256']
        assert zlib.crc32(data) & 0xffffffff == plan['binary_crc32']
        assert data[:2] == b'MZ'
        root.mkdir(parents=True, exist_ok=False)
        profile = root / 'profile'
        settings = profile / 'GeneralsOnlineData'
        settings.mkdir(parents=True)
        (settings / 'settings.json').write_text(json.dumps({'debug': {'verbose_logging': True}}))
        for name in ('generalszh.exe', 'mss32.dll', 'binkw32.dll', 'cacert.pem'):
            (a.assets / name).write_bytes((a.client / name).read_bytes())
        env = {k: v for k, v in os.environ.items() if not k.startswith('GENERALSX_ONLINE_')}
        for key in ('GENERALSX_ONLINE_URL', 'GENERALSX_ONLINE_REFRESH_TOKEN',
                    'GENERALSX_ONLINE_STUN_SERVERS', 'GENERALSX_ONLINE_TURN_SERVERS'):
            value = config[key]
            assert isinstance(value, str) and '\n' not in value and '\r' not in value
            env[key] = value
        env.pop('GENERALSX_WINDOWS_LIVE_CONFIG', None)
        env.update(GENERALSX_HEADLESS_ONLINE_GUI='1', GENERALSX_AUTOREADY='1',
                   GENERALSX_SMOKE_COMMANDS=plan['command_mode'],
                   GENERALSX_CRC_TRACE=str(root / 'state'),
                   GENERALSX_CRC_TRACE_END_FRAME=str(plan['end_frame']),
                   GENERALSX_USER_DATA_DIR=str(profile),
                   CNC_GENERALS_ZH_PATH=str(a.assets.resolve()),
                   CNC_GENERALS_PATH=str(a.base_assets.resolve()))
        log = (root / 'native.log').open('wb')
        child = subprocess.Popen([str((a.assets / 'generalszh.exe').resolve()), '-headless',
                                  '-quickstart', '-nologo', '-noshellmap', '-joinAutostart'],
                                 cwd=a.assets.resolve(), env=env, stdout=log, stderr=subprocess.STDOUT)
        log.close()
        state = {'pid': child.pid, 'case': plan['case'], 'binary_sha256': plan['binary_sha256'],
                 'binary_crc32': plan['binary_crc32'], 'assets': str(a.assets.resolve()),
                 'base_assets': str(a.base_assets.resolve())}
        (root / 'process.json').write_text(json.dumps(state, indent=2))
        print(json.dumps({'native_windows_os': True, 'account': 34633, 'case': plan['case'], 'owned_process_started': True}))
        return 0
    process = json.loads((root / 'process.json').read_text())
    public = Path('safe-live-metadata')
    public.mkdir(exist_ok=True)
    if a.phase == 'release':
        deadline = time.monotonic() + a.seconds
        while time.monotonic() < deadline:
            content = subprocess.check_output(['gh', 'api',
                'repos/nathansbradshaw/Generals-Mac-iOS-iPad/contents/scripts/qa/replay/windows-live-plan.json?ref=codex/native-replay-ci-20261008',
                '--jq', '.content'], text=True)
            control = json.loads(base64.b64decode(content))
            if control.get('case') == plan['case'] and control.get('binary_sha256') == plan['binary_sha256'] \
                    and control.get('release_for_local_replay') is True:
                return 0
            time.sleep(10)
        print('Root release for local replay was not received; no peer cleanup or completion inferred.')
        return 1
    if a.phase == 'keepalive':
        deadline = time.monotonic() + a.seconds
        while time.monotonic() < deadline and alive(process['pid']):
            time.sleep(2)
        return 0  # Keepalive expiry is never completion evidence.
    if a.phase == 'local-replay':
        candidates = []
        for f in (root / 'profile').rglob('*.rep'):
            header = recording(f)
            if header and header['clean_header']:
                candidates.append((f, header))
        assert len(candidates) == 1, 'Require one fresh clean recording'
        replay, header = candidates[0]
        out = root / 'local-replay'
        out.mkdir()
        env = {k: v for k, v in os.environ.items()
               if not k.startswith(('GENERALSX_ONLINE_', 'GENERALSX_SMOKE_'))}
        env.pop('GENERALSX_WINDOWS_LIVE_CONFIG', None)
        env.pop('GENERALSX_HEADLESS_ONLINE_GUI', None)
        env.update(GENERALSX_USER_DATA_DIR=str(out / 'profile'),
                   GENERALSX_CRC_TRACE=str(out / 'state'),
                   GENERALSX_CRC_TRACE_END_FRAME=str(plan['end_frame']),
                   CNC_GENERALS_ZH_PATH=process['assets'], CNC_GENERALS_PATH=process['base_assets'])
        with (out / 'native.log').open('wb') as log:
            replay_process = subprocess.run([str(Path(process['assets']) / 'generalszh.exe'),
                                             '-headless', '-replay', str(replay)],
                                            cwd=process['assets'], env=env, stdout=log,
                                            stderr=subprocess.STDOUT, timeout=120)
        text = (out / 'native.log').read_text(errors='replace')
        samples = []
        for frame in range(0, plan['end_frame'] + 1, 100):
            live = root / f'state-{frame:06}.trace'
            played = out / live.name
            samples.append({'frame': frame, 'matching_live_state': live.is_file() and played.is_file()
                            and live.read_bytes() == played.read_bytes()})
        duration = header['frame_count'] // 30
        expected = f'{duration // 60:02}:{duration % 60:02}'
        passed = replay_process.returncode == 0 and 'REPLAY_CRC_MISMATCH' not in text \
                 and re.search(r'Game Time:\s*' + re.escape(expected) + '/' + re.escape(expected), text) \
                 and all(x['matching_live_state'] for x in samples)
        report = {'native_windows_os': True, 'account': 34633, 'case': plan['case'],
                  'binary_sha256': process['binary_sha256'], 'replay_sha256': header['sha256'],
                  'exit_code': replay_process.returncode, 'required_samples': len(samples),
                  'matching_samples': sum(x['matching_live_state'] for x in samples),
                  'passed_fresh_local_replay': bool(passed), 'raw_recording_exported': False}
        (public / 'fresh-local-replay.json').write_text(json.dumps(report, indent=2))
        print(json.dumps(report))
        return 0 if passed else 1
    deadline = time.monotonic() + a.seconds
    result = None
    ready = False
    while time.monotonic() < deadline:
        result = snapshot(root, process)
        if a.phase == 'lobby':
            ready = result['joined_lobby_observed']
        else:
            samples = {x['frame']: x for x in result['state_samples']}
            ready = all(frame in samples for frame in range(0, plan['end_frame'] + 1, 100)) \
                    and samples.get(100, {}).get('labels', 0) >= 200 \
                    and any(x['clean_header'] for x in result['recordings'])
        if ready or not result['game_process_alive']:
            break
        time.sleep(2)
    result['observer_phase'] = a.phase
    result['phase_ready'] = bool(ready)
    result['scope'] = 'Allocated native Windows metadata; root must compare all four clients and classify routes/endings'
    (public / (a.phase + '.json')).write_text(json.dumps(result, indent=2))
    print(json.dumps({'case': plan['case'], 'phase': a.phase, 'phase_ready': bool(ready),
                      'state_samples': len(result['state_samples']), 'raw_gameplay_exported': False}))
    return 0 if ready else 1


if __name__ == '__main__':
    raise SystemExit(main())
