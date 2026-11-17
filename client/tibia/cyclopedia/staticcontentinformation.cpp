#include "staticcontentinformation.h"

#include <tibia/assets/things.h>

#include <QHash>

#include <algorithm>
#include <stdexcept>

namespace tibia::cyclopedia {

namespace {

std::vector<TStaticMonsterClassification> s_monsterClassifications;
QHash<quint8, TStaticMonsterClassification> s_monsterClassificationsById;
QHash<QString, TStaticMonsterClassification> s_monsterClassificationsByName;
std::vector<TStaticAchievement> s_achievements;
std::vector<TStaticHouse> s_houses;
QHash<int, QString> s_questNames;

void setMonsterClassifications(std::vector<TStaticMonsterClassification> classifications)
{
    s_monsterClassifications = std::move(classifications);
    std::sort(s_monsterClassifications.begin(), s_monsterClassifications.end(),
              [](const TStaticMonsterClassification& a, const TStaticMonsterClassification& b) { return a.id < b.id; });
    s_monsterClassificationsById.clear();
    s_monsterClassificationsByName.clear();
    s_monsterClassificationsById.reserve(qsizetype(s_monsterClassifications.size()));
    s_monsterClassificationsByName.reserve(qsizetype(s_monsterClassifications.size()));
    // A later entry with the same id or name replaces the earlier one.
    for (const TStaticMonsterClassification& classification : s_monsterClassifications) {
        s_monsterClassificationsById.insert(classification.id, classification);
        s_monsterClassificationsByName.insert(classification.name, classification);
    }
}

void setAchievements(std::vector<TStaticAchievement> achievements)
{
    s_achievements = std::move(achievements);
    std::sort(s_achievements.begin(), s_achievements.end(),
              [](const TStaticAchievement& a, const TStaticAchievement& b) { return a.id < b.id; });
}

void setHouses(std::vector<TStaticHouse> houses)
{
    s_houses = std::move(houses);
    std::sort(s_houses.begin(), s_houses.end(), [](const TStaticHouse& a, const TStaticHouse& b) { return a.id < b.id; });
}

void addQuests(const std::vector<TStaticQuest>& quests)
{
    // The first name of an id stays.
    for (const TStaticQuest& quest : quests) {
        if (!s_questNames.contains(quest.id))
            s_questNames.insert(quest.id, quest.name);
    }
}

}

TStaticHouse TStaticHouse::unknown(const quint32 id)
{
    TStaticHouse house;
    house.id = id;
    house.name = qtTrId("dummy_unknown");
    house.description = qtTrId("dummy_unknown");
    house.town = qtTrId("dummy_unknown");
    return house;
}

void setStaticContentInformation(TStaticContentInformation information)
{
    std::vector<RaceType> races = std::move(information.monsters);
    races.insert(races.end(), std::make_move_iterator(information.bosses.begin()), std::make_move_iterator(information.bosses.end()));
    appearances::TAppearancesManager::instance().setRaces(std::move(races));
    setMonsterClassifications(std::move(information.monsterClassifications));
    setAchievements(std::move(information.achievements));
    setHouses(std::move(information.houses));
    addQuests(information.quests);
}

const std::vector<TStaticMonsterClassification>& monsterClassifications() { return s_monsterClassifications; }

std::optional<TStaticMonsterClassification> monsterClassification(const quint8 id)
{
    const auto it = s_monsterClassificationsById.constFind(id);
    if (it == s_monsterClassificationsById.constEnd())
        return std::nullopt;
    return *it;
}

std::optional<QString> monsterClassificationName(const quint8 id)
{
    const auto it = s_monsterClassificationsById.constFind(id);
    if (it == s_monsterClassificationsById.constEnd())
        return std::nullopt;
    return it->name;
}

std::optional<TStaticMonsterClassification> monsterClassificationByName(const QString& name)
{
    const auto it = s_monsterClassificationsByName.constFind(name);
    if (it == s_monsterClassificationsByName.constEnd())
        return std::nullopt;
    return *it;
}

std::optional<quint8> monsterClassificationId(const QString& name)
{
    const auto it = s_monsterClassificationsByName.constFind(name);
    if (it == s_monsterClassificationsByName.constEnd())
        return std::nullopt;
    return it->id;
}

const std::vector<TStaticAchievement>& staticAchievements() { return s_achievements; }

TStaticAchievement staticAchievement(const quint16 id)
{
    const auto it = std::lower_bound(s_achievements.begin(), s_achievements.end(), id,
                                     [](const TStaticAchievement& entry, const quint16 key) { return entry.id < key; });
    if (it != s_achievements.end() && it->id == id)
        return *it;
    return { id, 1, qtTrId("dummy_unknown"), qtTrId("dummy_unknown") };
}

const std::vector<TStaticHouse>& staticHouses() { return s_houses; }

TStaticHouse staticHouse(const quint32 id)
{
    const auto it = std::lower_bound(s_houses.begin(), s_houses.end(), id,
                                     [](const TStaticHouse& entry, const quint32 key) { return entry.id < key; });
    if (it != s_houses.end() && it->id == id)
        return *it;
    return TStaticHouse::unknown(id);
}

QString questName(const int id)
{
    const auto it = s_questNames.constFind(id);
    if (it == s_questNames.constEnd())
        throw std::out_of_range("unordered_map::at: key not found");
    return *it;
}

bool hasQuest(const int id) { return s_questNames.contains(id); }

std::vector<TStaticQuest> staticQuests()
{
    std::vector<TStaticQuest> quests;
    quests.reserve(size_t(s_questNames.size()));
    for (auto it = s_questNames.constBegin(); it != s_questNames.constEnd(); ++it)
        quests.push_back({ it.key(), it.value() });
    std::sort(quests.begin(), quests.end(), [](const TStaticQuest& a, const TStaticQuest& b) { return a.id < b.id; });
    return quests;
}

}
