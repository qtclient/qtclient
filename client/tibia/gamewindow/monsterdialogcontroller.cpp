#include "monsterdialogcontroller.h"
#include "colorhelper.h"

#include "analysers/analyserstorages.h"
#include "quickloot/quicklootblackwhiteliststorage.h"

#include <utility>
#include "gamewindow/smoothfiltering.h"

#include "cyclopedia/bestiarystorage.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "skillwheel/skillwheeldata.h"

#include <client/const.h>
#include <tibia/game/gamesession.h>
#include <client/outfit.h>

#include <QLocale>
#include <QQuickWindow>
#include <QSGRendererInterface>

#include <algorithm>
#include <cmath>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using cyclopedia::TBestiaryStorage;
using cyclopedia::TCharm;

/// CIP: 15 classifications / races a page .
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
    const QColor c = tibia::shared::outfitColor(index);
    return QColor(c.red(), c.green(), c.blue());
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

/// CIP : the damage type names.
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
    }

    /// The classification's cell of the "monster-classifications" strip.
    QString imageSource() const override
    {
        return QStringLiteral("image://%1/%2").arg(QStringLiteral("monster-classifications")).arg(classificationImageId(m_name));
    }
};

/// The label of a sensitivity value: weak above 100, neutral at 100, strong
/// below, immune at 0 and healing below 0.
QString sensitivityLabel(const short value)
{
    if (value > 100)
        return qtTrId("monsters_sensitivity_weak");
    if (value == 100)
        return qtTrId("monsters_sensitivity_neutral");
    if (value > 0)
        return qtTrId("monsters_sensitivity_strong");
    if (value == 0)
        return qtTrId("monsters_sensitivity_immune");
    return qtTrId("monsters_sensitivity_healing");
}

/// CIP TQmlMonsterSensitivity: an element's damage percentage, unknown until
/// the race's third stage.
class TQmlMonsterSensitivity final : public cip::tibia::gamewindow::TQmlMonsterSensitivity
{
public:
    /// An unknown sensitivity.
    explicit TQmlMonsterSensitivity(const int element)
        : m_element(element)
    {
        m_elementName = damageTypeName(element);
    }
    TQmlMonsterSensitivity(const int element, const short value)
        : m_element(element)
        , m_value(value)
        , m_known(true)
    {
        m_elementName = damageTypeName(element);
    }

    /// The bar's fill: 0..1/3 for healing, 1/3..2/3 up to neutral, 2/3..1
    /// for weaknesses up to 200%.
    double fillPercentage() const override
    {
        if (!m_known)
            return 0.0;
        const short v = std::clamp<short>(m_value, -200, 200);
        if (v == 100)
            return 0.66666;
        if (v == 0)
            return 0.33333;
        if (m_value >= 0) {
            if (m_value < 100)
                return std::abs(static_cast<double>(v)) / 100.0 * 0.33333 + 0.33333;
            return std::abs(static_cast<double>(static_cast<short>(v - 100))) / 100.0 * 0.33334 + 0.66666;
        }
        return std::abs(static_cast<double>(v)) / 200.0 * 0.33333;
    }

    QString readableSensitivity() const override
    {
        if (!m_known)
            return qtTrId("monsters_unknown_data");
        return qtTrId("monsters_sensitivity").arg(damageTypeName(m_element).toLower()).arg(m_value).arg(sensitivityLabel(m_value));
    }

private:
    int m_element = 0;
    short m_value = 0;
    bool m_known = false;
};

/// CIP TQmlLoot: one loot tile; the empty tiles pad a rarity to full rows.
class TQmlLoot final : public cip::tibia::gamewindow::TQmlLoot
{
public:
    /// An empty tile.
    TQmlLoot()
    {
        m_isEmpty = true;
    }
    TQmlLoot(const LootItem& item)
        : m_dropCount(item.amount)
    {
        m_appearanceTypeID = item.itemId;
        m_isSpecialEventOnly = item.specialEvent != 0;
        m_name = qstr(item.name);
    }

    bool isSecretOrUnknown() const override { return m_appearanceTypeID == 0; }

    /// 1: drops one, 2: drops several, otherwise unknown.
    QString dropCountString() const override
    {
        if (m_dropCount == 1)
            return qtTrId("monsters_dropcount_one");
        if (m_dropCount == 2)
            return qtTrId("monsters_dropcount_several");
        return qtTrId("monsters_unknown_data");
    }

private:
    int m_dropCount = 0;
};

/// CIP TQmlLootOfRarity: one rarity's loot, padded to full rows of 15.
class TQmlLootOfRarity final : public cip::tibia::gamewindow::TQmlLootOfRarity
{
public:
    TQmlLootOfRarity(const int rarity, const std::vector<LootItem>& loot)
        : m_rarity(rarity)
    {
        for (const LootItem& item : loot) {
            if (item.diffculty == rarity)
                m_lootItems.append(new TQmlLoot(item));
        }
        while (m_lootItems.size() % PageSize != 0)
            m_lootItems.append(new TQmlLoot());
        m_lootColumns = PageSize;
    }
    ~TQmlLootOfRarity() override { qDeleteAll(m_lootItems); }

    QString rarityString() const override
    {
        static const char* const Names[] = { "monsters_object_rarity_common", "monsters_object_rarity_uncommon",
                                             "monsters_object_rarity_semirare", "monsters_object_rarity_rare",
                                             "monsters_object_rarity_veryrare" };
        return m_rarity >= 0 && m_rarity < 5 ? qtTrId(Names[m_rarity]) : qtTrId("monsters_unknown_data");
    }
    QString rarityTooltip() const override
    {
        static const char* const Tooltips[] = { "monsters_object_rarity_common_tooltip", "monsters_object_rarity_uncommon_tooltip",
                                                "monsters_object_rarity_semirare_tooltip", "monsters_object_rarity_rare_tooltip",
                                                "monsters_object_rarity_veryrare_tooltip" };
        return m_rarity >= 0 && m_rarity < 5 ? qtTrId(Tooltips[m_rarity]) : qtTrId("monsters_unknown_data");
    }
    int lootRows() const override { return static_cast<int>(m_lootItems.size() / PageSize); }

private:
    int m_rarity = 0;
};

/// CIP TQmlPreviewCharm: an assigned charm.
class TQmlPreviewCharm final : public cip::tibia::gamewindow::TQmlPreviewCharm
{
public:
    explicit TQmlPreviewCharm(const TCharm& charm)
        : m_charm(charm)
    {
        m_name = charm.name();
        m_description = charm.description();
        m_unassignCostInGold = static_cast<int>(charm.removeCost);
    }

    /// The grade, 0 outside 1..3.
    cip::tibia::qmlcomponents::enums::ECharmUnlockGrade unlockGrade() const override
    {
        return static_cast<cip::tibia::qmlcomponents::enums::ECharmUnlockGrade>(m_charm.grade >= 1 && m_charm.grade <= 3 ? m_charm.grade : 0);
    }
    QString icon() const override { return cyclopedia::charmImageSource(m_charm.id); }
    int charmID() const override { return m_charm.id; }

private:
    TCharm m_charm;
};

class TQmlAvailableCharmFilter final : public cip::tibia::gamewindow::TQmlAvailableCharmFilter
{
public:
    TQmlAvailableCharmFilter(const QString& name, const int id)
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
                                 const BestiaryMonsterData* details, QList<TCharm> charms, QObject* parent)
    : cip::tibia::gamewindow::TQmlMonsterRace(parent)
    , m_progress(progress)
    , m_animusMasteryBonus(animusMasteryBonus)
    , m_animusMasteryPoints(animusMasteryPoints)
    , m_charms(std::move(charms))
{
    if (details)
        m_details = *details;
    m_raceID = raceId;
    m_selectedCharmID = -1;
    m_indexOfSelectedCharmInCombobox = -1;
    m_selectedCharmSlot = -1;

    // One loot group a rarity with loot.
    if (const BestiaryMonsterData* difficulty = difficultyData()) {
        for (int rarity = 0; rarity < 5; ++rarity) {
            const bool any = std::any_of(difficulty->loot.begin(), difficulty->loot.end(),
                                         [rarity](const LootItem& item) { return item.diffculty == rarity; });
            if (any)
                m_monsterLoot.append(new TQmlLootOfRarity(rarity, difficulty->loot));
        }
    }

    // The elements in the order of the sensitivity table, unknown until the
    // third stage.
    const BestiaryMonsterData* sensitivities = sensitivityData();
    for (const int element : { 0, 2, 1, 6, 3, 5, 4, 7 }) {
        if (!sensitivities) {
            m_monsterSensitivities.append(new TQmlMonsterSensitivity(element));
            continue;
        }
        const auto it = sensitivities->combat.find(static_cast<uint8_t>(element));
        if (it == sensitivities->combat.end())
            m_monsterSensitivities.append(new TQmlMonsterSensitivity(element));
        else
            m_monsterSensitivities.append(new TQmlMonsterSensitivity(element, static_cast<short>(it->second)));
    }

    // The race's charms: the major and the minor one.
    for (const TCharm& charm : m_charms) {
        if (charm.raceId != raceId)
            continue;
        QObject*& slot = charm.isMinor() ? m_minorCharm : m_majorCharm;
        delete slot;
        slot = new TQmlPreviewCharm(charm);
    }
}

TQmlMonsterRace::~TQmlMonsterRace()
{
    qDeleteAll(m_monsterLoot);
    qDeleteAll(m_monsterSensitivities);
    qDeleteAll(m_charmsInCombobox);
    delete m_minorCharm;
    delete m_majorCharm;
}

const BestiaryMonsterData* TQmlMonsterRace::difficultyData() const { return m_details && m_progress != 0 ? &*m_details : nullptr; }
const BestiaryMonsterData* TQmlMonsterRace::statsData() const { return m_details && m_progress > 1 ? &*m_details : nullptr; }
const BestiaryMonsterData* TQmlMonsterRace::sensitivityData() const { return m_details && m_progress > 2 ? &*m_details : nullptr; }

QString TQmlMonsterRace::name() const { return qstr(tibia::game::raceData(static_cast<int>(m_raceID)).name); }

bool TQmlMonsterRace::hasAppearanceInstance() const { return tibia::game::raceData(static_cast<int>(m_raceID)).outfit.getId() != 0; }

bool TQmlMonsterRace::graphicEffectsSupported() const { return QQuickWindow::graphicsApi() != QSGRendererInterface::Software; }

QString TQmlMonsterRace::unlockState() const
{
    if (m_progress == 0)
        return qtTrId("monsters_unknown_data");
    return qtTrId("monsters_unlock_state").arg(QString::number(m_progress - 1)).arg(QString::number(3)).arg(QString());
}

bool TQmlMonsterRace::isUnknown() const { return m_progress == 0; }
bool TQmlMonsterRace::hasDetails1() const { return m_progress > 1; }
bool TQmlMonsterRace::isFullyUnlocked() const { return m_progress == 4; }
bool TQmlMonsterRace::isAnimusMasteryUnlocked() const { return m_animusMasteryBonus > 0.0; }

QString TQmlMonsterRace::animusMasteryTooltip() const
{
    if (!(m_animusMasteryBonus > 0.0))
        return {};
    return qtTrId("monsters_animus_mastery_unlocked_tooltip")
        .arg(skillwheel::percentNumberTrimmed(m_animusMasteryBonus, 2, 3, false))
        .arg(QLocale(QLocale::English).toString(m_animusMasteryPoints));
}

QString TQmlMonsterRace::leaderKilledTooltip() const
{
    const BestiaryMonsterData* difficulty = isLeaderKilled() ? difficultyData() : nullptr;
    if (!difficulty)
        return {};
    // The charm points an Echo Warden of the difficulty gives.
    int points = 0;
    switch (difficulty->difficulty) {
        case 0: return qtTrId("monsters_leader_killed_singular_tooltip").arg(1);
        case 1: points = 2; break;
        case 2: points = 5; break;
        case 3: points = 10; break;
        case 4: points = 15; break;
        case 5: points = 30; break;
        default: points = 0; break;
    }
    return qtTrId("monsters_leader_killed_tooltip").arg(points);
}

uint TQmlMonsterRace::totalKills() const { return m_details ? m_details->killCounter : 0; }
uint TQmlMonsterRace::details1Kills() const { return m_details ? m_details->thirdDifficulty : 0; }
uint TQmlMonsterRace::details2Kills() const { return m_details ? m_details->secondUnlock : 0; }
uint TQmlMonsterRace::details3Kills() const { return m_details ? m_details->lastProgressKillCount : 0; }
bool TQmlMonsterRace::isLeaderKilled() const { return m_details && m_details->leaderKilled; }

int TQmlMonsterRace::monsterDifficulty() const
{
    const BestiaryMonsterData* difficulty = difficultyData();
    return difficulty ? difficulty->difficulty : 1;
}

QString TQmlMonsterRace::monsterDifficultyString() const
{
    static const char* const Difficulties[] = { "monsters_difficulty_harmless", "monsters_difficulty_trivial", "monsters_difficulty_easy",
                                                "monsters_difficulty_medium",   "monsters_difficulty_hard",    "monsters_difficulty_challenging" };
    const int difficulty = monsterDifficulty();
    if (difficulty < 0 || difficulty > 5)
        return qtTrId("dummy_unknown");
    return qtTrId("monsters_difficulty_tooltip").arg(qtTrId(Difficulties[difficulty]));
}

int TQmlMonsterRace::monsterRarity() const
{
    const BestiaryMonsterData* difficulty = difficultyData();
    return difficulty ? difficulty->ocorrence : 0;
}

QString TQmlMonsterRace::monsterRarityString() const
{
    static const char* const Rarities[] = { "monsters_rarity_common", "monsters_rarity_uncommon", "monsters_rarity_rare", "monsters_rarity_veryrare" };
    const int rarity = monsterRarity();
    if (rarity < 0 || rarity > 3)
        return qtTrId("dummy_unknown");
    return qtTrId("monsters_rarity_tooltip").arg(qtTrId(Rarities[rarity]));
}

bool TQmlMonsterRace::isVeryRare() const
{
    const BestiaryMonsterData* difficulty = difficultyData();
    return difficulty && difficulty->ocorrence == 3;
}

/// The wire attack mode: 1 melee, 2 ranged, 0 none.
bool TQmlMonsterRace::isMeleeAttacker() const
{
    const BestiaryMonsterData* stats = statsData();
    return stats && stats->attackMode == 1;
}

bool TQmlMonsterRace::isRangedAttacker() const
{
    const BestiaryMonsterData* stats = statsData();
    return stats && stats->attackMode == 2;
}

bool TQmlMonsterRace::isSpellcaster() const
{
    const BestiaryMonsterData* stats = statsData();
    return stats && stats->castsSpells;
}

bool TQmlMonsterRace::doesNotAttack() const
{
    const BestiaryMonsterData* stats = statsData();
    return stats && stats->attackMode == 0;
}

QString TQmlMonsterRace::hitpointsString() const
{
    const BestiaryMonsterData* stats = statsData();
    return stats ? QLocale(QLocale::English).toString(stats->maxHealth) : qtTrId("monsters_unknown_data");
}

QString TQmlMonsterRace::experienceString() const
{
    const BestiaryMonsterData* stats = statsData();
    return stats ? QLocale(QLocale::English).toString(stats->experience) : qtTrId("monsters_unknown_data");
}

QString TQmlMonsterRace::speedString() const
{
    const BestiaryMonsterData* stats = statsData();
    return stats ? QLocale(QLocale::English).toString(stats->speed) : qtTrId("monsters_unknown_data");
}

QString TQmlMonsterRace::armorString() const
{
    const BestiaryMonsterData* stats = statsData();
    return stats ? QLocale(QLocale::English).toString(stats->armor) : qtTrId("monsters_unknown_data");
}

QString TQmlMonsterRace::mitigationString() const
{
    const BestiaryMonsterData* stats = statsData();
    if (!stats)
        return qtTrId("monsters_unknown_data");
    return QStringLiteral("%1%").arg(QLocale(QLocale::English).toString(stats->mitigation, 'f', 2));
}

QString TQmlMonsterRace::bonusPointsOnUnlockString() const
{
    const BestiaryMonsterData* stats = statsData();
    return stats ? QLocale(QLocale::English).toString(stats->charmValue) : qtTrId("monsters_unknown_data");
}

/// The locations joined with ", ", unknown before the third stage.
QString TQmlMonsterRace::monsterLocations() const
{
    const BestiaryMonsterData* sensitivities = sensitivityData();
    if (!sensitivities)
        return qtTrId("monsters_unknown_data");
    QStringList locations = qstr(sensitivities->location).split(u',', Qt::SkipEmptyParts);
    for (QString& location : locations)
        location = location.trimmed();
    if (locations.isEmpty())
        locations.append(qtTrId("monsters_unknown_data"));
    return locations.join(QStringLiteral(", "));
}

uint TQmlMonsterRace::creatureOutfitId() const { return tibia::game::raceData(static_cast<int>(m_raceID)).outfit.getId(); }
QColor TQmlMonsterRace::creatureHeadColor() const { return outfitColor(tibia::game::raceData(static_cast<int>(m_raceID)).outfit.getHead()); }
QColor TQmlMonsterRace::creatureTorsoColor() const { return outfitColor(tibia::game::raceData(static_cast<int>(m_raceID)).outfit.getBody()); }
QColor TQmlMonsterRace::creatureLegsColor() const { return outfitColor(tibia::game::raceData(static_cast<int>(m_raceID)).outfit.getLegs()); }
QColor TQmlMonsterRace::creatureDetailColor() const { return outfitColor(tibia::game::raceData(static_cast<int>(m_raceID)).outfit.getFeet()); }
bool TQmlMonsterRace::creatureFirstAddOn() const { return (tibia::game::raceData(static_cast<int>(m_raceID)).outfit.getAddons() & 1) != 0; }
bool TQmlMonsterRace::creatureSecondAddOn() const { return (tibia::game::raceData(static_cast<int>(m_raceID)).outfit.getAddons() & 2) != 0; }

/// The selected charm's removal cost, unknown without a selection.
QString TQmlMonsterRace::unassignCostString() const
{
    if (m_selectedCharmID != -1) {
        for (const TCharm& charm : m_charms) {
            if (charm.id == m_selectedCharmID)
                return QLocale(QLocale::English).toString(static_cast<qulonglong>(charm.removeCost));
        }
    }
    return qtTrId("monsters_unknown_data");
}

void TQmlMonsterRace::setSelectedCharmID(const int v)
{
    if (m_selectedCharmID == v)
        return;
    m_selectedCharmID = v;
    Q_EMIT selectedCharmChanged();
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

void TQmlMonsterRace::updateCanAfford()
{
    const TCharm* charm = nullptr;
    for (const TCharm& candidate : m_charms) {
        if (m_selectedCharmID >= 0 && candidate.id == m_selectedCharmID)
            charm = &candidate;
    }
    if (!charm)
        return;
    const bool canAfford = charm->removeCost <= TBestiaryStorage::goldBalance();
    if (canAfford == m_canAffordCurrentlySelectedEffectUnassignment)
        return;
    m_canAffordCurrentlySelectedEffectUnassignment = canAfford;
    Q_EMIT canAffordCurrentlySelectedEffectUnassignmentChanged();
}

/// The combo box of a slot (1 major, 2 minor) from the third stage on: the
/// assigned charm, else the free charms fitting the slot (major charms only
/// when fully unlocked), else a hint why there is none.
void TQmlMonsterRace::rebuildCombobox()
{
    const QList<QObject*> previous = m_charmsInCombobox;
    m_charmsInCombobox.clear();
    bool filled = false;
    const int slot = m_selectedCharmSlot;
    const int progress = m_progress;
    if (slot != -1 && (progress == 3 || progress == 4)) {
        const auto* assigned = slot == 1   ? static_cast<TQmlPreviewCharm*>(m_majorCharm)
                               : slot == 2 ? static_cast<TQmlPreviewCharm*>(m_minorCharm)
                                           : nullptr;
        if (assigned) {
            m_charmsInCombobox.append(new TQmlAvailableCharmFilter(assigned->name(), assigned->charmID()));
            filled = true;
        } else {
            const bool majorCharms = slot != 2;
            for (const TCharm& charm : m_charms) {
                const bool minor = charm.isMinor();
                if ((progress == 4 || minor) && charm.raceId == 0 && !(slot == 1 && minor) && (minor || majorCharms))
                    m_charmsInCombobox.append(new TQmlAvailableCharmFilter(charm.name(), charm.id));
            }
            filled = !m_charmsInCombobox.isEmpty();
            if (!filled) {
                const char* text = progress != 4 && slot == 1 ? "monsters_not_fully_unlocked"
                                   : slot == 1                ? "monsters_no_unlocked_major_charms"
                                                              : "monsters_no_unlocked_minor_charms";
                m_charmsInCombobox.append(new TQmlAvailableCharmFilter(qtTrId(text), -1));
            }
        }
    }
    Q_EMIT selectedCharmSlotChanged();
    qDeleteAll(previous);
    if (filled && m_indexOfSelectedCharmInCombobox != 0)
        Q_EMIT indexOfSelectedCharmInComboboxChanged();
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

bool TMonsterDialogController::clientSettingSmoothFiltering() const { return smoothFilteringEnabled(); }

TMonsterDialogController::~TMonsterDialogController()
{
    // CIP: the session goes first, then the classification summary, the race list and the single
    // monster are refreshed from the (now empty) data. The page builders are guarded so nothing new
    // is made for QML while the controller goes away.
    m_tearingDown = true;
    m_hooks.clear();
    m_classifications.clear();
    m_races.clear();
    m_details.reset();
    onClassificationSummaryChanged();
    onMonsterRacesChanged();
    if (QObject* single = std::exchange(m_singleMonster, nullptr)) {
        Q_EMIT qmlSingleMonsterChanged();
        delete single;
    }
}

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
    tibia::game::requestBestiary();
    if (!m_pendingContext)
        return;
    const Context context = *m_pendingContext;
    m_pendingContext.reset();
    m_selectedClassification = context.classification;
    m_search = context.search;
    m_selectedRace = context.raceId;
    if (context.state == SingleMonster && context.raceId != 0)
        tibia::game::requestBestiarySearch(static_cast<uint16_t>(context.raceId));
    else if (context.state == MonsterRaces && !context.search.isEmpty())
        requestMonsterSearch(context.search);
    else if (context.state == MonsterRaces && !context.classification.isEmpty())
        tibia::game::requestBestiaryOverview(context.classification.toStdString());
}

void TMonsterDialogController::hideDialogTab()
{
    m_shown = false;
    // CIP: the charm confirmation goes with the tab.
    if (m_confirmDialog->dialog()->item())
        m_confirmDialog->dialog()->close();
}

void TMonsterDialogController::requestClassificationSelection(const QString& ClassificationID)
{
    m_selectedClassification = ClassificationID;
    m_search.clear();
    tibia::game::requestBestiaryOverview(ClassificationID.toStdString());
}

void TMonsterDialogController::requestMonsterSearch(const QString& SearchString)
{
    m_search = SearchString;
    const QString needle = SearchString.toLower();
    std::vector<uint16_t> raceIds;
    for (const RaceType& race : tibia::game::getAllRaces()) {
        if (race.boss || race.raceId == 0)
            continue;
        if (qstr(race.name).toLower().contains(needle))
            raceIds.push_back(static_cast<uint16_t>(race.raceId));
    }
    tibia::game::requestBestiaryOverview("", true, raceIds);
}

void TMonsterDialogController::requestClassificationPageSwitch(const int NewPage)
{
    // The page's entries are rebuilt; a page out of range, or the current
    // one, leaves the list empty.
    QList<QObject*> previous;
    previous.swap(m_monsterClassificationSummary);
    if (!m_tearingDown && NewPage > 0 && NewPage != m_monsterClassificationCurrentPage && NewPage <= m_monsterClassificationTotalPages) {
        m_monsterClassificationCurrentPage = NewPage;
        const size_t end = std::min(m_classifications.size(), static_cast<size_t>(NewPage * PageSize));
        for (size_t i = static_cast<size_t>((NewPage - 1) * PageSize); i < end; ++i)
            m_monsterClassificationSummary.append(new TQmlMonsterClassificationSummary(m_classifications[i], this));
    }
    Q_EMIT qmlMonsterClassificationSummaryChanged();
    Q_EMIT qmlMonsterClassificationPageChanged();
    deleteAll(previous);
}

void TMonsterDialogController::requestMonsterPageSwitch(const int NewPage)
{
    QList<QObject*> previous;
    previous.swap(m_monsterRaceSummary);
    if (!m_tearingDown && NewPage > 0 && NewPage != m_monsterRacesCurrentPage && NewPage <= m_monsterRacesTotalPages) {
        m_monsterRacesCurrentPage = NewPage;
        const size_t end = std::min(m_races.size(), static_cast<size_t>(NewPage * PageSize));
        for (size_t i = static_cast<size_t>((NewPage - 1) * PageSize); i < end; ++i) {
            const BestiaryOverviewMonsters& race = m_races[i];
            m_monsterRaceSummary.append(new TQmlMonsterRace(race.id, race.currentLevel, race.creatureAnimusMasteryBonus / 1000.0,
                                                            m_animusMasteryPoints, nullptr, {}, this));
        }
    }
    Q_EMIT qmlMonsterRacesPageChanged();
    Q_EMIT qmlMonsterRaceSummaryChanged();
    deleteAll(previous);
}

void TMonsterDialogController::onMonsterRaceClicked(const uint MonsterRaceID)
{
    m_selectedRace = static_cast<int>(MonsterRaceID);
    tibia::game::requestBestiarySearch(static_cast<uint16_t>(MonsterRaceID));
}

void TMonsterDialogController::requestSwitchBackToClassificationSummary() { onClassificationSummaryChanged(); }

void TMonsterDialogController::requestSwitchBackToRaceSummary() { requestClassificationSelection(m_selectedClassification); }

void TMonsterDialogController::requestSwitchToItemInfo(const int AppearanceTypeID)
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    action->parameter = 0;
    action->instance = AppearanceTypeID;
    input::publish(action);
}

void TMonsterDialogController::requestContextMenuForItem(const int AppearanceTypeID)
{
    // CIP: the loot tile's menu adds the item to the loot black/whitelist or removes it.
    TContextMenuController* menu = TContextMenuController::instance();
    analysers::TAnalyserStorages* storages = analysers::TAnalyserStorages::instance();
    quickloot::TQuickLootBlackWhitelistStorage* loot = storages ? storages->lootBlackWhitelist() : nullptr;
    if (!menu || !loot || AppearanceTypeID <= 0)
        return;
    const auto typeId = static_cast<uint16_t>(AppearanceTypeID);
    const bool listed = loot->isListed(typeId);
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId(listed ? "context_remove_from_loot_blackwhitelist" : "context_add_to_loot_blackwhitelist")),
            &TContextMenuEntryItem::triggered, this, [typeId, listed] {
                auto action = input::mutableClone(
                    *input::makeAction(listed ? A::LOOT_REMOVE_FROM_BLACKWHITELIST : A::LOOT_ADD_TO_BLACKWHITELIST));
                action->objectClientId = typeId;
                input::publish(action);
            });
    menu->showCustomMenu();
}

void TMonsterDialogController::requestToggleBestiaryTracking()
{
    if (!m_singleMonster)
        return;
    tibia::game::sendStatusTrackerBestiary(static_cast<uint16_t>(m_selectedRace), !m_storage.isTracked(m_selectedRace));
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
    if (!race)
        return;
    m_confirmCharm = race->selectedCharmID();
    m_confirmAction = 1;
    const TCharm* charm = m_storage.charm(m_confirmCharm);
    m_confirmDialog->showMessage(qtTrId("monsters_confirm_unassign_charm_caption"),
                                 qtTrId("monsters_confirm_unassign_charm")
                                     .arg(charm ? charm->name() : QString())
                                     .arg(englishNumber(charm ? charm->removeCost : 0)));
}

void TMonsterDialogController::assignSelectedCharm(const int SelectedCharmID)
{
    m_confirmCharm = SelectedCharmID;
    m_confirmAction = 0;
    const TCharm* charm = m_storage.charm(SelectedCharmID);
    m_confirmDialog->showMessage(qtTrId("monsters_confirm_assign_charm_caption"),
                                 qtTrId("monsters_confirm_assign_charm").arg(charm ? charm->name() : QString()));
}

void TMonsterDialogController::onConfirmDialogAccepted()
{
    // Crystal's charm actions: 1 assign, 2 remove.
    const uint8_t action = m_confirmAction == 1 ? 2 : 1;
    tibia::game::requestSendBuyCharmRune(action, static_cast<uint8_t>(m_confirmCharm), action == 1 ? static_cast<uint16_t>(m_selectedRace) : 0);
    tibia::game::requestBestiarySearch(static_cast<uint16_t>(m_selectedRace));
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
    // The page of the selected classification, else the first.
    setDialogState(MonsterClassification);
    m_monsterClassificationCurrentPage = 0;
    m_monsterClassificationTotalPages = pageCount(m_classifications.size());
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
    // The page of the selected race, else the first.
    setDialogState(MonsterRaces);
    m_monsterRacesCurrentPage = 0;
    m_monsterRacesTotalPages = pageCount(m_races.size());
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
    if (!m_details || m_tearingDown)
        return;
    QObject* previous = m_singleMonster;
    // The unlocked charms.
    QList<TCharm> charms;
    for (const TCharm& charm : m_storage.charms()) {
        if (charm.grade != 0)
            charms.append(charm);
    }
    auto* race = new TQmlMonsterRace(m_details->id, m_details->currentLevel, m_details->AnimusMasteryBonus / 1000.0,
                                     m_details->AnimusMasteryPoints, &*m_details, std::move(charms), this);
    connect(race, &TQmlMonsterRace::selectedCharmChanged, race, &TQmlMonsterRace::updateCanAfford);
    m_singleMonster = race;
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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseBestiaryRaces", [this](GameObject*, const Args& args) {
        const auto* races = arg<std::vector<CyclopediaBestiaryRace>>(args, 0);
        if (!m_shown || !races)
            return std::any{};
        m_classifications = *races;
        if (m_dialogState == MonsterClassification)
            onClassificationSummaryChanged();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseBestiaryOverview", [this](GameObject*, const Args& args) {
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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onUpdateBestiaryMonsterData", [this](GameObject*, const Args& args) {
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
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this](qint64, qint64, int type) {
        if (type == static_cast<int>(tibia::client::RESOURCE_BANK_BALANCE) || type == static_cast<int>(tibia::client::RESOURCE_GOLD_EQUIPPED)) {
            if (TQmlMonsterRace* race = singleRace())
                race->updateCanAfford();
        }
    });
}

}
