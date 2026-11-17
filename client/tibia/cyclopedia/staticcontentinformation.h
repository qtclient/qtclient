#pragma once

#include "worldmap/coordinates.h"

#include <client/staticdata.h>

#include <QString>

#include <optional>
#include <vector>

namespace tibia {
namespace cyclopedia {

/// A monster classification of the static data ("Amphibic", "Undead", …).
struct TStaticMonsterClassification
{
    quint8 id = 0;
    QString name;
};

/// An achievement of the static data.
struct TStaticAchievement
{
    quint16 id = 0;
    quint8 grade = 0;
    QString name;
    QString description;
};

/// A house of the static data.
struct TStaticHouse
{
    quint32 id = 0;
    QString name;
    QString description;
    quint16 size = 0;
    quint8 beds = 0;
    quint32 rent = 0;
    QString town;
    bool isGuildHall = false;
    bool isShop = false;
    worldmap::TWorldMapCoordinate position;

    /// The stand-in for a house the static data does not name: every text is
    /// "dummy_unknown", every number zero.
    static TStaticHouse unknown(quint32 id);
};

/// A quest of the static data.
struct TStaticQuest
{
    int id = 0;
    QString name;
};

/// The static data file, as the asset handler hands it over once it is
/// loaded (CIP: cyclopedia::TStaticContentInformation).
struct TStaticContentInformation
{
    std::vector<RaceType> monsters;
    std::vector<TStaticMonsterClassification> monsterClassifications;
    std::vector<RaceType> bosses;
    std::vector<TStaticAchievement> achievements;
    std::vector<TStaticHouse> houses;
    std::vector<TStaticQuest> quests;
};

/// Replaces the static content: the tables below (sorted by id, plus the
/// classification and quest look-ups); the appearances manager also gets
/// every race for its name searches.
void setStaticContentInformation(TStaticContentInformation information);

/// The monster or boss of the race id, null when the static data has none.
const RaceType* staticMonster(quint16 raceId);
const RaceType* staticBoss(quint16 raceId);

/// Sorted by id.
const std::vector<TStaticMonsterClassification>& monsterClassifications();
std::optional<TStaticMonsterClassification> monsterClassification(quint8 id);
std::optional<QString> monsterClassificationName(quint8 id);
std::optional<TStaticMonsterClassification> monsterClassificationByName(const QString& name);
std::optional<quint8> monsterClassificationId(const QString& name);

/// Sorted by id.
const std::vector<TStaticAchievement>& staticAchievements();
/// An achievement the static data does not know is "dummy_unknown" of grade 1.
TStaticAchievement staticAchievement(quint16 id);

/// Sorted by id.
const std::vector<TStaticHouse>& staticHouses();
/// TStaticHouse::unknown(id) when the static data does not name the house.
TStaticHouse staticHouse(quint32 id);

/// The quest's name; throws std::out_of_range for a quest the static data
/// does not know.
QString questName(int id);
bool hasQuest(int id);
/// Every quest, sorted by id (the pack ABI lists them).
std::vector<TStaticQuest> staticQuests();

}
}
