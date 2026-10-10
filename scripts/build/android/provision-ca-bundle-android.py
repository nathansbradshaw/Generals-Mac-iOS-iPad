#!/usr/bin/env python3
"""Stage a host-maintained public CA bundle into a stopped Android developer app."""
# GeneralsX @build Codex 09/10/2026 HTTPS/WebSocket clients need a trust bundle.
import hashlib
import io
import os
from pathlib import Path
import subprocess
import tarfile
import time

package = "com.nathanbradshaw.generalsxzh"
sdk = Path(os.environ.get("ANDROID_HOME", str(Path.home() / "Library/Android/sdk")))
adb = [str(sdk / "platform-tools/adb")]
source = os.environ.get("GX_CA_BUNDLE")
if source:
    bundle = Path(source)
else:
    bundle = next((p for p in (Path("/etc/ssl/cert.pem"),
        Path("/etc/ssl/certs/ca-certificates.crt")) if p.is_file()), None)
if not bundle:
    raise SystemExit("Set GX_CA_BUNDLE to a maintained PEM certificate bundle")
raw = bundle.read_bytes()
if b"-----BEGIN CERTIFICATE-----" not in raw:
    raise SystemExit("GX_CA_BUNDLE must contain PEM certificates")

def stopped():
    result = subprocess.run(adb + ["shell", "pidof", package],
        capture_output=True, timeout=10)
    if result.stdout.strip():
        raise SystemExit("Stop GeneralsXZH before provisioning its CA bundle")

stopped()
subprocess.run(adb + ["shell", "run-as", package, "mkdir", "-p", "files/GameData"],
    check=True, timeout=10)
pending = "cacert.pem.pending"
stream = io.BytesIO()
with tarfile.open(fileobj=stream, mode="w") as archive:
    info = tarfile.TarInfo(pending)
    info.size, info.mode = len(raw), 0o600
    archive.addfile(info, io.BytesIO(raw))
subprocess.run(adb + ["exec-in", "run-as", package, "tar", "xf", "-", "-C", "files/GameData"],
    input=stream.getvalue(), check=True, timeout=30)
for attempt in range(30):
    result = subprocess.run(adb + ["exec-out", "run-as", package, "cat", "files/GameData/" + pending],
        capture_output=True, timeout=10)
    if result.returncode == 0 and result.stdout == raw:
        break
    time.sleep(0.2)
else:
    raise SystemExit("CA transfer did not verify; active bundle remains unchanged")
stopped()
subprocess.run(adb + ["shell", "run-as", package, "mv", "files/GameData/" + pending,
    "files/GameData/cacert.pem"], check=True, timeout=10)
result = subprocess.check_output(adb + ["exec-out", "run-as", package, "cat",
    "files/GameData/cacert.pem"], timeout=10)
if result != raw:
    raise SystemExit("Active CA bundle did not verify")
print("Android CA bundle verified: " + hashlib.sha256(raw).hexdigest())
