# Two-client GeneralsOnline test runbook (macOS, self-hosted)

How to drive two GeneralsX clients through the online flow (login → lobby list →
host/join → staging room → ready → match start) against the self-hosted backend
on one Mac. Uses the `-*Autostart` command-line test hooks so no clicking is
required. All hooks are guarded `SAGE_GENERALS_ONLINE`; they do nothing in an
OFF build and are cut from any release.

## 0. Prerequisites (one-time)

**Backend** (see `BACKEND_NOTES.md` for full setup):

```sh
# MariaDB (Docker) — usually already running from a prior session
docker ps --filter name=go-mariadb            # should show "Up"
# .NET service — runs as a launchd agent (auto-respawns if killed):
launchctl list | grep com.generalsx.service   # running?
launchctl kickstart -k gui/$(id -u)/com.generalsx.service   # (re)start
tail -f /tmp/go-service-debug.log             # service stdout
# It runs /private/tmp/go-services-debug/GenOnlineService/bin/ARM64/Debug/net10.0
# (NOTE: /tmp is wiped on reboot). After editing service code, rebuild with
#   dotnet build -p:Platform=ARM64      # plain `dotnet build` targets bin/Debug — wrong dir
# then kickstart again. A TURN agent (com.generalsx.turn) runs alongside.
# Listens: https://localhost:9000 (+ ws), http://localhost:9001
```

Quick liveness check (401 = up and requiring auth; connection-refused = down):

```sh
curl -sk --max-time 3 https://localhost:9000/env/prod/contract/1/MOTD \
  -o /dev/null -w "http=%{http_code}\n"
```

**Accounts** — pre-seed a couple of users (friends-scale, no registration flow):

```sh
docker exec go-mariadb sh -lc \
 'mariadb -uroot -pdev generalsonline -e \
  "INSERT INTO users (account_type, displayname, active) VALUES (0,'\''nathan'\'',1),(0,'\''friend1'\'',1);"'
docker exec go-mariadb sh -lc \
 'mariadb -uroot -pdev generalsonline -e "SELECT user_id, displayname FROM users;"'
# note the user_ids (e.g. nathan=34621, friend1=34622); each client needs a DIFFERENT account
```

**Build + deploy** the client (ON):

```sh
cd ~/Documents/GitHub/GeneralsX
cmake -B build/macos-vulkan -DSAGE_GENERALS_ONLINE=ON   # if not already configured ON
cmake --build build/macos-vulkan --target GeneralsXZH -j8
bash scripts/build/macos/deploy-macos-zh.sh             # deploys binary + dylibs + run.sh to ~/GeneralsX/GeneralsZH
```

## 1. Auth: how login works here

No launcher / no browser. The client reads a **pre-minted refresh token** from
the `GENERALSX_ONLINE_REFRESH_TOKEN` env var, POSTs `LoginWithToken`, and gets a
session token + websocket URI back. Mint a token per account with the backend's
JWT signing key (`JwtSettings.Key` from the service `appsettings.json`):

```sh
KEY="43a0f039d4469a2b23e1bd8dfd54a69c5e25e8cd581226b36591eb587c75dfbb1247b76a3ab20870bc04ae8f181d215d"
python3 scripts/go-online/mint_refresh_token.py --user-id 34621 --name nathan --key "$KEY"
```

The services base URL defaults to `https://localhost:9000/env/prod/contract/1`.
Override with `GENERALSX_ONLINE_URL` to point at a LAN host (see T4.2 in
`PORTING_LOG.md`).

## 2. Command-line test hooks

Passed after `-win` on the `run.sh` line. Each implies the ones before it:

| Flag | Effect |
|---|---|
| `-onlineAutostart` | At the main menu, auto-enter the online flow (as if clicking "Online") and log in with the env token. |
| `-hostAutostart` | …then, in the custom lobby, create a default lobby ("GeneralsX Autohost", default map) — no host popup. |
| `-joinAutostart` | …then, in the custom lobby, join the first lobby not owned by this account. |

Definitions: `parse*Autostart` in `Common/CommandLine.cpp`; triggers in
`MainMenu.cpp` (online) and `WOLLobbyMenu.cpp` (host/join).

## 3. Run the two clients

The clients share one runtime dir (`~/GeneralsX/GeneralsZH`); give each its own
account token and its own log. `run.sh` is the wrapper that sets the dylib env.

```sh
cd ~/GeneralsX/GeneralsZH
KEY="43a0f039d4469a2b23e1bd8dfd54a69c5e25e8cd581226b36591eb587c75dfbb1247b76a3ab20870bc04ae8f181d215d"

# --- Client A: HOST (nathan) ---
export GENERALSX_ONLINE_REFRESH_TOKEN=$(python3 ~/Documents/GitHub/GeneralsX/scripts/go-online/mint_refresh_token.py --user-id 34621 --name nathan --key "$KEY")
./run.sh -win -hostAutostart > /tmp/host.log 2>&1 &

# wait until the host has created its lobby before starting the joiner:
until grep -q 'Lobby/[0-9]\+|Verb 0] Response was 200' /tmp/host.log; do sleep 2; done

# --- Client B: JOINER (friend1, a DIFFERENT account) ---
export GENERALSX_ONLINE_REFRESH_TOKEN=$(python3 ~/Documents/GitHub/GeneralsX/scripts/go-online/mint_refresh_token.py --user-id 34622 --name friend1 --key "$KEY")
./run.sh -win -joinAutostart > /tmp/joiner.log 2>&1 &
```

Expected: both windows reach the staging room together; the backend lobby shows
`numcurrentplayers:2` and both members ready up (`isready:true`).

## 4. Watching progress (logs, not screenshots)

The SDL/DXVK window can't be screenshotted headlessly, so verify from the logs +
backend responses.

```sh
# online base URL + which backend endpoints were hit
grep -oE 'https://localhost:9000/[^ |]*' /tmp/joiner.log | sort | uniq -c

# join + 2-player state
grep -iE 'Joined lobby|numcurrentplayers.:2' /tmp/joiner.log | tail

# ready state (both should reach isready:true)
grep -oE '"displayname":"[a-z0-9]+","isready":(true|false)' /tmp/joiner.log | tail

# hang check: sample the main thread twice; a MOVING leaf frame = responsive,
# the SAME deep frame across samples = hung (usually a null-TheGameSpyInfo deref)
PID=$(pgrep -f 'GeneralsXZH -win' | head -1)
sample "$PID" 1 -mayDie 2>/dev/null | grep -m1 -A8 com.apple.main-thread
```

## 5. Cleanup

```sh
pkill -9 -f GeneralsXZH        # both windows beachball-proof kill
# lobbies are in-memory in the service: killing the host removes its lobby.
```

## Notes / gotchas

- **Two accounts required** — a second session on the same account will be
  rejected. Seed and mint a distinct account per client.
- **Lobbies are in-memory** in the .NET service (no DB table); restart the host
  to get a fresh lobby, and start the joiner only after the host's lobby exists.
- **`run.sh`, not the raw binary** — the wrapper sets `DYLD_LIBRARY_PATH` so the
  co-located DXVK/MoltenVK dylibs resolve; the bare binary fails to load d3d9.
- **`screencapture` doesn't work** in this environment ("could not create image
  from display"); rely on logs + `sample` for verification.
