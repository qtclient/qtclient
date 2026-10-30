#pragma once

// CIP-named dailyreward surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::dailyreward {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TDailyRewardFixedItems final : public ::tibia::CipTyped {};

class TDailyRewardGameActionHandler final : public cip::tibia::dailyreward::TDailyRewardGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::dailyreward::TDailyRewardGameActionHandler::TDailyRewardGameActionHandler;
};

class TDailyRewardPickItems final : public ::tibia::CipTyped {};

class TDailyRewardProtocolMessageHandler final : public cip::tibia::dailyreward::TDailyRewardProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::dailyreward::TDailyRewardProtocolMessageHandler::TDailyRewardProtocolMessageHandler;
};

class TDailyRewardStorage final : public cip::tibia::dailyreward::TDailyRewardStorage
{
    Q_OBJECT
public:
    using cip::tibia::dailyreward::TDailyRewardStorage::TDailyRewardStorage;
};

} // namespace tibia::dailyreward

