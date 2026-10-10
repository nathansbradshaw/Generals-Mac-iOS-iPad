# GeneralsOnline self-host: connectivity model & options

Reference for how clients reach the self-hosted backend and each other, and the
open decisions for going beyond LAN. Written 2026-07-13; no decision made yet.

## The one fact that drives everything

The backend carries only **two** things:
1. The **HTTP API** (login, lobbies, stats).
2. The **signalling websocket** (lobby state + P2P rendezvous).

The actual **gameplay is peer-to-peer** (GameNetworkingSockets ICE) and never
transits the backend. So "connecting players" is really two separate problems:

- **(A) reach the backend** — HTTP + WS.
- **(B) establish P2P** between the players (ICE: direct / STUN / TURN).

## What's already built (works today)

- **In-app server field** (Extras menu → Online Server) + env/`Options` override.
  Persists to `settings.json`; the client uses whatever host you enter.
- **Backend binds `0.0.0.0`** (was `localhost`) — reachable from other devices.
- **Backend derives `ws_uri` from the request host** (`Core.ws_derive_from_request_host`,
  default true). So if a client reaches the API at `https://HOST:PORT/...`, login
  hands back `wss://HOST:PORT/ws` — the websocket follows the same host/port with
  zero server reconfig. (Verified: localhost→localhost, LAN IP→LAN IP, and through
  a Docker port map localhost:9010→:9010.)
- **STUN works**: the last LAN match's ICE gathered a `typ srflx` (public,
  STUN-discovered) candidate alongside `typ host`. ICE policy is `_All`
  (direct+STUN+relay), always on — no LAN-vs-internet "mode" in the client.
- **Docker stack** (`~/go-services/`): `docker-compose.yml` (mariadb + backend,
  LAN mode) built + smoke-tested; `docker-compose.tailscale.yml` overlay staged.

## Mode 1 — LAN / at-home (keep this)

Nothing extra. Backend on the host (launchd or Docker); each player enters the
host's **LAN IP** in the Extras field (`https://192.168.x.y:9000/env/prod/contract/1`).
Direct ICE wins on a shared network. This is the current, working model — untouched
by the multi-device work and coexists with the modes below on the same backend.

## Mode 2 — playit.gg / public tunnel (remote friends, no friend-side install)

**Reaching the backend: genuinely "just change the host."** The host runs a
tunnel (playit.gg / Cloudflare Tunnel) that maps a public `HOST:PORT` → the
backend's `:9000`. Friends paste `https://HOST:PORT/env/prod/contract/1` into the
Extras field and install nothing. The ws-derive fix makes the websocket follow
the tunnel automatically. TLS: playit's raw TCP tunnel passes the backend's
self-signed cert through end-to-end; the client verifies-off, so it's accepted.

**The P2P caveat (this part is NOT "just the host"):**
| Friend's network | P2P result |
|---|---|
| Typical home router (full/restricted-cone NAT) | ✅ connects via STUN (works today) |
| Strict / symmetric NAT, CGNAT, most mobile hotspots | ⚠️ needs a **TURN relay** |

Without TURN, strict-NAT friends can reach the lobby but fail to start the match.
Current TURN default (`turn.playgenerals.online`) is the production host and does
not work for self-host; overridable via `GENERALSX_ONLINE_TURN_SERVERS`, and the
per-lobby TURN creds come from the backend (currently unset).

## Mode 3 — Tailscale / mesh VPN (considered, deprioritized)

Puts every device on a flat virtual network → solves (A) and (B) at once, direct
ICE like LAN. Robust, but **each friend must install Tailscale + make an account +
join the tailnet** — too much friend-side friction for casual play. Overlay is
staged (`docker-compose.tailscale.yml`, needs a `TS_AUTHKEY`) if ever wanted.

## Recommendation shape (for the later decision)

Keep **Mode 1 (LAN)** as-is. For remote play, prefer **Mode 2 (tunnel)** over
Tailscale because friends install nothing. The only thing that makes Mode 2
bulletproof is a **self-hosted TURN server (coturn, in Docker)** for the
strict-NAT minority — a one-time host setup, still zero friend-side install.

## Open decisions (deferred)

1. **Exposure method for remote play** — playit.gg (easiest, no router access) vs
   Cloudflare Tunnel (free, robust) vs port-forward (no third party, needs router
   + dynamic DNS). Host's infra choice.
2. **Add coturn TURN container?** — closes the strict/symmetric-NAT P2P gap.
   Optional (STUN covers typical home NATs). All-Docker; would wire the client's
   TURN config / backend TURN creds to it.
3. **Rotate the JWT signing key** — still outstanding. It was committed+pushed to
   the public fork (history); scrubbed from the working tree but rotation is the
   real fix (`openssl rand -hex 48` → update both `.env` + launchd plist → restart;
   invalidates per-session tokens only). See secret-hygiene entry in PORTING_LOG.md.

## Cross-platform note

Same model applies to iOS/iPad: the iPad enters the host address in the Extras
field. For iPad↔Mac LAN (T5.4) that's the Mac's LAN IP; the earlier iPad
`localhost:9000` VersionCheck failure was just the default URL, resolved by
entering a reachable host. iOS still needs a non-env auth/token path (no shell env
on device) for a real login — separate from this connectivity work.
