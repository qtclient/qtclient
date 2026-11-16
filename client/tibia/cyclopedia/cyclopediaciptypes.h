#pragma once

// CIP-named cyclopedia surfaces (promoted from cipwire).

#include "client/gamecore.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <client/staticdata.h>

#include <QObject>

#include <optional>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessageBestiaryTracker;
class GameserverMessageCyclopediaCharacterInfo;
class GameserverMessageImpactTracking;
class GameserverMessageItemLooted;
class GameserverMessageKillTracking;
class GameserverMessageItemWasted;
class GameserverMessageResourceBalance;
class GameserverMessageCyclopediaHouseActionResult;
class GameserverMessageMarketStatistics;
class GameserverMessageMonsterCyclopedia;
class GameserverMessageMonsterCyclopediaNewDetails;
class GameserverMessageXpChanged;
}

namespace tibia {
namespace cyclopedia {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TBestiaryTrackerProtocolMessageHandler final : public cip::tibia::cyclopedia::TBestiaryTrackerProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TBestiaryTrackerProtocolMessageHandler::TBestiaryTrackerProtocolMessageHandler;

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void onServerMessageEnqueued(int opcode);
    /// The tracked races with their capitalised names ("onCreatureTracker": tracker type,
    /// rows; tracker 0 is the bestiary's, any other the bosstiary's). The raw rows also go
    /// out as "onParseCyclopediaTracker" for packs.
    static void handleBestiaryTrackerMessage(const ::tibia::protobuf::protocol::GameserverMessageBestiaryTracker& message);
};

class TCharacterInfoStorage final : public cip::tibia::cyclopedia::TCharacterInfoStorage
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TCharacterInfoStorage::TCharacterInfoStorage;
};

// TConcoctionQmlData — real implementation in combatstatsqmldata.h.

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
public:
    /// Tip Q→H wire: XpChanged, ItemWasted, MonsterCyclopediaNewDetails and ResourceBalance from `serverMessageEnqueued`.
    static void onServerMessageEnqueued(int opcode);

    // Slots, one per server message.
    /// One kill: its drops, stamped with the current time, go to the loot tracking, the
    /// monster's name to the hunting session ("onKillTracker": name, outfit, drops).
    static void handleKillTrackingMessage(const ::tibia::protobuf::protocol::GameserverMessageKillTracking& message);
    /// One page of the character info: an error, or the page's data by info type (no
    /// combat stats and no wheel page here).
    static void handleCyclopediaCharacterInfoMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& message);
    /// Healing, damage dealt (physical and elemental part) and damage received with its source.
    static void handleImpactTrackingMessage(const ::tibia::protobuf::protocol::GameserverMessageImpactTracking& message);
    /// One object used up; a cumulative object counts once.
    static void handleItemWastedMessage(const ::tibia::protobuf::protocol::GameserverMessageItemWasted& message);
    /// One looted object: a cumulative object with its count, any other once.
    static void handleItemLootedMessage(const ::tibia::protobuf::protocol::GameserverMessageItemLooted& message);
    /// The charm point balances (resource types 30..33); other types are not this handler's.
    static void handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message);
    static void handleMarketStatisticsMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketStatistics& message);
    static void handleMonsterCyclopediaMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopedia& message);
    static void handleMonsterCyclopediaNewDetailsMessage(
        const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaNewDetails& message);
    static void handleCyclopediaHouseActionResulMessage(
        const ::tibia::protobuf::protocol::GameserverMessageCyclopediaHouseActionResult& message);
    static void handleXpChangedMessage(const ::tibia::protobuf::protocol::GameserverMessageXpChanged& message);

    // Requests (CipSoft's ICyclopediaProtocol slots).
    /// Cyclopedia inspection of an object type (InspectObject type 3, no count).
    static void sendInspectCyclopediaObject(uint16_t objectId);
    static void sendMarketStatistics();
    static void sendOpenMonsterCyclopedia();
    /// The races of a classification.
    static void sendOpenMonsterCyclopediaMonsters(const QString& classification);
    /// The races of a search result.
    static void sendOpenMonsterCyclopediaMonsters(const std::vector<uint16_t>& raceIds);
    static void sendOpenMonsterCyclopediaRace(uint32_t raceId);
    /// MonsterBonusEffectAction 0..3: unlock, assign to a race, remove, reset all.
    static void sendUnlockCharm(uint32_t charmId);
    static void sendAssignCharm(uint32_t charmId, uint32_t raceId);
    static void sendRemoveCharm(uint32_t charmId);
    static void sendResetAllCharms();
    /// CyclopediaMapAction: an area's data, a donation, resetting an area's viewpoints.
    static void sendRequestMapArea(uint32_t areaId);
    static void sendMapDonation(uint32_t areaId, uint64_t amount);
    static void sendResetMapAreaViewpoints(uint32_t areaId);
    static void sendOpenCharacterInfo(uint32_t characterId, int infoType);
    /// Paged form; general stats also request the wheel page.
    static void sendOpenCharacterInfo(uint32_t characterId, int infoType, uint32_t entriesPerPage, uint32_t page);
    /// CyclopediaHouseAction 0..7.
    static void sendRequestTownHouses(const QString& town);
    static void sendHouseBid(uint32_t houseId, uint64_t bid);
    static void sendHouseMoveOut(uint32_t houseId, uint32_t date);
    static void sendHouseTransfer(uint32_t houseId, uint32_t date, const QString& newOwner, uint64_t price);
    static void sendHouseCancelMoveOut(uint32_t houseId);
    static void sendHouseCancelTransfer(uint32_t houseId);
    static void sendHouseAcceptTransfer(uint32_t houseId);
    static void sendHouseRejectTransfer(uint32_t houseId);
    /// PartyHuntAnalyser 0 and 2: reset the session; send the loot prices of objects.
    static void sendResetPartyHuntAnalyser();
    static void sendPartyHuntObjectPrices(const std::vector<uint16_t>& objectIds);
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

// TPlayerMiscStatsProvider — real implementation in playermiscstatsprovider.h.

// TPlayerOffenceStatsProvider — real implementation in playeroffencestatsprovider.h.

}
}
