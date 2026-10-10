#!/usr/bin/env python3
"""Test compiled TLS policy helpers against local HTTPS and WSS fixtures.

Pass release/debug helper binaries, a working directory, and a maintained CA
bundle. No credentials or game windows are used. Keys stay in the work directory.
"""
# GeneralsX @bugfix Codex 09/10/2026 Verify failed certificates cannot enable later connections.
import argparse
import base64
import hashlib
import http.server
import json
from pathlib import Path
import shutil
import ssl
import subprocess
import threading


class Fixture(http.server.ThreadingHTTPServer):
    daemon_threads = True

    def get_request(self):
        connection, address = super().get_request()
        try:
            return self.context.wrap_socket(connection, server_side=True), address
        except ssl.SSLError:
            with self.lock:
                self.failed_tls += 1
            connection.close()
            raise

    def handle_error(self, request, client_address):
        # Rejected TLS handshakes and client disconnects are expected fixtures.
        with self.lock:
            self.handler_errors += 1


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *args):
        return

    def do_GET(self):
        with self.server.lock:
            self.server.http_requests += 1
        if self.headers.get('Upgrade', '').lower() == 'websocket':
            key = self.headers['Sec-WebSocket-Key']
            digest = hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()
            self.send_response(101)
            self.send_header('Upgrade', 'websocket')
            self.send_header('Connection', 'Upgrade')
            self.send_header('Sec-WebSocket-Accept', base64.b64encode(digest).decode())
        else:
            self.send_response(200)
            self.send_header('Content-Length', '0')
        self.end_headers()


def make_server(work, name, san):
    directory = work/name
    directory.mkdir(mode=0o700, exist_ok=True)
    certificate, key = directory/'certificate.pem', directory/'key.pem'
    ca, ca_key = directory/'ca.pem', directory/'ca-key.pem'
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-days', '1', '-subj', '/CN=GeneralsX fixture CA',
                    '-addext', 'basicConstraints=critical,CA:TRUE',
                    '-addext', 'keyUsage=critical,keyCertSign,cRLSign',
                    '-keyout', str(ca_key), '-out', str(ca)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    csr, extensions = directory/'server.csr', directory/'server-extensions.txt'
    extensions.write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\n'
                          'extendedKeyUsage=serverAuth\nsubjectAltName='+san+'\n')
    subprocess.run(['openssl', 'req', '-new', '-newkey', 'rsa:2048', '-nodes', '-subj', '/CN=fixture',
                    '-keyout', str(key), '-out', str(csr)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(['openssl', 'x509', '-req', '-in', str(csr), '-CA', str(ca), '-CAkey', str(ca_key),
                    '-CAcreateserial', '-days', '1', '-extfile', str(extensions), '-out', str(certificate)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    key.chmod(0o600)
    ca_key.chmod(0o600)
    server = Fixture(('127.0.0.1', 0), Handler)
    server.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    server.context.load_cert_chain(certificate, key)
    server.lock = threading.Lock()
    server.http_requests = server.failed_tls = server.handler_errors = 0
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server, ca


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', action='append', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--ca-bundle', type=Path, required=True)
    args = parser.parse_args()
    work = args.work.resolve()
    work.mkdir(mode=0o700, parents=True, exist_ok=True)
    servers = [make_server(work, 'ip-san', 'IP:127.0.0.1'),
               make_server(work, 'wrong-host', 'DNS:wrong.invalid')]
    results = []
    try:
        for binary in args.binary:
            binary = binary.resolve()
            for mode in ('http', 'ws'):
                for case in ('trusted', 'untrusted', 'missing_bundle', 'invalid_bundle', 'wrong_host'):
                    server, cert = servers[case == 'wrong_host']
                    profile = work/(binary.name+'-'+mode+'-'+case)
                    profile.mkdir(mode=0o700, exist_ok=True)
                    bundle = profile/'cacert.pem'
                    if bundle.exists():
                        bundle.unlink()
                    if case in ('trusted', 'wrong_host'):
                        shutil.copy2(cert, bundle)
                    elif case == 'untrusted':
                        shutil.copy2(args.ca_bundle, bundle)
                    elif case == 'invalid_bundle':
                        bundle.write_text('invalid test certificate bundle\n')
                    before = server.http_requests
                    scheme = 'wss' if mode == 'ws' else 'https'
                    completed = subprocess.run([str(binary), f'{scheme}://127.0.0.1:{server.server_port}/', mode],
                                               cwd=profile, capture_output=True, text=True, timeout=15, check=True)
                    codes = [int(line) for line in completed.stdout.splitlines()]
                    request_delta = server.http_requests - before
                    passed = (codes == [0, 0] and request_delta == 2) if case == 'trusted' else (
                        len(codes) == 2 and all(code in (60, 77) for code in codes) and request_delta == 0)
                    results.append({'binary': binary.name, 'mode': mode, 'case': case,
                                    'curl_results': codes, 'http_requests': request_delta, 'passed': passed, 'errors': completed.stderr.splitlines()})
        report = {'passed': all(result['passed'] for result in results), 'cases': results,
                  'scope': 'Real libcurl connections through the production shared TLS policy; full rebuilt game runtime remains separate.'}
        (work/'result.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps(report, indent=2))
        return 0 if report['passed'] else 1
    finally:
        for server, _ in servers:
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    raise SystemExit(main())
