#pragma once
// GeneralsX @bugfix Codex 09/10/2026 Retain early WSS starts until the joined lobby can dispatch them.
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

struct PendingLobbyPeer
{
    int64_t userID;
    std::string middlewareID;
    uint16_t preferredPort;
};

class PendingLobbySignalling
{
public:
    static constexpr size_t MaxPeers = 8;
    uint64_t BeginJoin() { Reset(); m_state = State::Joining; return m_generation; }
    bool Activate(uint64_t generation)
    {
        if (generation != m_generation || m_state != State::Joining)
            return false;
        m_state = State::Active;
        return true;
    }
    uint64_t Generation() const { return m_generation; }
    void Reset() { m_peers.clear(); m_state = State::Idle; ++m_generation; }
    size_t Size() const { return m_peers.size(); }

    bool Queue(int64_t userID, const std::string& middlewareID, uint16_t preferredPort)
    {
        if (m_state == State::Idle || userID <= 0 || middlewareID.size() > 256)
            return false;
        const auto found = std::find_if(m_peers.begin(), m_peers.end(),
            [userID](const PendingLobbyPeer& peer) { return peer.userID == userID; });
        if (found != m_peers.end())
            *found = {userID, middlewareID, preferredPort};
        else if (m_peers.size() < MaxPeers)
            m_peers.push_back({userID, middlewareID, preferredPort});
        else
            return false;
        return true;
    }

    template<class IsCurrentMember>
    std::vector<PendingLobbyPeer> TakeReady(bool dispatchReady, IsCurrentMember isCurrentMember)
    {
        std::vector<PendingLobbyPeer> ready;
        if (m_state != State::Active || !dispatchReady)
            return ready;
        for (auto it = m_peers.begin(); it != m_peers.end();)
        {
            // A roster update can arrive after the start. Retain unknown peers, never start them.
            if (isCurrentMember(it->userID))
            {
                ready.push_back(*it);
                it = m_peers.erase(it);
            }
            else
                ++it;
        }
        return ready;
    }
private:
    enum class State { Idle, Joining, Active };
    State m_state = State::Idle;
    uint64_t m_generation = 0;
    std::vector<PendingLobbyPeer> m_peers;
};
