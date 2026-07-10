#!/usr/bin/env python3
"""Mint a GeneralsOnline refresh token for friends-scale self-hosted login.

Replaces the Windows launcher's token acquisition. The self-hosted Services
backend accepts an HS256 refresh token signed with JwtSettings.Key from its
appsettings.json; the game client presents it to /LoginWithToken and gets a
session token + websocket URI back. See docs/port/go-online/BACKEND_NOTES.md.

Pre-seed the account first, e.g.:
  INSERT INTO users (account_type, displayname, active) VALUES (0,'nathan',1);

Then mint and export:
  export GENERALSX_ONLINE_REFRESH_TOKEN="$(scripts/go-online/mint_refresh_token.py \
      --user-id 34621 --name nathan --key <JwtSettings.Key>)"
  (run the game; the client reads that env var — no launcher needed.)

Only depends on the Python standard library.
"""

import argparse
import base64
import hashlib
import hmac
import json
import time
import uuid


def b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def mint(user_id: int, name: str, key: str, issuer: str, audience: str,
         address: str, ttl_minutes: int) -> str:
    now = int(time.time())
    header = {"alg": "HS256", "typ": "JWT"}
    # Claim contract per BACKEND_NOTES.md (verified against the live backend):
    # typ=1 refresh, client_id=5 custom_third_party_client, session_type=0 GameClient.
    payload = {
        "sub": str(user_id),
        "jti": str(uuid.uuid4()),
        "name": name,
        "address": address,
        "typ": "1",
        "client_id": "5",
        "session_type": "0",
        "http://schemas.microsoft.com/ws/2008/06/identity/claims/role":
            ["Player", "GameClient"],
        "iss": issuer,
        "aud": audience,
        "iat": now,
        "nbf": now,
        "exp": now + ttl_minutes * 60,
    }
    signing_input = (
        b64url(json.dumps(header, separators=(",", ":")).encode())
        + "."
        + b64url(json.dumps(payload, separators=(",", ":")).encode())
    )
    sig = hmac.new(key.encode(), signing_input.encode(), hashlib.sha256).digest()
    return signing_input + "." + b64url(sig)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--user-id", type=int, required=True, help="users.user_id from the DB")
    ap.add_argument("--name", required=True, help="displayname of the seeded account")
    ap.add_argument("--key", required=True, help="JwtSettings.Key from the backend appsettings.json")
    ap.add_argument("--issuer", default="go-lan", help="JwtSettings.Issuer (default: go-lan)")
    ap.add_argument("--audience", default="go-lan-clients", help="JwtSettings.Audience")
    ap.add_argument("--address", default="127.0.0.1", help="client IP claim")
    ap.add_argument("--ttl-minutes", type=int, default=43200, help="token lifetime (default 30 days)")
    args = ap.parse_args()
    print(mint(args.user_id, args.name, args.key, args.issuer, args.audience,
               args.address, args.ttl_minutes))


if __name__ == "__main__":
    main()
