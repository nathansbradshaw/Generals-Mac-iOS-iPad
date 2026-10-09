#!/usr/bin/env python3
"""Export a Linux client with matching ELF libraries and verify linked dependencies."""
# GeneralsX @build Codex 09/10/2026 Preserve build-root libraries and reject mixed-architecture exports.
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess


def architecture(path):
    with path.open('rb') as stream:
        header = stream.read(20)
    if len(header) < 20 or header[:4] != b'\x7fELF' or header[5] != 1:
        return None
    return header[4], struct.unpack_from('<H', header, 18)[0]


def checksum(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--executable', type=Path)
parser.add_argument('--ca-bundle', type=Path, default=Path('/etc/ssl/certs/ca-certificates.crt'))
args = parser.parse_args()
build = args.build.resolve()
output = args.output.resolve()
executable = (args.executable or build/'GeneralsMD/GeneralsXZH').resolve()
target = architecture(executable)
assert target is not None, 'Client is not a supported ELF executable'
output.mkdir(parents=True, exist_ok=False)
shutil.copy2(executable, output/'GeneralsXZH')
assert b'-----BEGIN CERTIFICATE-----' in args.ca_bundle.read_bytes(), 'Missing certificate trust bundle'
subprocess.run(['openssl', 'x509', '-in', str(args.ca_bundle), '-noout'], check=True)
shutil.copy2(args.ca_bundle, output/'cacert.pem')
libraries = {}
skipped = 0
for source in sorted(set(build.glob('*.so*')) | set((build/'_deps').rglob('*.so*'))):
    if not source.is_file():
        continue
    if architecture(source) != target:
        skipped += 1
        continue
    digest = checksum(source)
    if source.name in libraries:
        assert libraries[source.name] == digest, f'Conflicting library: {source.name}'
        continue
    libraries[source.name] = digest
    shutil.copy2(source, output/source.name)
for required in ('libgamespy.so', 'libSDL3.so.0', 'libSDL3_image.so.0', 'libopenal.so.1', 'libdxvk_d3d8.so', 'libdxvk_d3d9.so'):
    assert required in libraries, f'Missing runtime library: {required}'
environment = os.environ.copy()
environment['LD_LIBRARY_PATH'] = str(output)
linked = subprocess.run(['ldd', str(output/'GeneralsXZH')], env=environment,
                        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
assert 'not found' not in linked.stdout, linked.stdout
for path in re.findall(r'=>\s+(/\S+)', linked.stdout):
    assert not Path(path).resolve().is_relative_to(build), f'Export still loads build-tree dependency: {path}'
report = {'executable_sha256': checksum(executable), 'elf_class': target[0],
          'elf_machine': target[1], 'libraries': libraries,
          'ca_bundle_sha256': checksum(output/'cacert.pem'),
          'skipped_other_architecture': skipped, 'linked_dependency_check_passed': True}
(output/'package-manifest.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps({key: value for key, value in report.items() if key != 'libraries'}, indent=2))
