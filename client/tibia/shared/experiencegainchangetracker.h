#pragma once

namespace tibia::shared {

/// CIP shared::IExperienceGainChangeTracker: tells whether an experience gain
/// rate changed since the last call.
class IExperienceGainChangeTracker
{
public:
    virtual ~IExperienceGainChangeTracker() = default;
    virtual bool update(double rate) = 0;
};

/// CIP shared::TExperienceGainChangeTracker: keeps the last rate (0 at start)
/// and reports a change on any difference; NaN always counts as a change.
class TExperienceGainChangeTracker final : public IExperienceGainChangeTracker
{
public:
    bool update(const double rate) override
    {
        if (m_rate == rate)
            return false;
        m_rate = rate;
        return true;
    }

private:
    double m_rate = 0.0;
};

}
