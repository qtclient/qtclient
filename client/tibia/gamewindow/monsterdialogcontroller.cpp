#include "monsterdialogcontroller.h"

#include "cyclopedia/bestiarystorage.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <client/const.h>
#include <tibia/world/game.h>
#include <client/outfit.h>
#include <tibia/assets/things.h>

#include <QLocale>

#include <algorithm>
#include <cmath>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using cyclopedia::TBestiaryStorage;
using cyclopedia::TCharm;

/// CIP: 15 classifications / races a page (0x1017dbd28).
constexpr int PageSize = 15;

template <class T>
const T* arg(const std::vector<std::any>& args, const size_t index)
{
    return index < args.size() ? std::any_cast<T>(&args[index]) : nullptr;
}

QString qstr(const std::string& text) { return QString::fromStdString(text); }
QString englishNumber(const qlonglong value) { return QLocale(QLocale::English).toString(value); }

QColor outfitColor(const int index)
{
    const Color c = Outfit::getColor(index);
    return QColor(c.r(), c.g(), c.b());
}

int pageCount(const size_t entries) { return static_cast<int>(std::ceil(static_cast<double>(entries) / PageSize)); }

/// The "monster-classifications" image strip's cells (cell 0: unknown).
int classificationImageId(const QString& name)
{
    static const char* const Names[] = { "Amphibic", "Aquatic",  "Bird",        "Construct", "Demon",   "Dragon",           "Elemental",
                                         "Fey",      "Giant",    "Human",       "Humanoid",  "Lycanthrope", "Magical",      "Mammal",
                                         "Plant",    "Reptile",  "Slime",       "Undead",    "Vermin",  "Extra Dimensional", "Inkborn" };
    for (int i = 0; i < static_cast<int>(std::size(Names)); ++i) {
        if (name.compare(QLatin1String(Names[i]), Qt::CaseInsensitive) == 0)
            return i + 1;
    }
    return 0;
}

/// CIP FUN_100a45e80: the damage type names.
QString damageTypeName(const int element)
{
    switch (element) {
        case 1: return qtTrId("damagetype_fire");
        case 2: return qtTrId("damagetype_earth");
        case 3: return qtTrId("damagetype_energy");
        case 4: return qtTrId("damagetype_ice");
        case 5: return qtTrId("damagetype_holy");
        case 6: return qtTrId("damagetype_death");
        case 7: return qtTrId("damagetype_healing");
        default: return qtTrId("damagetype_physical");
    }
}

class TQmlMonsterClassificationSummary final : public cip::tibia::gamewindow::TQmlMonsterClassificationSummary
{
public:
    TQmlMonsterClassificationSummary(const CyclopediaBestiaryRace& race, QObject* parent)
        : cip::tibia::gamewindow::TQmlMonsterClassificationSummary(parent)
    {
        m_name = qstr(race.bestClass);
        m_totalMonsters = race.count;
        m_knownMonsters = race.unlockedCount;
        // CIP 0x1009279a0.
        m_imageSource = QStringLiteral("image://%1/%2").arg(QStringLiteral("monster-classifications")).arg(classificationImageId(m_name));
    }
};

/// CIP: TQmlMonsterSensitivity (getters 0x100cfcf50 / 0x100cfce80 / 0x100cfce60).
class TQmlMonsterSensitivity final : public cip::tibia::gamewindow::TQmlMonsterSensitivity
{
public:
    TQmlMonsterSensitivity(const int element, const std::optional<int> value, QObject* parent)
        : cip::tibia::gamewindow::TQmlMonsterSensitivity(parent)
    {
        m_elementName = damageTypeName(element);
        if (!value) {
            m_readableSensitivity = qtTrId("monsters_unknown_data");
            return;
        }
        const int v = std::clamp(*value, -200, 200);
        if (v == 100)
            m_fillPercentage = 0.66666;
        else if (v == 0)
            m_fillPercentage = 0.33333;
        else if (v > 0 && v < 100)
            m_fillPercentage = v / 100.0 * 0.33333 + 0.33333;
        else if (v > 100)
            m_fillPercentage = (v - 100) / 100.0 * 0.33334 + 0.66666;
        else
            m_fillPercentage = std::abs(v) / 200.0 * 0.33333;
        const char* label = *value > 100   ? "monsters_sensitivity_weak"
                            : *value == 100 ? "monsters_sensitivity_neutral"
                            : *value > 0    ? "monsters_sensitivity_strong"
                            : *value == 0   ? "monsters_sensitivity_immune"
                                            : "monsters_sensitivity_healing";
        m_readableSensitivity = qtTrId("monsters_sensitivity").arg(m_elementName.toLower()).arg(*value).arg(qtTrId(label));
    }
};

/// CIP: TQmlLoot (built in 0x100cfc7f0, getters 0x100cfc730..0x100cfc7c0).
class TQmlLoot final : public cip::tibia::gamewindow::TQmlLoot
{
public:
    explicit TQmlLoot(QObject* parent)
        : cip::tibia::gamewindow::TQmlLoot(parent)
    {
        m_isEmpty = true;
    }
    TQmlLoot(const LootItem& item, QObject* parent)
        : cip::tibia::gamewindow::TQmlLoot(parent)
    {
        m_appearanceTypeID = item.itemId;
        m_isSecretOrUnknown = item.itemId == 0;
        m_isSpecialEventOnly = item.specialEvent != 0;
        m_name = qstr(item.name);
        m_dropCountString = item.amount == 1   ? qtTrId("monsters_dropcount_one")
                            : item.amount == 2 ? qtTrId("monsters_dropcount_several")
                                               : qtTrId("monsters_unknown_data");
    }
};

/// CIP: TQmlLootOfRarity (ctor 0x100cfc7f0): one rarity's loot, padded to
/// full rows of 15.
class TQmlLootOfRarity final : public cip::tibia::gamewindow::TQmlLootOfRarity
{
public:
    TQmlLootOfRarity(const int rarity, const std::vector<LootItem>& loot, QObject* parent)
        : cip::tibia::gamewindow::TQmlLootOfRarity(parent)
    {
        static const char* const Names[] = { "monsters_object_rarity_common", "monsters_object_rarity_uncommon",
                                             "monsters_object_rarity_semirare", "monsters_object_rarity_rare",
                                             "monsters_object_rarity_veryrare" };
        static const char* const Tooltips[] = { "monsters_object_rarity_common_tooltip", "monsters_object_rarity_uncommon_tooltip",
                                                "monsters_object_rarity_semirare_tooltip", "monsters_object_rarity_rare_tooltip",
                                                "monsters_object_rarity_veryrare_tooltip" };
        m_rarityString = qtTrId(Names[rarity]);
        m_rarityTooltip = qtTrId(Tooltips[rarity]);
        for (const LootItem& item : loot) {
            if (item.diffculty == rarity)
                m_lootItems.append(new TQmlLoot(item, this));
        }
        while (m_lootItems.size() % PageSize != 0)
            m_lootItems.append(new TQmlLoot(this));
        m_lootRows = static_cast<int>(m_lootItems.size() / PageSize);
        m_lootColumns = PageSize;
    }
};

class TQmlPreviewCharm final : public cip::tibia::gamewindow::TQmlPreviewCharm
{
public:
    TQmlPreviewCharm(const TCharm& charm, QObject* parent)
        : cip::tibia::gamewindow::TQmlPreviewCharm(parent)
    {
        m_name = charm.name();
        m_description = charm.description();
        m_unlockGrade = static_cast<cip::tibia::qmlcomponents::enums::ECharmUnlockGrade>(charm.grade);
        m_icon = cyclopedia::charmImageSource(charm.id);
        m_charmID = charm.id;
        m_unassignCostInGold = static_cast<int>(charm.removeCost);
    }
};

class TQmlAvailableCharmFilter final : public cip::tibia::gamewindow::TQmlAvailableCharmFilter
{
public:
    TQmlAvailableCharmFilter(const QString& name, const int id, QObject* parent)
        : cip::tibia::gamewindow::TQmlAvailableCharmFilter(parent)
    {
        m_name = name;
        m_id = id;
    }
};

void deleteAll(QList<QObject*>& objects)
{
    for (QObject* object : objects)
        object->deleteLater();
    objects.clear();
}

}

TQmlMonsterRace::TQmlMonsterRace(const int raceId, const int progress, const double animusMasteryBonus, const int animusMasteryPoints,
                                 const BestiaryMonsterData* details, const TBestiaryStorage& storage, QObject* parent)
    : cip::tibia::gamewindow::TQmlMonsterRace(parent)
    , m_storage(storage)
    , m_progress(progress)
{
    const RaceType& race = g_things.getRaceData(raceId);
    m_raceID = raceId;
    m_name = qstr(race.name);
    m_hasAppearanceInstance = race.outfit.getId() != 0;
    m_graphicEffectsSupported = true;
    m_creatureOutfitId = race.outfit.getId();
    m_creatureHeadColor = outfitColor(race.outfit.getHead());
    m_creatureTorsoColor = outfitColor(race.outfit.getBody());
    m_creatureLegsColor = outfitColor(race.outfit.getLegs());
    m_creatureDetailColor = outfitColor(race.outfit.getFeet());
    m_creatureFirstAddOn = (race.outfit.getAddons() & 1) != 0;
    m_creatureSecondAddOn = (race.outfit.getAddons() & 2) != 0;

    // CIP 0x100cfa820 .. 0x100cfaac0.
    m_unlockState = progress == 0 ? qtTrId("monsters_unknown_data")
                                  : qtTrId("monsters_unlock_state").arg(progress - 1).arg(3).arg(QString());
    m_isUnknown = progress == 0;
    m_hasDetails1 = progress > 1;
    m_isFullyUnlocked = progress == 4;
    m_isAnimusMasteryUnlocked = animusMasteryBonus > 0.0;
    if (m_isAnimusMasteryUnlocked) {
        const QLocale english(QLocale::English);
        m_animusMasteryTooltip = qtTrId("monsters_animus_mastery_unlocked_tooltip")
                                     .arg(english.toString(animusMasteryBonus * 100.0, 'f', 2))
                                     .arg(english.toString(animusMasteryPoints));
    }

    const bool hasDifficulty = details && progress != 0;
    const bool hasStats = details && progress > 1;
    const bool hasSensitivities = details && progress > 2;

    // CIP 0x100cfad50 / 0x100cfafa0 / 0x100cfb310: the difficulty's charm
    // points for an Echo Warden, the difficulty and the occurrence.
    if (details) {
        m_totalKills = details->killCounter;
        m_details1Kills = details->thirdDifficulty;
        m_details2Kills = details->secondUnlock;
        m_details3Kills = details->lastProgressKillCount;
        m_isLeaderKilled = details->leaderKilled;
    }
    m_monsterDifficulty = hasDifficulty ? details->difficulty : 1;
    m_monsterRarity = hasDifficulty ? details->ocorrence : 0;
    static const char* const Difficulties[] = { "monsters_difficulty_harmless", "monsters_difficulty_trivial", "monsters_difficulty_easy",
                                                "monsters_difficulty_medium",   "monsters_difficulty_hard",    "monsters_difficulty_challenging" };
    static const char* const Rarities[] = { "monsters_rarity_common", "monsters_rarity_uncommon", "monsters_rarity_rare",
                                            "monsters_rarity_veryrare" };
    m_monsterDifficultyString = m_monsterDifficulty >= 0 && m_monsterDifficulty <= 5
                                    ? qtTrId("monsters_difficulty_tooltip").arg(qtTrId(Difficulties[m_monsterDifficulty]))
                                    : qtTrId("dummy_unknown");
    m_monsterRarityString = m_monsterRarity >= 0 && m_monsterRarity <= 3 ? qtTrId("monsters_rarity_tooltip").arg(qtTrId(Rarities[m_monsterRarity]))
                                                                        : qtTrId("dummy_unknown");
    m_isVeryRare = hasDifficulty && m_monsterRarity == 3;
    if (m_isLeaderKilled) {
        static const int LeaderPoints[] = { 1, 2, 5, 10, 15, 30 };
        const int points = m_monsterDifficulty >= 0 && m_monsterDifficulty <= 5 ? LeaderPoints[m_monsterDifficulty] : 0;
        m_leaderKilledTooltip = points == 1 ? qtTrId("monsters_leader_killed_singular_tooltip").arg(points)
                                            : qtTrId("monsters_leader_killed_tooltip").arg(points);
    }

    if (details) {
        for (int rarity = 0; rarity < 5; ++rarity) {
            const bool any = std::any_of(details->loot.begin(), details->loot.end(),
                                         [rarity](const LootItem& item) { return item.diffculty == rarity; });
            if (any)
                m_monsterLoot.append(new TQmlLootOfRarity(rarity, details->loot, this));
        }
    }

    // CIP 0x100cfb5c0..0x100cfb680: the wire attack mode (1 melee, 2 ranged,
    // 0 none) and whether it casts spells.
    if (hasStats) {
        m_isMeleeAttacker = details->attackMode == 1;
        m_isRangedAttacker = details->attackMode == 2;
        m_isSpellcaster = details->castsSpells;
        m_doesNotAttack = details->attackMode == 0;
    }
    const QLocale english(QLocale::English);
    const QString unknown = qtTrId("monsters_unknown_data");
    m_hitpointsString = hasStats ? english.toString(details->maxHealth) : unknown;
    m_experienceString = hasStats ? english.toString(details->experience) : unknown;
    m_speedString = hasStats ? english.toString(details->speed) : unknown;
    m_armorString = hasStats ? english.toString(details->armor) : unknown;
    m_mitigationString = hasStats ? QStringLiteral("%1%").arg(details->mitigation, 0, 'f', 2) : unknown;
    m_bonusPointsOnUnlockString = hasStats ? english.toString(details->charmValue) : unknown;

    // CIP: the elements in the order of table 0x1022de9e8.
    for (const int element : { 0, 2, 1, 6, 3, 5, 4, 7 }) {
        std::optional<int> value;
        if (hasSensitivities) {
            const auto it = details->combat.find(static_cast<uint8_t>(element));
            if (it != details->combat.end())
                value = static_cast<int16_t>(it->second);
        }
        m_monsterSensitivities.append(new TQmlMonsterSensitivity(element, value, this));
    }
    // CIP 0x100cfbb60.
    if (hasSensitivities && !details->location.empty()) {
        QStringList locations = qstr(details->location).split(u',', Qt::SkipEmptyParts);
        for (QString& location : locations)
            location = location.trimmed();
        m_monsterLocations = locations.join(QStringLiteral(", "));
    } else {
        m_monsterLocations = unknown;
    }

    if (const TCharm* major = storage.assignedCharm(raceId, false))
        m_majorCharm = new TQmlPreviewCharm(*major, this);
    if (const TCharm* minor = storage.assignedCharm(raceId, true))
        m_minorCharm = new TQmlPreviewCharm(*minor, this);
    m_selectedCharmID = -1;
    m_selectedCharmSlot = -1;
    m_indexOfSelectedCharmInCombobox = -1;
    updateUnassignCost();
}

void TQmlMonsterRace::setSelectedCharmID(const int v)
{
    if (m_selectedCharmID == v)
        return;
    m_selectedCharmID = v;
    updateUnassignCost();
    Q_EMIT selectedCharmChanged();
    updateCanAfford();
}

void TQmlMonsterRace::setIndexOfSelectedCharmInCombobox(const int v)
{
    if (m_indexOfSelectedCharmInCombobox == v)
        return;
    m_indexOfSelectedCharmInCombobox = v;
    Q_EMIT indexOfSelectedCharmInComboboxChanged();
}

void TQmlMonsterRace::setSelectedCharmSlot(const int v)
{
    if (m_selectedCharmSlot == v)
        return;
    m_selectedCharmSlot = v;
    rebuildCombobox();
    Q_EMIT selectedCharmSlotChanged();
}

void TQmlMonsterRace::updateUnassignCost()
{
    // CIP 0x100cfbdf0.
    const TCharm* charm = m_selectedCharmID >= 0 ? m_storage.charm(m_selectedCharmID) : nullptr;
    m_unassignCostString = charm ? englishNumber(charm->removeCost) : qtTrId("monsters_unknown_data");
}

void TQmlMonsterRace::updateCanAfford()
{
    const TCharm* charm = m_selectedCharmID >= 0 ? m_storage.charm(m_selectedCharmID) : nullptr;
    if (!charm)
        return;
    const bool canAfford = charm->removeCost <= TBestiaryStorage::goldBalance();
    if (canAfford == m_canAffordCurrentlySelectedEffectUnassignment)
        return;
    m_canAffordCurrentlySelectedEffectUnassignment = canAfford;
    Q_EMIT canAffordCurrentlySelectedEffectUnassignmentChanged();
}

void TQmlMonsterRace::rebuildCombobox()
{
    // CIP 0x100cf6b30.
    deleteAll(m_charmsInCombobox);
    const int slot = m_selectedCharmSlot;
    if (slot != -1 && (m_progress == 3 || m_progress == 4)) {
        const auto* assigned = slot == 1 ? qobject_cast<cip::tibia::gamewindow::TQmlPreviewCharm*>(m_majorCharm)
                               : slot == 2 ? qobject_cast<cip::tibia::gamewindow::TQmlPreviewCharm*>(m_minorCharm)
                                           : nullptr;
        if (assigned) {
            m_charmsInCombobox.append(new TQmlAvailableCharmFilter(assigned->name(), assigned->charmID(), this));
        } else {
            for (const TCharm& charm : m_storage.charms()) {
                if (charm.grade == 0 || charm.raceId != 0)
                    continue;
                const bool fits = slot == 1 ? !charm.isMinor() && m_progress == 4 : slot == 2 && charm.isMinor();
                if (fits)
                    m_charmsInCombobox.append(new TQmlAvailableCharmFilter(charm.name(), charm.id, this));
            }
            if (m_charmsInCombobox.isEmpty()) {
                const char* text = m_progress == 3 && slot == 1 ? "monsters_not_fully_unlocked"
                                   : slot == 1                  ? "monsters_no_unlocked_major_charms"
                                                                : "monsters_no_unlocked_minor_charms";
                m_charmsInCombobox.append(new TQmlAvailableCharmFilter(qtTrId(text), -1, this));
            }
        }
    }
    Q_EMIT charmsChanged();
    if (m_indexOfSelectedCharmInCombobox != 0) {
        m_indexOfSelectedCharmInCombobox = 0;
        Q_EMIT indexOfSelectedCharmInComboboxChanged();
    }
}

TMonsterDialogController::TMonsterDialogController(TBestiaryStorage& storage, QObject* parent)
    : cip::tibia::gamewindow::TMonsterDialogController(parent)
    , m_storage(storage)
    , m_confirmDialog(new TMessageDialogController(this))
{
    m_monsterClassificationCurrentPage = 1;
    m_monsterClassificationTotalPages = 1;
    m_monsterRacesCurrentPage = 1;
    m_monsterRacesTotalPages = 1;
    m_confirmDialog->setYesNoButtons();
    m_confirmDialog->setCloseOnButtonClicked(true);
    connect(m_confirmDialog, &TMessageDialogController::acceptClicked, this, &TMonsterDialogController::onConfirmDialogAccepted);
    connect(&m_storage, &TBestiaryStorage::trackerChanged, this, [this] {
        Q_EMIT qmlIsMonsterTrackedChanged();
        Q_EMIT qmlCanTrackMoreMonstersChanged();
    });
    connect(&m_storage, &TBestiaryStorage::charmsChanged, this, [this] {
        if (m_dialogState == SingleMonster)
            rebuildSingleMonster();
    });
    hookEvents();
}

TMonsterDialogController::~TMonsterDialogController() = default;

TQmlMonsterRace* TMonsterDialogController::singleRace() const { return qobject_cast<TQmlMonsterRace*>(m_singleMonster); }

bool TMonsterDialogController::isMonsterTracked() const { return m_singleMonster && m_storage.isTracked(m_selectedRace); }

bool TMonsterDialogController::canTrackMoreMonsters() const { return m_storage.canTrackMore(); }

TMonsterDialogController::Context TMonsterDialogController::captureContext() const
{
    Context context;
    context.state = m_dialogState;
    context.classification = m_selectedClassification;
    context.search = m_search;
    context.raceId = m_selectedRace;
    return context;
}

void TMonsterDialogController::setNextRace(const int raceId)
{
    Context context;
    context.state = SingleMonster;
    context.raceId = raceId;
    m_pendingContext = context;
}

void TMonsterDialogController::showDialogTab()
{
    m_shown = true;
    m_classifications.clear();
    m_races.clear();
    m_details.reset();
    m_selectedClassification.clear();
    m_search.clear();
    m_selectedRace = 0;
    onClassificationSummaryChanged();
    g_game.requestBestiary();
    if (!m_pendingContext)
        return;
    const Context context = *m_pendingContext;
    m_pendingContext.reset();
    m_selectedClassification = context.classification;
    m_search = context.search;
    m_selectedRace = context.raceId;
    if (context.state == SingleMonster && context.raceId != 0)
        g_game.requestBestiarySearch(static_cast<uint16_t>(context.raceId));
    else if (context.state == MonsterRaces && !context.search.isEmpty())
        requestMonsterSearch(context.search);
    else if (context.state == MonsterRaces && !context.classification.isEmpty())
        g_game.requestBestiaryOverview(context.classification.toStdString());
}

void TMonsterDialogController::hideDialogTab() { m_shown = false; }

void TMonsterDialogController::requestClassificationSelection(const QString& ClassificationID)
{
    m_selectedClassification = ClassificationID;
    m_search.clear();
    g_game.requestBestiaryOverview(ClassificationID.toStdString());
}

void TMonsterDialogController::requestMonsterSearch(const QString& SearchString)
{
    m_search = SearchString;
    const QString needle = SearchString.toLower();
    std::vector<uint16_t> raceIds;
    for (const RaceType& race : g_things.getAllRaces()) {
        if (race.boss || race.raceId == 0)
            continue;
        if (qstr(race.name).toLower().contains(needle))
            raceIds.push_back(static_cast<uint16_t>(race.raceId));
    }
    g_game.requestBestiaryOverview("", true, raceIds);
}

void TMonsterDialogController::requestClassificationPageSwitch(const int NewPage)
{
    const int pages = std::max(1, pageCount(m_classifications.size()));
    const int page = std::clamp(NewPage, 1, pages);
    deleteAll(m_monsterClassificationSummary);
    const size_t end = std::min(m_classifications.size(), static_cast<size_t>(page * PageSize));
    for (size_t i = static_cast<size_t>((page - 1) * PageSize); i < end; ++i)
        m_monsterClassificationSummary.append(new TQmlMonsterClassificationSummary(m_classifications[i], this));
    m_monsterClassificationCurrentPage = page;
    m_monsterClassificationTotalPages = pages;
    Q_EMIT qmlMonsterClassificationSummaryChanged();
    Q_EMIT qmlMonsterClassificationPageChanged();
}

void TMonsterDialogController::requestMonsterPageSwitch(const int NewPage)
{
    const int pages = std::max(1, pageCount(m_races.size()));
    const int page = std::clamp(NewPage, 1, pages);
    deleteAll(m_monsterRaceSummary);
    const size_t end = std::min(m_races.size(), static_cast<size_t>(page * PageSize));
    for (size_t i = static_cast<size_t>((page - 1) * PageSize); i < end; ++i) {
        const BestiaryOverviewMonsters& race = m_races[i];
        m_monsterRaceSummary.append(new TQmlMonsterRace(race.id, race.currentLevel, race.creatureAnimusMasteryBonus / 1000.0,
                                                        m_animusMasteryPoints, nullptr, m_storage, this));
    }
    m_monsterRacesCurrentPage = page;
    m_monsterRacesTotalPages = pages;
    Q_EMIT qmlMonsterRaceSummaryChanged();
    Q_EMIT qmlMonsterRacesPageChanged();
}

void TMonsterDialogController::onMonsterRaceClicked(const uint MonsterRaceID)
{
    m_selectedRace = static_cast<int>(MonsterRaceID);
    g_game.requestBestiarySearch(static_cast<uint16_t>(MonsterRaceID));
}

void TMonsterDialogController::requestSwitchBackToClassificationSummary() { onClassificationSummaryChanged(); }

void TMonsterDialogController::requestSwitchBackToRaceSummary()
{
    if (!m_search.isEmpty())
        requestMonsterSearch(m_search);
    else
        g_game.requestBestiaryOverview(m_selectedClassification.toStdString());
}

void TMonsterDialogController::requestSwitchToItemInfo(const int AppearanceTypeID)
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    action->parameter = 0;
    action->instance = AppearanceTypeID;
    input::publish(action);
}

void TMonsterDialogController::requestToggleBestiaryTracking()
{
    if (!m_singleMonster)
        return;
    g_game.sendStatusTrackerBestiary(static_cast<uint16_t>(m_selectedRace), !m_storage.isTracked(m_selectedRace));
}

void TMonsterDialogController::viewSelectedCharm(const int charmID)
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    action->parameter = 3;
    action->category = charmID;
    input::publish(action);
}

void TMonsterDialogController::removeSelectedCharm()
{
    TQmlMonsterRace* race = singleRace();
    const TCharm* charm = race ? m_storage.charm(race->selectedCharmID()) : nullptr;
    if (!charm)
        return;
    m_confirmAction = 1;
    m_confirmCharm = charm->id;
    m_confirmDialog->showMessage(qtTrId("monsters_confirm_unassign_charm_caption"),
                                 qtTrId("monsters_confirm_unassign_charm").arg(charm->name(), englishNumber(charm->removeCost)));
}

void TMonsterDialogController::assignSelectedCharm(const int SelectedCharmID)
{
    if (!singleRace() || SelectedCharmID < 0)
        return;
    m_confirmAction = 0;
    m_confirmCharm = SelectedCharmID;
    const TCharm* charm = m_storage.charm(SelectedCharmID);
    m_confirmDialog->showMessage(qtTrId("monsters_confirm_assign_charm_caption"),
                                 qtTrId("monsters_confirm_assign_charm").arg(charm ? charm->name() : QString()));
}

void TMonsterDialogController::onConfirmDialogAccepted()
{
    // Crystal's charm actions: 1 assign, 2 remove.
    const uint8_t action = m_confirmAction == 1 ? 2 : 1;
    g_game.requestSendBuyCharmRune(action, static_cast<uint8_t>(m_confirmCharm), action == 1 ? static_cast<uint16_t>(m_selectedRace) : 0);
    g_game.requestBestiarySearch(static_cast<uint16_t>(m_selectedRace));
}

void TMonsterDialogController::setDialogState(const DialogState state)
{
    if (m_dialogState == state)
        return;
    m_dialogState = state;
    Q_EMIT qmlDialogStateChanged();
}

void TMonsterDialogController::onClassificationSummaryChanged()
{
    setDialogState(MonsterClassification);
    int page = 1;
    for (size_t i = 0; i < m_classifications.size(); ++i) {
        if (qstr(m_classifications[i].bestClass) == m_selectedClassification) {
            page = static_cast<int>(i / PageSize) + 1;
            break;
        }
    }
    requestClassificationPageSwitch(page);
}

void TMonsterDialogController::onMonsterRacesChanged()
{
    setDialogState(MonsterRaces);
    int page = 1;
    for (size_t i = 0; i < m_races.size(); ++i) {
        if (m_races[i].id == m_selectedRace) {
            page = static_cast<int>(i / PageSize) + 1;
            break;
        }
    }
    requestMonsterPageSwitch(page);
}

void TMonsterDialogController::rebuildSingleMonster()
{
    if (!m_details)
        return;
    QObject* previous = m_singleMonster;
    m_singleMonster = new TQmlMonsterRace(m_details->id, m_details->currentLevel, m_details->AnimusMasteryBonus / 1000.0,
                                          m_details->AnimusMasteryPoints, &*m_details, m_storage, this);
    Q_EMIT qmlSingleMonsterChanged();
    Q_EMIT qmlIsMonsterTrackedChanged();
    if (previous)
        previous->deleteLater();
}

void TMonsterDialogController::onMonsterRaceDetailsChanged()
{
    m_selectedRace = m_details->id;
    m_selectedClassification = qstr(m_details->bestClass);
    rebuildSingleMonster();
    singleRace()->updateCanAfford();
    setDialogState(SingleMonster);
}

void TMonsterDialogController::hookEvents()
{
    m_hooks.emplace_back("g_game", "onParseBestiaryRaces", [this](LuaObject*, const Args& args) {
        const auto* races = arg<std::vector<CyclopediaBestiaryRace>>(args, 0);
        if (!m_shown || !races)
            return std::any{};
        m_classifications = *races;
        if (m_dialogState == MonsterClassification)
            onClassificationSummaryChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onParseBestiaryOverview", [this](LuaObject*, const Args& args) {
        const auto* races = arg<std::vector<BestiaryOverviewMonsters>>(args, 1);
        if (!races)
            return std::any{};
        for (const BestiaryOverviewMonsters& race : *races)
            m_storage.setRaceProgress(race.id, race.currentLevel);
        if (!m_shown)
            return std::any{};
        m_races = *races;
        m_animusMasteryPoints = static_cast<int>(client::gamecore::argInt(args, 2));
        onMonsterRacesChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onUpdateBestiaryMonsterData", [this](LuaObject*, const Args& args) {
        const auto* data = arg<BestiaryMonsterData>(args, 0);
        if (!data)
            return std::any{};
        m_storage.setRaceProgress(data->id, data->currentLevel);
        if (!m_shown)
            return std::any{};
        m_details = *data;
        onMonsterRaceDetailsChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const Args& args) {
        const auto* type = arg<Otc::ResourceTypes_t>(args, 2);
        if (type && (*type == Otc::RESOURCE_BANK_BALANCE || *type == Otc::RESOURCE_GOLD_EQUIPPED)) {
            if (TQmlMonsterRace* race = singleRace())
                race->updateCanAfford();
        }
        return std::any{};
    });
}

}
