#!/usr/bin/env python3
"""Remove only cached public GNS packages so the acceptance build compiles its audited source."""
# GeneralsX @build Codex 10/10/2026 Keep other public dependency caches while requiring fresh SDK3 compilation.
import os
from pathlib import Path
import zipfile

cache = Path(os.environ['LOCALAPPDATA']) / 'vcpkg/archives' if os.name == 'nt' else Path.home() / '.cache/vcpkg/archives'
removed = 0
if cache.exists():
    for path in cache.rglob('*.zip'):
        with zipfile.ZipFile(path) as archive:
            networking = any(name.startswith('share/gamenetworkingsockets/') for name in archive.namelist())
        if networking:
            path.unlink()
            removed += 1
print('Removed only cached public networking SDK packages:', removed)
