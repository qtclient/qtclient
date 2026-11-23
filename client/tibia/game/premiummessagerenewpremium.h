#pragma once

#include "game/premiummessage.h"

namespace tibia {
namespace game {

/// CIP game::TPremiumMessageRenewPremium: the premium panel's line of feature
/// 13, the premium days left.
class TPremiumMessageRenewPremium final : public TPremiumMessage
{
    Q_OBJECT
public:
    explicit TPremiumMessageRenewPremium(QObject* parent = nullptr);

    void setDaysLeft(int days) { m_daysLeft = days; }
    /// "premium_time_expires" for one day, else "premium_time_expires_plural",
    /// with the days left.
    QString message() const override;

private:
    int m_daysLeft = 0;
};

}
}
