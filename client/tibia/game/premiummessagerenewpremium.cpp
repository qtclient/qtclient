#include "premiummessagerenewpremium.h"

namespace tibia::game {

namespace {

constexpr uint8_t RenewPremiumFeature = 0xd;

}

TPremiumMessageRenewPremium::TPremiumMessageRenewPremium(QObject* parent)
    : TPremiumMessage(RenewPremiumFeature, false, parent)
{
}

QString TPremiumMessageRenewPremium::message() const
{
    return qtTrId(m_daysLeft == 1 ? "premium_time_expires" : "premium_time_expires_plural").arg(m_daysLeft);
}

}
