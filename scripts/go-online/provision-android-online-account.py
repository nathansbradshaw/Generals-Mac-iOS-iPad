#!/usr/bin/env python3
"""Provision a debug Android client's existing self-hosted account without launching UI.

Usage: provision-android-online-account.py --user-id 34622 --name friend1
       [--device SERIAL] [--url https://10.0.2.2:9000/env/prod/contract/1]
       [--package com.nathanbradshaw.generalsxzh] [--env-file PATH]

The backend account must already exist. Credentials stay in app-private storage;
existing settings are merged and both replaced files receive private backups.
Stop the app before provisioning; this helper refuses to write while it is running.
GeneralsX @build Codex 08/10/2026 Provision emulator/device NGMP credentials without a launcher or shell-visible token.
"""

import argparse
import datetime
import io
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tarfile
import time
from urllib.parse import urlsplit
import uuid

from mint_refresh_token import mint


def load_key(env_file):
    key = os.environ.get("GENERALSX_ONLINE_JWT_KEY")
    if key:
        return key
    for line in env_file.read_text().splitlines():
        line = line.strip().removeprefix("export ")
        if not line or line.startswith("#") or "=" not in line:
            continue
        name, value = line.split("=", 1)
        if name.strip() == "GENERALSX_ONLINE_JWT_KEY":
            tokens = shlex.split(value, comments=True)
            if len(tokens) == 1 and tokens[0]:
                return tokens[0]
    raise ValueError("Set GENERALSX_ONLINE_JWT_KEY or provide it in the gitignored .env")


def main():
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--user-id", required=True, type=int)
    parser.add_argument("--name", required=True)
    parser.add_argument("--device")
    parser.add_argument("--package", default="com.nathanbradshaw.generalsxzh")
    parser.add_argument("--url", default="https://10.0.2.2:9000/env/prod/contract/1")
    parser.add_argument("--env-file", type=Path, default=repo / ".env")
    args = parser.parse_args()
    if args.user_id <= 0 or not args.name.strip():
        parser.error("An existing positive user ID and nonempty display name are required")
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+", args.package):
        parser.error("Invalid Android package identifier")
    url = urlsplit(args.url.strip())
    if url.scheme not in ("http", "https") or not url.hostname or url.username or url.password or url.query or url.fragment:
        parser.error("--url must be an HTTP(S) service URL without credentials, query or fragment")
    service_url = args.url.strip().rstrip("/")
    adb = Path(os.environ.get("ANDROID_HOME", str(Path.home() / "Library/Android/sdk"))) / "platform-tools/adb"
    command = [str(adb)] + (["-s", args.device] if args.device else [])

    def call(*arguments, data=None, check=True):
        return subprocess.run(command + list(arguments), input=data, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, check=check)

    call("get-state")
    call("shell", "run-as", args.package, "true")
    running = call("shell", "pidof", args.package, check=False)
    if running.returncode == 0 and running.stdout.strip():
        raise ValueError("Stop the Android app before provisioning")
    directory = "files/GameData/GeneralsX/GeneralsZH/GeneralsOnlineData"
    call("shell", "run-as", args.package, "mkdir", "-p", directory)
    settings = {}
    settings_path = directory + "/settings.json"
    if call("shell", "run-as", args.package, "test", "-e", settings_path, check=False).returncode == 0:
        existing = call("exec-out", "run-as", args.package, "cat", settings_path)
        settings = json.loads(existing.stdout)
    if not isinstance(settings, dict) or not isinstance(settings.get("network", {}), dict):
        raise ValueError("Existing settings are not a JSON object; no settings were replaced")
    settings.setdefault("network", {})["service_url"] = service_url
    refresh_token = mint(args.user_id, args.name, load_key(args.env_file), "go-lan", "go-lan-clients", "127.0.0.1", 43200)
    payloads = {"credentials.json": {"refresh_token": refresh_token}, "settings.json": settings}
    suffix = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
    for name in payloads:
        path = directory + "/" + name
        if call("shell", "run-as", args.package, "test", "-f", path, check=False).returncode == 0:
            call("shell", "run-as", args.package, "cp", path, path + ".before-" + suffix)
            call("shell", "run-as", args.package, "chmod", "600", path + ".before-" + suffix)
    # Stream private files directly to run-as: no token in argv, /sdcard or host temp files.
    stream = io.BytesIO()
    expected = {}
    with tarfile.open(fileobj=stream, mode="w") as archive:
        for name, payload in payloads.items():
            content = (json.dumps(payload, indent=1) + "\n").encode()
            expected[name] = content
            info = tarfile.TarInfo(name + ".pending-" + suffix)
            info.size = len(content)
            info.mode = 0o600
            archive.addfile(info, io.BytesIO(content))
    call("exec-in", "run-as", args.package, "tar", "-xf", "-", "-C", directory, data=stream.getvalue())
    # GeneralsX @bugfix Codex 08/10/2026 ADB exec-in can return before tar
    # finishes; verify private bytes before replacing either active file.
    deadline = time.monotonic() + 5.0
    for name, content in expected.items():
        pending = directory + "/" + name + ".pending-" + suffix
        while True:
            copied = call("exec-out", "run-as", args.package, "cat", pending, check=False)
            if copied.returncode == 0 and copied.stdout == content:
                break
            if time.monotonic() >= deadline:
                raise ValueError("Private transfer did not finish; active files were not replaced")
            time.sleep(0.1)
    for name in payloads:
        call("shell", "run-as", args.package, "mv", directory + "/" + name + ".pending-" + suffix, directory + "/" + name)
    print(f"Provisioned {args.name} ({args.user_id}) for {service_url}; app was not launched.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        # Do not echo subprocess output or credentials on failure.
        detail = str(error) if isinstance(error, ValueError) and not isinstance(error, json.JSONDecodeError) else type(error).__name__
        raise SystemExit(f"Provisioning failed: {detail}; verify adb, the debug package, account and .env") from None
