#!/usr/bin/env python3
"""Verify that the actual Windows PE contains the opt-in ordinary command helper."""
# GeneralsX @build Codex 10/10/2026 Compilation identity does not prove runtime commands or live multiplayer.
import argparse
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
for name in ('source', 'build', 'executable', 'output'):
    p.add_argument('--' + name, type=Path, required=True)
a = p.parse_args()
relative = 'GeneralsMD/Code/GameEngineDevice/Source/Win32Device/Common/Win32GameEngine.cpp'
cpp = (a.source / relative).read_bytes()
start = cpp.index(b'static void updateOnlineCommandSmokeTest()')
end = cpp.index(b'void Win32GameEngine::update()', start)
body = cpp[start:end]
update = cpp[end:]
assert b'GameEngine::update();\n\tupdateOnlineCommandSmokeTest();' in update
assert b'GENERALSX_SMOKE_COMMANDS' in body
for command in (b'MSG_QUEUE_UNIT_CREATE', b'MSG_DO_MOVETO', b'MSG_SELF_DESTRUCT'):
    assert command in body
data = a.executable.read_bytes()
assert data.startswith(b'MZ')
markers = (b'COMMAND_SMOKE queued', b'COMMAND_SMOKE move',
           b'COMMAND_SMOKE movement', b'COMMAND_SMOKE surrender')
for marker in markers:
    assert marker in data, marker
refs = []
for ninja in a.build.rglob('*.ninja'):
    lines = [line for line in ninja.read_text(errors='replace').splitlines()
             if 'Win32GameEngine.cpp' in line and ('build ' in line or '/Source/' in line)]
    if lines:
        refs.append({'file': str(ninja.relative_to(a.build)), 'matching_lines': len(lines)})
assert refs
result = {'source_file': relative, 'source_sha256': hashlib.sha256(cpp).hexdigest(),
          'helper_body_sha256': hashlib.sha256(body).hexdigest(),
          'binary_sha256': hashlib.sha256(data).hexdigest(),
          'compiled_helper_markers_verified': True, 'generated_compile_references': refs,
          'ordinary_message_commands_present': True,
          'actual_live_runtime_commands_verified': False}
a.output.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
