# Self-hosted backend notes (T1.1–T1.3, done 07/2026)

Working local stack on the dev Mac:

## Database (Docker)
```sh
docker run -d --name go-mariadb \
  -e MARIADB_ROOT_PASSWORD=dev -e MARIADB_DATABASE=generalsonline \
  -p 3306:3306 -v go-mariadb-data:/var/lib/mysql mariadb:12
docker exec -i go-mariadb mariadb -uroot -pdev generalsonline \
  < ~/go-services/GenOnlineService/Database_Structure/structure.sql
```
14 tables. Interesting ones: `users`, `user_devices`, `pending_logins`
(login appears launcher/web-initiated — relevant to T4.3), `match_history`.

## Service (.NET 10)
- Repo: `~/go-services` (clone of GeneralsOnlineDevelopmentTeam/Services).
- SDK via user-local install (brew cask needs sudo):
  `curl -sSL https://dot.net/v1/dotnet-install.sh | bash -s -- --channel 10.0`
  then `export DOTNET_ROOT=$HOME/.dotnet; PATH=$DOTNET_ROOT:$PATH`.
- `dotnet build` at repo root; output lands in `bin/ARM64/Debug/net10.0/`
  (their solution sets Platform=ARM64 — `dotnet run` looks in `bin/Debug` and
  fails; run the DLL from its own directory instead).
- `appsettings.json` edits (in the project dir, then rebuild to copy):
  - `JwtSettings.Key` = random hex (openssl rand -hex 48); Issuer/Audience arbitrary.
  - `Database`: 127.0.0.1 / generalsonline / root / dev / 3306.
  - `MatchData`: **dummy S3 values required** — `S3CredentialManager.Initialize()`
    throws if unset even with `upload_match_data: false`; the AWS client is lazy
    so dummies are safe. (Portability patch candidate: skip init when disabled.)
  - Discord/Sentry/TURN/ExternalLeaderboards: leave disabled/null.
- Run: `cd bin/ARM64/Debug/net10.0 && dotnet GenOnlineService.dll`
- Listens: https://localhost:9000 (+ ws at /ws), http://localhost:9001.
  HTTPS uses the ASP.NET dev cert (`dotnet dev-certs https`); for LAN use we'll
  need either the insecure ws endpoint or a cert the clients trust (T4.2 concern).

## curl features needed by the client (T2.3 input)
Their config exposes `wss://…/ws` — client talks websockets. Verify vcpkg curl
has `websockets` feature on both triplets (or they use a different ws lib — check
NGMP HTTPManager during T3).

## Open (T1.4)
Account creation flow not yet discovered — `pending_logins` suggests the
launcher starts a login and the service completes it. Find the registration
endpoint or seed `users` directly via SQL.

## T1.4 — friends-scale auth (VERIFIED WORKING 07/2026)

No launcher, no external IdP, no code changes needed:

1. Seed users: `INSERT INTO users (account_type, displayname, active) VALUES (0,'<name>',1);`
2. Mint a **refresh token** (HS256, signed with `JwtSettings.Key` from appsettings):
   claims `sub`=<user_id>, `jti`=uuid, `name`=<displayname>, `address`=ip,
   `typ`="1" (refresh), `client_id`="5" (custom_third_party_client),
   `session_type`="0" (GameClient),
   `http://schemas.microsoft.com/ws/2008/06/identity/claims/role`=["Player","GameClient"],
   `iss`/`aud` per config, `exp`/`iat`/`nbf`. (Python stdlib mint script in git
   history of this file's commit.)
3. `POST /env/prod/contract/1/LoginWithToken` with `Authorization: Bearer <refresh>`,
   JSON body `{"exe_crc":"0"}` → HTTP 200 `{result:1, session_token, refresh_token,
   user_id, display_name, ws_uri}`.

So the game client (T4.3) only needs: a stored refresh token (config/ini),
LoginWithToken call, then session-token auth + websocket connect. Their enum
`custom_third_party_client=5` exists precisely for clients like ours.
Session-type enum: GameClient=0, ChatClient=1, GameLauncher=2.

## T4.2/T4.3 client wiring (2026-07-09)

- Client services base URL is now runtime-configurable (was compile-time PROD):
  env `GENERALSX_ONLINE_URL`, default `https://localhost:9000/env/prod/contract/1`.
  Our self-hosted backend serves the `/env/prod/contract/1/` path (the verified
  T1.4 flow used it). TLS: the client disables curl peer/host verification when
  no `cacert.pem` is present, so the ASP.NET dev cert is accepted as-is.
- Login without the launcher: supply a pre-minted refresh token via env
  `GENERALSX_ONLINE_REFRESH_TOKEN`. Mint with
  `scripts/go-online/mint_refresh_token.py --user-id <id> --name <displayname> --key <JwtSettings.Key>`.
  Our appsettings: Issuer `go-lan`, Audience `go-lan-clients` (script defaults).
- Seeded users (DB `users`): 34621 `nathan`, 34622 `friend1` (account_type 0, active 1).
- Client reads a credentials file at
  `~/Library/Application Support/GeneralsX/GeneralsZH/GeneralsOnlineData/credentials.json`
  (`{"refresh_token": "..."}`, plaintext off-Windows) — the env var takes
  precedence over it.
- Headless exercise: `./run.sh -win -onlineAutostart` with the env vars set drives
  main-menu → login → welcome → custom lobby automatically.
