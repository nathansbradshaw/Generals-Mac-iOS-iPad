#pragma once

// GeneralsX @bugfix Codex 09/10/2026 Keep a bounded failure budget across peer connection replacement.
class SignallingRetryBudget
{
public:
    static constexpr int MaxAttempts = 3;

    bool CanStartAttempt() const { return m_attempts < MaxAttempts; }
    bool TryStartAttempt()
    {
        if (!CanStartAttempt())
            return false;
        ++m_attempts;
        return true;
    }
    int GetAttempts() const { return m_attempts; }
    void ResetAfterConnected() { m_attempts = 0; }

private:
    int m_attempts = 0;
};

static_assert(sizeof(SignallingRetryBudget) == sizeof(int), "PlayerConnection retry budget must preserve its prior integer layout");
