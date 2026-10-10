#!/usr/bin/env python3
"""Prove scripted menu callbacks against a fake loopback endpoint without QA login."""
# GeneralsX @build Codex 10/10/2026 Startup proof remains separate from real multiplayer and rendering.
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('executable', 'assets', 'base-assets', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=20)
    args = parser.parse_args()
    if not 10 <= args.seconds <= 30:
        parser.error('Startup probes must remain bounded to 10 through 30 seconds')
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    requests = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *unused):
            pass

        def do_POST(self):
            length = int(self.headers.get('Content-Length', '0'))
            if length > 65536:
                self.send_error(413)
                return
            self.rfile.read(length)  # Dummy-only input; never persist credentials or request bodies.
            requests.append(self.path)
            # Fail version acceptance intentionally so this probe cannot proceed to authentication.
            data = json.dumps({'result': 1, 'patcher_name': '', 'patcher_path': '', 'patcher_size': 0}).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    env = {k: v for k, v in os.environ.items() if not k.startswith('GENERALSX_ONLINE_')}
    env.update(GENERALSX_ONLINE_URL='http://127.0.0.1:' + str(server.server_port),
               GENERALSX_ONLINE_REFRESH_TOKEN='fake-startup-probe-no-account',
               GENERALSX_HEADLESS_ONLINE_GUI='1',
               GENERALSX_USER_DATA_DIR=str(args.output / 'profile'),
               CNC_GENERALS_ZH_PATH=str(args.assets.resolve()),
               CNC_GENERALS_PATH=str(args.base_assets.resolve()),
               ALSOFT_DRIVERS='null', SDL_AUDIODRIVER='dummy')
    started = time.monotonic()
    stopped = False
    with (args.output / 'native.log').open('wb') as log:
        process = subprocess.Popen([str(args.executable.resolve()), '-headless', '-quickstart',
                                    '-nologo', '-noshellmap', '-onlineAutostart'],
                                   cwd=args.assets.resolve(), env=env,
                                   stdout=log, stderr=subprocess.STDOUT)
        try:
            exit_code = process.wait(timeout=args.seconds)
        except subprocess.TimeoutExpired:
            stopped = True
            process.terminate()
            try:
                exit_code = process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                exit_code = process.wait(timeout=10)
    server.shutdown()
    text = (args.output / 'native.log').read_text(errors='replace')
    autostart = '-onlineAutostart: entering online flow' in text
    version_request = any(path.rstrip('/').endswith('/VersionCheck') for path in requests)
    unexpected = any(not path.rstrip('/').endswith('/VersionCheck') for path in requests)
    scripted_menu = 'HEADLESS_ONLINE_GUI layout=Menus/MainMenu.wnd' in text
    # Release builds may compile out DEBUG_LOG's autostart text. Require the real
    # scripted layout plus actual loopback request; this command invokes no input.
    passed = stopped and scripted_menu and version_request and not unexpected
    result = {'platform': sys.platform, 'native_windows_os': sys.platform == 'win32',
              'binary_sha256': hashlib.sha256(args.executable.read_bytes()).hexdigest(),
              'headless': True, 'fake_loopback_endpoint_only': True,
              'real_qa_credentials_present': False, 'real_account_login_invoked': False,
              'version_acceptance_intentionally_failed': True,
              'main_menu_scripted_layout_observed': scripted_menu,
              'autostart_debug_log_observed': autostart,
              'loopback_version_request_observed': version_request,
              'unexpected_endpoint_request_observed': unexpected,
              'owned_process_intentionally_stopped': stopped, 'exit_code': exit_code,
              'elapsed_seconds': round(time.monotonic() - started, 2),
              'host_windows_opened': False, 'rendered_game': False,
              'real_online_lobby_or_match_verified': False, 'passed_callback_probe': passed}
    (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
