#pragma once

// CIP-named cyclopedia surfaces (promoted from cipwire).

#include "client/gamecore.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <protocol.pb.h>

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
namespace analysers {
class TDamageInputStorage;
class TGainWasteStorage;
class THuntingSessionStorage;
class TImpactStorage;
class TPartyHuntStorage;
class TProgressStorage;
}
namespace cyclopedia {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TCreatureTrackerStorage;

class TBestiaryTrackerProtocolMessageHandler final : public cip::tibia::cyclopedia::TBestiaryTrackerProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The session's creature tracker storage the tracker message fills.
    explicit TBestiaryTrackerProtocolMessageHandler(std::shared_ptr<TCreatureTrackerStorage> pCreatureTrackerStorage,
                                                    QObject* parent = nullptr);

    /// The tracked races with their capitalised names: tracker 0 is the bestiary's,
    /// any other the bosstiary's. The raw rows also go out as
    /// "onParseCyclopediaTracker" for packs.
    void handleBestiaryTrackerMessage(const ::tibia::protobuf::protocol::GameserverMessageBestiaryTracker& message);

Q_SIGNALS:
    void sendTrackBestiaryRace(const ::tibia::protobuf::protocol::GameclientMessageTrackBestiaryRace& TrackBestiaryRaceMessage);

private:
    std::shared_ptr<TCreatureTrackerStorage> m_pCreatureTrackerStorage;
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

class TCharacterInfoStorage;
class TCyclopediaMapStorage;
class TItemTrackingStorage;
class TMonsterBonusEffectStorage;
class TMonsterRaceStorage;

class TCyclopediaProtocolMessageHandler final : public cip::tibia::cyclopedia::TCyclopediaProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The session's storages the messages fill.
    struct TStorages
    {
        std::shared_ptr<TItemTrackingStorage> itemTracking;
        std::shared_ptr<analysers::TGainWasteStorage> gainWaste;
        std::shared_ptr<analysers::TImpactStorage> impact;
        std::shared_ptr<analysers::TDamageInputStorage> damageInput;
        std::shared_ptr<analysers::TProgressStorage> progress;
        std::shared_ptr<analysers::THuntingSessionStorage> huntingSession;
        std::shared_ptr<analysers::TPartyHuntStorage> partyHunt;
        std::shared_ptr<TMonsterRaceStorage> monsterRace;
        std::shared_ptr<TMonsterBonusEffectStorage> monsterBonusEffect;
        std::shared_ptr<TCyclopediaMapStorage> cyclopediaMap;
        std::shared_ptr<TCharacterInfoStorage> characterInfo;
        std::shared_ptr<THousesStorage> houses;
    };

    /// The handler of the session's storages. The item price storage's
    /// requests for market statistics go out as the market statistics message.
    explicit TCyclopediaProtocolMessageHandler(TStorages Storages, QObject* parent = nullptr);

    /// The observer told after each static house data message; null for none.
    void setStaticHouseDataObserver(std::shared_ptr<IStaticHouseDataObserver> pObserver);
    /// The charms (with the static table's name, description and kind), the races they
    /// can go to by charm kind, the free charm slots and the reset cost.
    void handleMonsterCyclopediaBonusEffectsMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaBonusEffects& MonsterCyclopediaBonusEffectsMessage);
    void handleCyclopediaCurrentHouseDataMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaCurrentHouseData& CyclopediaCurrentHouseDataMessage);
    void handleCyclopediaStaticHouseDataMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaStaticHouseData& CyclopediaStaticHouseDataMessage);
    /// The map data message: each part goes to its map storage setter (the
    /// minimap marker part is the minimap's).
    void handleCyclopediaMapDataMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaMapData& CyclopediaMapDataMessage);
public:

    // Slots, one per server message.
    /// The party hunt: session start, loot type (0 or 1 only), leader, then the members with names.
    void handlePartyHuntAnalyserMessage(const ::tibia::protobuf::protocol::GameserverMessagePartyHuntAnalyser& message);
    /// The races of a classification: progress 0 shows as unknown without occurrence.
    void handleMonsterCyclopediaMonstersMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaMonsters& message);
    /// One race's details, in steps by progress: occurrence and loot from 1, the
    /// characteristics from 2, element modifiers and locations from 3.
    void handleMonsterCyclopediaRaceMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaRace& message);
    /// One kill: its drops go to the item tracking storage, the monster's name to the
    /// hunting session.
    void handleKillTrackingMessage(const ::tibia::protobuf::protocol::GameserverMessageKillTracking& message);
    /// One page of the character info: an error, or the page's data by info type (no
    /// combat stats and no wheel page here).
    void handleCyclopediaCharacterInfoMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& message);
    /// Healing, damage dealt (physical and elemental part) and damage received with its source.
    void handleImpactTrackingMessage(const ::tibia::protobuf::protocol::GameserverMessageImpactTracking& message);
    /// One object used up; a cumulative object counts once.
    void handleItemWastedMessage(const ::tibia::protobuf::protocol::GameserverMessageItemWasted& message);
    /// One looted object: a cumulative object with its count, any other once.
    void handleItemLootedMessage(const ::tibia::protobuf::protocol::GameserverMessageItemLooted& message);
    /// The charm point balances (resource types 30..33); other types are not this handler's.
    static void handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message);
    void handleMarketStatisticsMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketStatistics& message);
    void handleMonsterCyclopediaMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopedia& message);
    void handleMonsterCyclopediaNewDetailsMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaNewDetails& message);
    void handleCyclopediaHouseActionResultMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaHouseActionResult& message);
    void handleXpChangedMessage(const ::tibia::protobuf::protocol::GameserverMessageXpChanged& message);
    /// The player died: the cyclopedia dialog closes (requestCloseCyclopediaDialog).
    void handleDeadMessage(const ::tibia::protobuf::protocol::GameserverMessageDead& message);

    // Requests (CipSoft's ICyclopediaProtocol slots): each builds its message and
    // emits the matching send signal; the session links them to its message queue.
    /// Cyclopedia inspection of an object type (InspectObject type 3, no count).
    void sendInspectCyclopediaObject(uint16_t objectId);
    void requestMarketStatistics();
    void requestMonsterCyclopedia();
    /// The races of a classification.
    void requestMonsterCyclopediaMonsters(const QString& classification);
    /// The races of a search result.
    void requestMonsterCyclopediaMonsters(const std::vector<uint16_t>& raceIds);
    void requestMonsterCyclopediaRace(uint32_t raceId);
    /// MonsterBonusEffectAction 0..3: unlock, assign to a race, remove, reset all.
    void sendUnlockCharm(uint32_t charmId);
    void sendAssignCharm(uint32_t charmId, uint32_t raceId);
    void sendRemoveCharm(uint32_t charmId);
    void sendResetAllCharms();
    /// CyclopediaMapAction: an area's data, a donation, resetting an area's viewpoints.
    void sendRequestMapArea(uint32_t areaId);
    void sendMapDonation(uint32_t areaId, uint64_t amount);
    void sendResetMapAreaViewpoints(uint32_t areaId);
    void sendOpenCharacterInfo(uint32_t characterId, int infoType);
    /// Paged form; general stats also request the wheel page.
    void sendOpenCharacterInfo(uint32_t characterId, int infoType, uint32_t entriesPerPage, uint32_t page);
    /// CyclopediaHouseAction 0..7.
    void sendRequestTownHouses(const QString& town);
    void sendHouseBid(uint32_t houseId, uint64_t bid);
    void sendHouseMoveOut(uint32_t houseId, uint32_t date);
    void sendHouseTransfer(uint32_t houseId, uint32_t date, const QString& newOwner, uint64_t price);
    void sendHouseCancelMoveOut(uint32_t houseId);
    void sendHouseCancelTransfer(uint32_t houseId);
    void sendHouseAcceptTransfer(uint32_t houseId);
    void sendHouseRejectTransfer(uint32_t houseId);
    /// PartyHuntAnalyser 0 and 2: reset the session; send the loot prices of objects.
    void sendResetPartyHuntAnalyser();
    void sendPartyHuntObjectPrices(const std::vector<uint16_t>& objectIds);
    /// The party's loot type (0 market, 1 leader); with the leader's prices the prices of every
    /// cyclopedia object follow, 400 per message.
    void sendPartyHuntLootType(int lootType);

Q_SIGNALS:
    void sendInspectObject(const ::tibia::protobuf::protocol::GameclientMessageInspectObject& InspectObjectMessage);
    void sendMarketStatistics(const ::tibia::protobuf::protocol::GameclientMessageMarketStatistics& MarketStatisticsMessage);
    void sendOpenMonsterCyclopedia(const ::tibia::protobuf::protocol::GameclientMessageOpenMonsterCyclopedia& MonsterCyclopediaMessage);
    void sendOpenMonsterCyclopediaMonsters(
        const ::tibia::protobuf::protocol::GameclientMessageOpenMonsterCyclopediaMonsters& MonsterCyclopediaMonstersMessage);
    void sendOpenMonsterCyclopediaRace(const ::tibia::protobuf::protocol::GameclientMessageOpenMonsterCyclopediaRace& MonsterCyclopediaRaceMessage);
    void sendMonsterBonusEffectAction(const ::tibia::protobuf::protocol::GameclientMessageMonsterBonusEffectAction& MonsterBonusEffectAction);
    void sendCyclopediaMapAction(const ::tibia::protobuf::protocol::GameclientMessageCyclopediaMapAction& CyclopediaMapAction);
    void sendOpenCyclopediaCharacterInfo(const ::tibia::protobuf::protocol::GameclientMessageOpenCyclopediaCharacterInfo& OpenCyclopediaCharacterInfo);
    void sendCyclopediaHouseAction(const ::tibia::protobuf::protocol::GameclientMessageCyclopediaHouseAction& CyclopediaHousActionMessage);
    void sendPartyHuntAnalyser(const ::tibia::protobuf::protocol::GameclientMessagePartyHuntAnalyser& Message);
    void monsterCyclopediaNewDetailsForMonster(quint16 UpdatedMonsterRaceID);
    void requestHouseActionResultDialog(quint32 HouseID, quint8 Action, quint8 ErrorCode, bool HasBeenOutbidden);

private:
    void sendHouseAction(uint32_t action, uint32_t houseId);
    void sendMonsterBonusEffect(uint32_t action, std::optional<uint32_t> charmId, std::optional<uint32_t> raceId);

private:
    TStorages m_storages;
    std::shared_ptr<IStaticHouseDataObserver> m_pStaticHouseDataObserver;
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
