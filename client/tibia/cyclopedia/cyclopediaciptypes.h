#pragma once

// CIP-named cyclopedia surfaces (promoted from cipwire).

#include "client/gamecore.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <client/outfit.h>
#include <client/staticdata.h>

#include <QObject>
#include <QStringList>

#include <map>
#include <memory>
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
class GameserverMessagePartyHuntAnalyser;
class GameserverMessageMonsterCyclopediaMonsters;
class GameserverMessageMonsterCyclopediaRace;
class GameserverMessageMonsterCyclopedia;
class GameserverMessageMonsterCyclopediaNewDetails;
class GameserverMessageXpChanged;
class GameserverMessageDead;
class GameserverMessageCyclopediaCurrentHouseData;
class GameserverMessageCyclopediaMapData;
class GameserverMessageCyclopediaStaticHouseData;
class GameserverMessageMonsterCyclopediaBonusEffects;
}

namespace tibia {
namespace cyclopedia {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TCreatureTrackerStorage;

class TBestiaryTrackerProtocolMessageHandler final : public cip::tibia::cyclopedia::TBestiaryTrackerProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TBestiaryTrackerProtocolMessageHandler::TBestiaryTrackerProtocolMessageHandler;

    /// The creature tracker storage the tracker message fills (CipSoft: handed to
    /// the constructor); none drops the message. Its owner clears it.
    static void setCreatureTrackerStorage(TCreatureTrackerStorage* pCreatureTrackerStorage);
    /// The tracked races with their capitalised names: tracker 0 is the bestiary's,
    /// any other the bosstiary's. The raw rows also go out as
    /// "onParseCyclopediaTracker" for packs.
    static void handleBestiaryTrackerMessage(const ::tibia::protobuf::protocol::GameserverMessageBestiaryTracker& message);
};

// TConcoctionQmlData — real implementation in combatstatsqmldata.h.

// TCreatureTrackerStorage — real implementation in creaturetrackerstorage.h.

// TCyclopediaMapCharacterConfigurationStorage — real implementation in cyclopediamapcharacterconfigurationstorage.h.

class THousesStorage;

/// Told once the static house data reached the houses storage (CIP: the
/// handler's static house data observer; the Cyclopedia map's static data
/// storage takes the house entrances from the houses storage).
class IStaticHouseDataObserver
{
public:
    virtual ~IStaticHouseDataObserver() = default;
    virtual void onStaticHouseDataChanged(const THousesStorage& HousesStorage) = 0;
};

class TCyclopediaProtocolMessageHandler final : public cip::tibia::cyclopedia::TCyclopediaProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The item price storage's requests for market statistics go out as the
    /// market statistics message. Host: CipSoft's constructor takes the
    /// session's storages; qtclient's handlers reach the client-wide ones.
    explicit TCyclopediaProtocolMessageHandler(QObject* parent = nullptr);

    /// Host: the client's handler, made on first use.
    static TCyclopediaProtocolMessageHandler* instance();

    /// The observer told after each static house data message; null for none.
    static void setStaticHouseDataObserver(std::shared_ptr<IStaticHouseDataObserver> pObserver);
    /// The houses storage the house data messages fill (CipSoft: the session's,
    /// handed to the constructor); none drops them. Its owner clears it.
    static void setHousesStorage(THousesStorage* pHousesStorage);
    /// The houses storage applied a static house data message.
    static void notifyStaticHouseDataObserver(const THousesStorage& HousesStorage);
    /// The charms (with the static table's name, description and kind), the races they
    /// can go to by charm kind, the free charm slots and the reset cost.
    static void handleMonsterCyclopediaBonusEffectsMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaBonusEffects& MonsterCyclopediaBonusEffectsMessage);
    static void handleCyclopediaCurrentHouseDataMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaCurrentHouseData& CyclopediaCurrentHouseDataMessage);
    static void handleCyclopediaStaticHouseDataMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaStaticHouseData& CyclopediaStaticHouseDataMessage);
    /// The map data message: each part goes to its map storage setter (the
    /// minimap marker part is the minimap's).
    static void handleCyclopediaMapDataMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaMapData& CyclopediaMapDataMessage);
public:

    // Slots, one per server message.
    /// The party hunt: session start, loot type (0 or 1 only), leader, then the members with names.
    static void handlePartyHuntAnalyserMessage(const ::tibia::protobuf::protocol::GameserverMessagePartyHuntAnalyser& message);
    /// The races of a classification: progress 0 shows as unknown without occurrence.
    static void handleMonsterCyclopediaMonstersMessage(
        const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaMonsters& message);
    /// One race's details, in steps by progress: occurrence and loot from 1, the
    /// characteristics from 2, element modifiers and locations from 3.
    static void handleMonsterCyclopediaRaceMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaRace& message);
    /// One kill: its drops go to the item tracking storage, the monster's name to the
    /// hunting session.
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
    static void handleCyclopediaHouseActionResultMessage(
        const ::tibia::protobuf::protocol::GameserverMessageCyclopediaHouseActionResult& message);
    static void handleXpChangedMessage(const ::tibia::protobuf::protocol::GameserverMessageXpChanged& message);
    /// The player died: the cyclopedia dialog closes (requestCloseCyclopediaDialog).
    static void handleDeadMessage(const ::tibia::protobuf::protocol::GameserverMessageDead& message);

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
    /// The party's loot type (0 market, 1 leader); with the leader's prices the prices of every
    /// cyclopedia object follow, 400 per message.
    static void sendPartyHuntLootType(int lootType);

Q_SIGNALS:
    void monsterCyclopediaNewDetailsForMonster(quint16 UpdatedMonsterRaceID);
    void requestHouseActionResultDialog(quint32 HouseID, quint8 Action, quint8 ErrorCode, bool HasBeenOutbidden);

private:
    std::shared_ptr<IStaticHouseDataObserver> m_pStaticHouseDataObserver;
    THousesStorage* m_pHousesStorage = nullptr;
};

class TItemPriceStorage final : public cip::tibia::cyclopedia::TItemPriceStorage
{
    Q_OBJECT
public:
    using cip::tibia::cyclopedia::TItemPriceStorage::TItemPriceStorage;
};

/// One charm of the player as the charm message and the static charm table give it
/// (CIP: TMonsterBonusEffect).
struct TMonsterBonusEffect
{
    uint8_t id = 0;
    QString name;
    /// The description with the percentage of `tier`.
    QString description;
    /// The static charm kind (0 offensive, 1 defensive, 2 minor).
    int type = 1;
    /// 0: not unlocked, 1..3.
    int tier = 0;
    /// The race the charm is assigned to, 0 for none.
    uint16_t raceId = 0;
    uint32_t removeCost = 0;
};

/// The player's charms (CIP: cyclopedia::TMonsterBonusEffectStorage).
class TMonsterBonusEffectStorage final : public cip::tibia::cyclopedia::TMonsterBonusEffectStorage
{
    Q_OBJECT
public:
    explicit TMonsterBonusEffectStorage(QObject* parent = nullptr);
    ~TMonsterBonusEffectStorage() override;
    static TMonsterBonusEffectStorage* instance();

    const std::map<uint8_t, TMonsterBonusEffect>& bonusEffects() const { return m_bonusEffects; }
    /// 0xFF: no limit.
    uint8_t remainingNumberOfAssignableBonusEffects() const { return m_remainingNumberOfAssignableBonusEffects; }
    uint64_t resetAllCharmCost() const { return m_resetAllCharmCost; }
    /// The charm state of a finished race the server sent (0..2), or -1.
    int finishedRaceState(uint16_t raceId) const;

    /// Each charm replaces the one with its id; charms the message leaves out stay.
    void setBonusEffects(const std::vector<TMonsterBonusEffect>& bonusEffects);
    void clearFinishedRaceStates();
    void setFinishedRaceState(uint16_t raceId, int state);
    /// Signals only a change.
    void setRemainingNumberOfAssignableBonusEffects(uint8_t remaining);
    void setResetAllCharmCost(uint64_t cost);

private:
    std::map<uint8_t, TMonsterBonusEffect> m_bonusEffects;
    uint8_t m_remainingNumberOfAssignableBonusEffects = 0;
    uint64_t m_resetAllCharmCost = 0;
    std::map<uint16_t, int> m_finishedRaceStates;
};

/// One monster race of the bestiary (CIP: TMonsterRace).
struct TMonsterRace
{
    /// One loot object of the race's details.
    struct Loot
    {
        uint16_t objectId = 0;
        QString name;
        /// 0 when the server leaves it out.
        int rarity = 0;
        int amount = 0;
        bool specialEvent = false;
    };
    struct Occurrence
    {
        int occurrence = 0;
        int extra = 0;
        std::vector<Loot> loot;
    };
    /// The second detail step.
    struct Characteristics
    {
        uint16_t charmValue = 0;
        int attackMode = 0;
        bool castsSpells = false;
        uint32_t maxHealth = 0;
        uint32_t experience = 0;
        uint16_t speed = 0;
        uint16_t armor = 0;
        double mitigation = 0;
    };
    /// The third detail step: element modifiers and locations.
    struct Combat
    {
        std::map<int, int> elements;
        QStringList locations;
    };

    uint16_t raceId = 0;
    QString name;
    int progress = 0;
    double animusMasteryBonus = 0;
    uint16_t animusMasteryPoints = 0;
    bool known = false;
    /// The race's appearance from the static data.
    Outfit appearance;
    /// Known once the race has progress.
    std::optional<Occurrence> occurrence;
    // The details of one race (MonsterCyclopediaRace).
    QString classificationName;
    uint32_t kills = 0;
    uint16_t firstUnlock = 0;
    uint16_t secondUnlock = 0;
    uint16_t lastUnlock = 0;
    std::optional<Characteristics> characteristics;
    std::optional<Combat> combat;
};

/// Every word of `name` with its first letter upper-cased; empty words dropped.
QString capitalizeWords(const QString& name);
/// The static race name, each word capitalised.
QString bossName(int raceId);
/// The static race name, each word capitalised (CIP: the monster race name lookup).
QString monsterRaceName(uint16_t raceId);
/// The static race's appearance, a monster's or else a boss's.
Outfit monsterRaceAppearance(uint16_t raceId);

/// The bestiary's races (CIP: cyclopedia::TMonsterRaceStorage).
class TMonsterRaceStorage final : public cip::tibia::cyclopedia::TMonsterRaceStorage
{
    Q_OBJECT
public:
    explicit TMonsterRaceStorage(QObject* parent = nullptr);
    ~TMonsterRaceStorage() override;
    static TMonsterRaceStorage* instance();

    /// One classification of the bestiary overview: its name, its races and the known ones.
    struct TMonsterClassificationSummary
    {
        QString name;
        int total = 0;
        int known = 0;
        bool operator==(const TMonsterClassificationSummary&) const = default;
    };
    const std::vector<TMonsterClassificationSummary>& monsterClassificationSummary() const { return m_monsterClassificationSummary; }
    /// The bestiary overview's classifications; only a change announces itself.
    void setMonsterClassificationSummary(std::vector<TMonsterClassificationSummary> summary);

    const QString& classificationName() const { return m_classificationName; }
    const std::vector<TMonsterRace>& classificationRaces() const { return m_classificationRaces; }
    /// The races of one classification (or of a search), replacing the previous ones.
    void setClassificationRaces(const QString& classificationName, std::vector<TMonsterRace> races);
    /// The race with `raceId`, or null.
    const TMonsterRace* race(uint16_t raceId) const;
    /// One race's details replace that race in the list (or join it).
    void setRaceDetails(TMonsterRace race);
    /// The races major and minor charms can be assigned to (from the charm message).
    const std::vector<TMonsterRace>& racesUnlockedForCharms() const { return m_racesUnlockedForCharms; }
    const std::vector<TMonsterRace>& racesUnlockedForMinorCharms() const { return m_racesUnlockedForMinorCharms; }
    void setRacesUnlockedForCharms(std::vector<TMonsterRace> races, std::vector<TMonsterRace> minorRaces);
    /// Forgets the races received (the classification's and the charms' lists),
    /// silently; the classification summary stays.
    void clearMonsterRaces();

Q_SIGNALS:
    void monsterClassificationMonstersChanged(const QString& MonsterClassificationID);
    void monsterRaceDetailsChanged(int ChangedRace);

private:
    std::vector<TMonsterClassificationSummary> m_monsterClassificationSummary;
    QString m_classificationName;
    std::vector<TMonsterRace> m_classificationRaces;
    std::vector<TMonsterRace> m_racesUnlockedForCharms;
    std::vector<TMonsterRace> m_racesUnlockedForMinorCharms;
};

// TPlayerDefenceStatsProvider — real implementation in playerdefencestatsprovider.h.

// TPlayerMiscStatsProvider — real implementation in playermiscstatsprovider.h.

// TPlayerOffenceStatsProvider — real implementation in playeroffencestatsprovider.h.

}
}
