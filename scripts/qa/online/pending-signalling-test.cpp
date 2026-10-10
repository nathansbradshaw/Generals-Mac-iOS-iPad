// GeneralsX @bugfix Codex 09/10/2026 Exercise early WSS/HTTP ordering and stale join completion.
#include "GameNetwork/GeneralsOnline/PendingLobbySignalling.h"
#include <cstdio>

int main()
{
    PendingLobbySignalling pending;
    const auto member = [](int64_t id) { return id == 1; };
    if (pending.Queue(1, "peer", 0)) return 1;
    const auto firstJoin = pending.BeginJoin();
    if (!pending.Queue(1, "old-port", 1) || !pending.Queue(1, "latest-port", 2) || pending.Size() != 1) return 2;
    if (!pending.TakeReady(true, member).empty()) return 3; // WSS before HTTP.
    if (!pending.Activate(firstJoin) || !pending.TakeReady(false, member).empty()) return 4; // Menu callbacks not installed.
    const auto ready = pending.TakeReady(true, member);
    if (ready.size() != 1 || ready[0].middlewareID != "latest-port" || ready[0].preferredPort != 2 || pending.Size()) return 5;
    if (!pending.TakeReady(true, member).empty()) return 6; // No repeated dispatch next Tick.
    // A legitimate new signal after connection establishment remains dispatchable.
    if (!pending.Queue(1, "re-signal", 0) || pending.TakeReady(true, member).size() != 1) return 15;
    for (int peer = 2; peer <= 9; ++peer) if (!pending.Queue(peer, "peer", 0)) return 7;
    if (pending.Queue(10, "overflow", 0) || pending.Size() != 8 || !pending.Queue(9, "updated", 3)) return 8;
    if (!pending.TakeReady(true, member).empty()) return 9; // Foreign or not-yet-rostered peers never start.
    const auto lateRoster = pending.TakeReady(true, [](int64_t id) { return id == 9; });
    if (lateRoster.size() != 1 || lateRoster[0].middlewareID != "updated") return 10;
    pending.Reset(); // Failed join or Leave: clear all and reject late WSS.
    if (pending.Size() || pending.Queue(1, "late", 0) || pending.Activate(firstJoin)) return 11;
    const auto nextJoin = pending.BeginJoin();
    if (pending.Activate(firstJoin) || !pending.Queue(1, "next-lobby", 0)) return 12;
    pending.BeginJoin(); // A newer join discards the queued peer and old HTTP completion.
    if (pending.Size() || pending.Activate(nextJoin)) return 13;
    if (pending.Queue(0, "peer", 0) || pending.Queue(1, std::string(257, 'x'), 0)) return 14;
    // Built-in GNS uses the NGMP user ID and legitimately supplies no middleware ID.
    const auto builtInJoin = pending.BeginJoin();
    if (!pending.Queue(1, "", 0) || !pending.Activate(builtInJoin)) return 16;
    const auto builtIn = pending.TakeReady(true, member);
    if (builtIn.size() != 1 || !builtIn[0].middlewareID.empty()) return 17;
    std::puts("PASS: early start retained; callback gate; dedup and max8; roster gate; failed/Leave/new join clear; stale HTTP generation rejected");
}
