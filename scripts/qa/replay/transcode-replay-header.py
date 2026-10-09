#!/usr/bin/env python3
"""Transcode only three replay-header strings; preserve every other byte."""
# GeneralsX @build Codex 09/10/2026 Separate native replay format compatibility from simulation evidence.
import argparse
import hashlib
import json
from pathlib import Path


def transcode(data):
    if data[:6] != b'GENREP':
        raise ValueError('Expected a GENREP recording')
    # Two int32 timestamps, uint32 frame count, ten bool flags (2 + 8 slots).
    prefix_end = 28
    position = prefix_end
    output = bytearray(data[:prefix_end])
    lengths = []
    for field in range(3):
        start = position
        while True:
            unit = data[position:position + 4]
            if len(unit) != 4:
                raise ValueError('Truncated UTF-32 header')
            position += 4
            if unit == b'\0' * 4:
                break
            if position - start > 4096:
                raise ValueError('Header string exceeds engine buffer')
        text = data[start:position - 4].decode('utf-32-le')
        encoded = text.encode('utf-16-le')
        output.extend(encoded + b'\0\0')
        lengths.append(len(text))
        if field == 0:
            if len(data[position:position + 16]) != 16:
                raise ValueError('Truncated SYSTEMTIME')
            output.extend(data[position:position + 16])
            position += 16
    tail = data[position:]
    if len(tail) < 12 or b'M=' not in tail[12:].split(b'\0', 1)[0]:
        raise ValueError('Header does not lead to expected game options')
    output.extend(tail)
    assert bytes(output[-len(tail):]) == tail
    return bytes(output), {
        'converted_header_strings': 3,
        'character_counts': lengths,
        'unchanged_prefix_bytes': prefix_end,
        'unchanged_tail_bytes': len(tail),
        'unchanged_tail_sha256': hashlib.sha256(tail).hexdigest(),
        'source_sha256': hashlib.sha256(data).hexdigest(),
        'derived_sha256': hashlib.sha256(output).hexdigest(),
        'source_bytes': len(data), 'derived_bytes': len(output),
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Output exists; preserve previous evidence')
    converted, report = transcode(args.source.read_bytes())
    args.output.write_bytes(converted)
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
