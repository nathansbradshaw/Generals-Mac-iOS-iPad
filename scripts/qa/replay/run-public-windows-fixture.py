#!/usr/bin/env python3
"""Require native Windows completion and exact populated states for an approved fixture."""
# GeneralsX @build Codex 10/10/2026 This checks published source; it does not certify corrected live clients.
import argparse, hashlib, json, os, re, subprocess, sys, time
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
for key in ('executable', 'assets', 'base-assets', 'fixture', 'reference', 'output'):
    p.add_argument('--' + key, type=Path, required=True)
a = p.parse_args()
assert sys.platform == 'win32', 'Run on actual Windows'
a.output = a.output.resolve()
a.output.mkdir(parents=True, exist_ok=False)
refs = sorted(a.reference.glob('state-??????.trace'))
assert len(refs) == 22 and [int(f.stem.rsplit('-', 1)[1]) for f in refs] == list(range(0, 2101, 100))
assert sum(x.startswith(b'LABEL ') for x in refs[1].read_bytes().splitlines()) >= 200
sha = lambda data: hashlib.sha256(data).hexdigest()
env = {k: v for k, v in os.environ.items()
       if not k.startswith(('GENERALSX_ONLINE_', 'GENERALSX_SMOKE_'))}
env.pop('GENERALSX_HEADLESS_ONLINE_GUI', None)
env.update(CNC_GENERALS_ZH_PATH=str(a.assets.resolve()),
           CNC_GENERALS_PATH=str(a.base_assets.resolve()),
           GENERALSX_USER_DATA_DIR=str(a.output / 'profile'),
           GENERALSX_CRC_TRACE=str(a.output / 'state'), GENERALSX_CRC_TRACE_END_FRAME='2100')
started = time.monotonic()
timeout = False
with (a.output / 'native.log').open('wb') as log:
    child = subprocess.Popen([str(a.executable.resolve()), '-headless', '-replay', str(a.fixture.resolve())],
                             cwd=a.assets.resolve(), env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        code = child.wait(timeout=600)
    except subprocess.TimeoutExpired:
        timeout = True
        child.terminate()
        try:
            code = child.wait(timeout=10)
        except subprocess.TimeoutExpired:
            child.kill()
            code = child.wait(timeout=10)
text = (a.output / 'native.log').read_text(errors='replace')
samples = []
for ref in refs:
    actual = a.output / ref.name
    data = actual.read_bytes() if actual.is_file() else b''
    samples.append({'frame': int(ref.stem.rsplit('-', 1)[1]), 'present': actual.is_file(),
                    'match': actual.is_file() and sha(data) == sha(ref.read_bytes()),
                    'sha256': sha(data) if data else None, 'bytes': len(data),
                    'labels': sum(x.startswith(b'LABEL ') for x in data.splitlines())})
completion = bool(re.search(r'Game Time:\s*01:20/01:20', text))
mismatch = 'REPLAY_CRC_MISMATCH' in text
passed = code == 0 and not timeout and completion and not mismatch \
         and samples[1]['labels'] >= 200 and all(x['match'] for x in samples)
report = {'public_source_commit': 'f16bbbf614ca798401d9bd086a25c8d4178d4859',
          'native_windows_os': True, 'binary_sha256': sha(a.executable.read_bytes()),
          'fixture_sha256': sha(a.fixture.read_bytes()), 'exit_code': code, 'timed_out': timeout,
          'elapsed_seconds': round(time.monotonic() - started, 2),
          'completion_observed': completion, 'replay_crc_mismatch_observed': mismatch,
          'populated_frame100_labels': samples[1]['labels'],
          'matching_samples': sum(x['match'] for x in samples), 'required_samples': 22,
          'private_engine_source_overlay_used': False, 'real_account_login_invoked': False,
          'rendered_game': False, 'live_interoperability_proven': False, 'passed': passed,
          'samples': samples}
(a.output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k: v for k, v in report.items() if k != 'samples'}))
raise SystemExit(0 if passed else 1)
