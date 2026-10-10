#!/usr/bin/env python3
"""Verify game networking sources, installed SDK3 headers and the actual static archives."""
# GeneralsX @build Codex 10/10/2026 Keep source identities and compiled dependency evidence with private replay results.
import argparse
import hashlib
import json
import os
from pathlib import Path


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--expected', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--build', type=Path)
parser.add_argument('--triplet')
parser.add_argument('--vcpkg-root', type=Path)
args = parser.parse_args()
expected = json.loads(args.expected.read_text())
source = args.source.resolve()
checked = {}
for name, checksum in expected['source_sha256'].items():
    actual = sha(source / name)
    if actual != checksum:
        discrepancy = {'path': name, 'actual_sha256': actual, 'expected_sha256': checksum,
                       'lf_normalized_sha256': hashlib.sha256((source / name).read_bytes().replace(b'\r\n', b'\n')).hexdigest()}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps({'source_identity_failed': discrepancy}, indent=2) + '\n')
    assert actual == checksum, 'Source identity mismatch: ' + name
    checked[name] = actual
port = json.loads((source / 'cmake/vcpkg-overlay-ports/gamenetworkingsockets/vcpkg.json').read_text())
assert port['version'] == expected['version'] and port['port-version'] == expected['port_version']
report = {'source_identities_verified': True, 'source_sha256': checked,
          'version': port['version'], 'port_version': port['port-version'],
          'installed_archives_and_headers_verified': False}
if args.build:
    assert args.triplet, 'Specify the actual target triplet'
    build = args.build.resolve()
    installed = build / 'vcpkg_installed'
    status = (installed / 'vcpkg/status').read_text()
    packages = [p for p in status.split('\n\n') if 'Package: gamenetworkingsockets\n' in p and 'Feature:' not in p]
    assert len(packages) == 1 and all('Version: 1.6.0\n' in p and 'Port-Version: 3\n' in p and 'Architecture: ' + args.triplet + '\n' in p for p in packages)
    target = installed / args.triplet
    headers = {}
    for name, checksum in expected['sdk_public_header_sha256'].items():
        candidates = list((target / 'include').rglob(Path(name).name))
        assert len(candidates) == 1, 'Expected one installed SDK header: ' + name
        actual = sha(candidates[0])
        assert actual == checksum, 'Installed SDK header mismatch: ' + name
        headers[str(candidates[0].relative_to(target))] = actual
    archives = {}
    for directory in ['lib', 'debug/lib']:
        matches = [p for p in (target / directory).glob('*GameNetworkingSockets_s.*') if p.suffix in ('.a', '.lib')]
        assert len(matches) == 1, 'Expected one actual static SDK archive in ' + directory
        archives[str(matches[0].relative_to(build))] = sha(matches[0])
    sdk_environment = os.environ.get('VCPKG_INSTALLATION_ROOT') if os.name == 'nt' else os.environ.get('VCPKG_ROOT')
    sdk_root = args.vcpkg_root or Path(sdk_environment or os.environ['VCPKG_ROOT'])
    sources = sdk_root / 'buildtrees/gamenetworkingsockets/src'
    matches = []
    for candidate in sources.iterdir():
        if candidate.is_dir() and all((candidate / name).is_file() and sha(candidate / name) == checksum for name, checksum in expected['sdk_source_sha256'].items()):
            matches.append(candidate)
    assert matches, 'The actual patched SDK source does not match the tested SDK3 CPP/header identities'
    link_proofs = []
    for path in list(build.rglob('*.ninja')) + list((build / 'GeneralsMD').rglob('*.vcxproj')):
        lines = [line for line in path.read_text(errors='replace').splitlines() if 'GameNetworkingSockets_s.' in line]
        if lines:
            link_proofs.append({'file': str(path.relative_to(build)), 'sha256': sha(path), 'matching_lines': len(lines)})
    assert link_proofs, 'No generated game build/link reference to the audited static SDK archive'
    report.update(installed_archives_and_headers_verified=True, actual_triplet=args.triplet,
                  archive_sha256=archives, installed_header_sha256=headers,
                  patched_sdk_source_sha256=expected['sdk_source_sha256'],
                  matching_actual_sdk_source_roots=[str(p) for p in matches],
                  generated_link_references=link_proofs)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({key: value for key, value in report.items() if not isinstance(value, (dict, list))}))
