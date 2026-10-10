// GeneralsX @bugfix Codex 09/10/2026 Regress replacement loops, independent peers, and confirmed-connect reset.
#include "GameNetwork/GeneralsOnline/SignallingRetry.h"
#include <cstdio>
#include <map>

struct Connection { SignallingRetryBudget retry; };

int main()
{
    std::map<int, Connection> peers;
    std::map<int, SignallingRetryBudget> budgets;
    int starts = 0;
    for (int failure = 0; failure < 31; ++failure)
    {
        SignallingRetryBudget& carried = budgets[1];
        if (!carried.TryStartAttempt())
            continue;
        peers.erase(1);
        peers[1] = Connection{};
        peers[1].retry = carried;
        ++starts;
    }
    if (starts != 3 || peers[1].retry.GetAttempts() != 3 || peers[1].retry.CanStartAttempt())
        return 1;
    // A terminal callback removes the player entry before delayed signaling arrives.
    peers.erase(1);
    if (budgets[1].TryStartAttempt() || budgets[1].GetAttempts() != 3)
        return 6;
    peers[1].retry = budgets[1];
    if (!peers[2].retry.TryStartAttempt() || peers[2].retry.GetAttempts() != 1)
        return 2;
    budgets[1].ResetAfterConnected();
    peers[1].retry.ResetAfterConnected();
    for (int attempt = 1; attempt <= 3; ++attempt)
        if (!peers[1].retry.TryStartAttempt() || peers[1].retry.GetAttempts() != attempt)
            return 3;
    if (peers[1].retry.TryStartAttempt())
        return 4;
    peers.clear();
    budgets.clear();
    if (!peers[1].retry.TryStartAttempt() || peers[1].retry.GetAttempts() != 1)
        return 5;
    std::puts("PASS: 31 failures bounded to 3 starts; terminal erasure guard, independent peer, confirmed-connect reset, fresh mesh");
    return 0;
}
