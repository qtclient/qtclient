#pragma once

// CIP-named cyclopedia surfaces (promoted from cipwire).

#include "client/gamecore.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <client/staticdata.h>

#include <QObject>

#include <optional>
#include <vector>

namespace tibia::cyclopedia {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TBestiaryTrackerProtocolMessageHandler final : public cip::tibia::cyclopedia::TBestiaryTrackerProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TBestiaryTrackerProtocolMessageHandler::TBestiaryTrackerProtocolMessageHandler;
};

class TCharacterInfoStorage final : public cip::tibia::cyclopedia::TCharacterInfoStorage
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TCharacterInfoStorage::TCharacterInfoStorage;
};

class TConcoctionQmlData final : public cip::tibia::cyclopedia::TConcoctionQmlData
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TConcoctionQmlData::TConcoctionQmlData;
};

// TCreatureTrackerStorage — real implementation in creaturetrackerstorage.h.

class TCyclopediaMapCharacterConfigurationStorage final : public cip::tibia::cyclopedia::TCyclopediaMapCharacterConfigurationStorage
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TCyclopediaMapCharacterConfigurationStorage::TCyclopediaMapCharacterConfigurationStorage;
};

class TCyclopediaMapGameActionProvider final : public ::tibia::CipTyped {};

class TCyclopediaMapStaticDataStorage final : public cip::tibia::cyclopedia::TCyclopediaMapStaticDataStorage
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TCyclopediaMapStaticDataStorage::TCyclopediaMapStaticDataStorage;
};

class TCyclopediaProtocolMessageHandler final : public cip::tibia::cyclopedia::TCyclopediaProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TCyclopediaProtocolMessageHandler::TCyclopediaProtocolMessageHandler;
};

/// CipSoft cache of the combined Cyclopedia combat-stats packet (also consumed by
/// TCharacterInfoDialogController for CipSoft offence / defence qml data).
class TGeneralPlayerStatisticsProvider final : public ::tibia::CipTyped
{
public:
    TGeneralPlayerStatisticsProvider();
    const std::optional<CyclopediaCharacterCombatStats>& lastCombatStats() const { return m_combat; }
    double lastMitigation() const { return m_mitigation; }

private:
    std::vector<client::gamecore::Hook> m_hooks;
    std::optional<CyclopediaCharacterCombatStats> m_combat;
    double m_mitigation = 0;
};

class TItemPriceStorage final : public cip::tibia::cyclopedia::TItemPriceStorage
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TItemPriceStorage::TItemPriceStorage;
};

class TItemTrackingStorage final : public cip::tibia::cyclopedia::TItemTrackingStorage
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TItemTrackingStorage::TItemTrackingStorage;
};

class TMonsterBonusEffectStorage final : public cip::tibia::cyclopedia::TMonsterBonusEffectStorage
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TMonsterBonusEffectStorage::TMonsterBonusEffectStorage;
};

class TMonsterRaceStorage final : public cip::tibia::cyclopedia::TMonsterRaceStorage
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TMonsterRaceStorage::TMonsterRaceStorage;
};

/// CipSoft shells for split offence / defence / misc providers (CIP IOffenceStats etc.).
/// Character Info fills QML from protocol hooks; these keep the last packet for reuse.
class TPlayerDefenceStatsProvider final : public ::tibia::CipTyped
{
public:
    TPlayerDefenceStatsProvider();
    const std::optional<CyclopediaCharacterDefenceStats>& last() const { return m_last; }

private:
    std::vector<client::gamecore::Hook> m_hooks;
    std::optional<CyclopediaCharacterDefenceStats> m_last;
};

class TPlayerMiscStatsProvider final : public ::tibia::CipTyped
{
public:
    TPlayerMiscStatsProvider();
    const std::optional<CyclopediaCharacterMiscStats>& last() const { return m_last; }

private:
    std::vector<client::gamecore::Hook> m_hooks;
    std::optional<CyclopediaCharacterMiscStats> m_last;
};

class TPlayerOffenceStatsProvider final : public ::tibia::CipTyped
{
public:
    TPlayerOffenceStatsProvider();
    const std::optional<CyclopediaCharacterOffenceStats>& last() const { return m_last; }

private:
    std::vector<client::gamecore::Hook> m_hooks;
    std::optional<CyclopediaCharacterOffenceStats> m_last;
};

} // namespace tibia::cyclopedia
