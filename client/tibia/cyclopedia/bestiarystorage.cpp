#include "bestiarystorage.h"

#include <client/const.h>
#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <client/staticdata.h>

#include <algorithm>
#include <tuple>

namespace tibia::cyclopedia {

namespace {

using Args = std::vector<std::any>;

template <class T>
const T* arg(const Args& args, const size_t index)
{
    return index < args.size() ? std::any_cast<T>(&args[index]) : nullptr;
}

constexpr TCharmInfo Charms[] = {
    { 0, 0, "Wound", "Your attacks have a {}% chance to deal physical damage equal to 5% of the target's initial hit points.", { 240, 360, 1200 }, { 500, 1000, 1100 } },
    { 1, 0, "Enflame", "Your attacks have a {}% chance to deal fire damage equal to 5% of the target's initial hit points.", { 400, 600, 2000 }, { 500, 1000, 1100 } },
    { 2, 0, "Poison", "Your attacks have a {}% chance to deal earth damage equal to 5% of the target's initial hit points.", { 240, 360, 1200 }, { 500, 1000, 1100 } },
    { 3, 0, "Freeze", "Your attacks have a {}% chance to deal ice damage equal to 5% of the target's initial hit points.", { 320, 480, 1600 }, { 500, 1000, 1100 } },
    { 4, 0, "Zap", "Your attacks have a {}% chance to deal energy damage equal to 5% of the target's initial hit points.", { 320, 480, 1600 }, { 500, 1000, 1100 } },
    { 5, 0, "Curse", "Your attacks have a {}% chance to deal death damage equal to 5% of the target's initial hit points.", { 360, 540, 1800 }, { 500, 1000, 1100 } },
    { 6, 2, "Cripple", "Your attacks have a {}% chance to paralyse the target for 10 seconds.", { 100, 150, 225 }, { 600, 900, 1200 } },
    { 7, 1, "Parry", "Each time you take damage, you have a {}% chance to reflect it back to the aggressor.", { 400, 600, 2000 }, { 500, 1000, 1100 } },
    { 8, 1, "Dodge", "Grants a {}% chance to dodge an attack.", { 240, 360, 1200 }, { 500, 1000, 1100 } },
    { 9, 2, "Adrenaline Burst", "Each time you're hit, you have a {}% chance to trigger a burst of adrenaline, boosting your speed by 150% for 10 seconds.", { 100, 150, 225 }, { 600, 900, 1200 } },
    { 10, 2, "Numb", "After being attacked, you have a {}% chance to paralyse the aggressor for 10 seconds.", { 100, 150, 225 }, { 600, 900, 1200 } },
    { 11, 2, "Cleanse", "Each time you're hit, you have a {}% chance to cleanse one random negative status effect and gain temporary immunity to it for 11 seconds.", { 100, 150, 225 }, { 600, 900, 1200 } },
    { 12, 2, "Bless", "Blesses you, reducing skill and experience loss by {}% when killed by the chosen creature.", { 100, 150, 225 }, { 600, 900, 1200 } },
    { 13, 2, "Scavenge", "Increases your chance of successfully skinning/dusting a skinnable/dustable creature by {}%.", { 100, 150, 225 }, { 6000, 9000, 12000 } },
    { 14, 2, "Gut", "Gutting the creature yields {}% more creature products.", { 100, 150, 225 }, { 600, 900, 1200 } },
    { 15, 0, "Low Blow", "Adds +{}% critical hit chance.", { 800, 1200, 4000 }, { 400, 800, 900 } },
    { 16, 0, "Divine Wrath", "Your attacks have a {}% chance to deal holy damage equal to 5% of the target's initial hit points.", { 600, 900, 3000 }, { 500, 1000, 1100 } },
    { 17, 2, "Vampiric Embrace", "Increases your current life leech by {}%.", { 100, 150, 225 }, { 160, 240, 320 } },
    { 18, 2, "Void's Call", "Increases your current mana leech by {}%.", { 100, 150, 225 }, { 80, 120, 160 } },
    { 19, 0, "Savage Blow", "Adds +{}% critical extra damage.", { 800, 1200, 4000 }, { 2000, 4000, 4400 } },
    { 20, 2, "Fatal Hold", "Your attacks have a {}% chance to prevent creatures from fleeing due to low health for 30 seconds.", { 100, 150, 225 }, { 3000, 4500, 6000 } },
    { 21, 2, "Void Inversion", "{}% chance to gain mana instead of losing it when taking mana drain damage.", { 100, 150, 225 }, { 2000, 3000, 4000 } },
    { 22, 0, "Carnage", "Killing a monster has {}% chance to deal physical damage equal to 15% of its maximum health to all monsters in a small radius.", { 600, 900, 3000 }, { 1000, 2000, 2200 } },
    { 23, 0, "Overpower", "Your attacks have a {}% chance to deal damage equal to 5% of your maximum health.", { 600, 900, 3000 }, { 500, 1000, 1100 } },
    { 24, 0, "Overflux", "Your attacks have a {}% chance to deal damage equal to 2.5% of your maximum mana.", { 600, 900, 3000 }, { 500, 1000, 1100 } },
};

}

const TCharmInfo* charmInfo(const int charmId)
{
    for (const TCharmInfo& info : Charms) {
        if (info.id == charmId)
            return &info;
    }
    return nullptr;
}

int charmPercent(const int charmId, const int grade)
{
    const TCharmInfo* info = charmInfo(charmId);
    return info && grade >= 1 && grade <= 3 ? info->percent[grade - 1] : 0;
}

int charmCost(const int charmId, const int grade)
{
    const TCharmInfo* info = charmInfo(charmId);
    return info && grade >= 1 && grade <= 3 ? info->cost[grade - 1] : 0;
}

QString charmDescription(const int charmId, const int grade)
{
    const TCharmInfo* info = charmInfo(charmId);
    if (!info)
        return {};
    const int percent = charmPercent(charmId, grade);
    QString value = QString::number(percent / 100);
    if (const int rest = percent % 100)
        value += u'.' + QString::number(rest % 10 == 0 ? rest / 10 : rest);
    return QString::fromUtf8(info->description).replace(QStringLiteral("{}"), value);
}

QString charmImageSource(const int charmId)
{
    return QStringLiteral("image://%1/%2").arg(QStringLiteral("monster-bonus-effects")).arg(charmId);
}

QString TCharm::name() const
{
    const TCharmInfo* info = charmInfo(id);
    return info ? QString::fromUtf8(info->name) : QString();
}

QString TCharm::description() const { return charmDescription(id, grade == 0 ? 1 : grade); }

bool TCharm::isMinor() const
{
    const TCharmInfo* info = charmInfo(id);
    return info && info->kind == 2;
}

int TCharm::nextGradeCost() const { return grade >= 3 ? 0 : charmCost(id, grade + 1); }

TBestiaryStorage::TBestiaryStorage(QObject* parent)
    : QObject(parent)
{
    m_hooks.emplace_back("g_game", "onUpdateBestiaryCharmsData", [this](LuaObject*, const Args& args) {
        const auto* data = arg<BestiaryCharmsData>(args, 0);
        if (!data)
            return std::any{};
        m_charms.clear();
        for (const CharmData& charm : data->charms) {
            if (!charmInfo(charm.id))
                continue;
            TCharm entry;
            entry.id = charm.id;
            entry.grade = std::min<int>(charm.tier, 3);
            entry.raceId = charm.asignedStatus ? charm.raceId : 0;
            entry.removeCost = charm.removeRuneCost;
            m_charms.append(entry);
        }
        m_availableCharmSlots = data->availableCharmSlots;
        m_resetAllCharmsCost = data->resetAllCharmsCost;
        m_charmableRaces = data->finishedMonsters;
        m_hasCharmData = true;
        Q_EMIT charmsChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onParseCyclopediaTracker", [this](LuaObject*, const Args& args) {
        using Entry = std::tuple<uint16_t, uint32_t, uint16_t, uint16_t, uint16_t, uint8_t>;
        if (client::gamecore::argInt(args, 0) != 0)
            return std::any{};
        const auto* entries = arg<std::vector<Entry>>(args, 1);
        if (!entries)
            return std::any{};
        m_trackedRaces.clear();
        for (const Entry& entry : *entries)
            m_trackedRaces.insert(std::get<0>(entry));
        Q_EMIT trackerChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_charms.clear();
        m_charmableRaces.clear();
        m_raceProgress.clear();
        m_trackedRaces.clear();
        m_hasCharmData = false;
        return std::any{};
    });
}

const TCharm* TBestiaryStorage::charm(const int charmId) const
{
    for (const TCharm& entry : m_charms) {
        if (entry.id == charmId)
            return &entry;
    }
    return nullptr;
}

const TCharm* TBestiaryStorage::assignedCharm(const int raceId, const bool minor) const
{
    if (raceId == 0)
        return nullptr;
    for (const TCharm& entry : m_charms) {
        if (entry.raceId == raceId && entry.isMinor() == minor)
            return &entry;
    }
    return nullptr;
}

bool TBestiaryStorage::isRaceOnlyAssignableForMinorCharms(const int raceId) const
{
    const int progress = raceProgress(raceId);
    return progress >= 0 && progress < 4;
}

void TBestiaryStorage::setRaceProgress(const int raceId, const int progress) { m_raceProgress.insert(raceId, progress); }

quint64 TBestiaryStorage::goldBalance()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (!player)
        return 0;
    return player->getResourceBalance(Otc::RESOURCE_BANK_BALANCE) + player->getResourceBalance(Otc::RESOURCE_GOLD_EQUIPPED);
}

}
