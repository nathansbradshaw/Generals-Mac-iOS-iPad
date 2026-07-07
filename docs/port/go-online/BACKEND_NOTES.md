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
