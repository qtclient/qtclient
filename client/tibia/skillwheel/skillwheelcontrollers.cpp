#include "skillwheelcontrollers.h"
#include "skillwheel/skillwheelciptypes.h"

#include "input/gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <QGuiApplication>

#include <algorithm>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::skillwheel {

namespace {

using A = input::EGameAction;
using gamewindow::TMessageDialogButton;
using gamewindow::TMessageDialogController;
using Button = TMessageDialogController::Button;
using EManageMode = cip::tibia::gamewindow::TSkillWheelPresetManagmentDialogController::EManageMode;

/// Socketed gem image and vessel bezel for atelier vessel index 0..7.
std::pair<QUrl, QUrl> vesselImages(const TSkillWheelStorage* storage, const int vesselIndex)
{
    const TWheel& wheel = storage->currentWheel();
    const int quality = vesselQualityAt(vesselIndex);
    const std::optional<TGem> gem = wheel.gems[vesselIndex] == NoGem ? std::nullopt : storage->gem(wheel.gems[vesselIndex]);
    // Bezel domain index is visual only (cycles 0..3); resonance gate removed.
    return { gem ? gemImage(*gem, storage->vocationIndex()) : QUrl(),
             socketBezel(vesselIndex % DomainCount, quality + 1, quality, gem.has_value()) };
}

/// The icon of a conviction perk; Revelation Mastery has one per revelation perk.
QUrl mediumPerkImage(const int perk, const std::optional<int> largePerk)
{
    int index = MasteryIncreasePerk + (largePerk ? *largePerk : 0);
    if (perk != MasteryIncreasePerk)
        index = perk;
    return imageUrl("icons-skillwheel-mediumperks", index);
}

QString itemName(const int typeId)
{
    if (typeId <= 0)
        return {};
    const TAppearanceTypePtr& type = tibia::game::thingType(static_cast<uint16_t>(typeId), ThingCategoryItem);
    return type ? QString::fromStdString(type->getName()) : QString();
}

/// CIP .
QUrl modGradeIcon(const int grade) { return QUrl(QStringLiteral("/images/skillwheel/icon-modgrade%1.png").arg(grade + 1)); }

/// CIP "skill_wheel_modinfo_with_grade": "<effect> (<Roman grade>)".
QString modInfo(const QString& effect, const int grade) { return qtTrId("skill_wheel_modinfo_with_grade").arg(effect, romanNumeral(grade + 1)); }

/// CIP: life, mana, capacity and mitigation first, then the other stats.
int statRank(const QString& label)
{
    if (label == basicStatName(10, true))
        return 0;
    if (label == basicStatName(11, true))
        return 1;
    if (label == basicStatName(12, true))
        return 2;
    return label == basicStatName(9, true) ? 3 : 99;
}

QList<TMessageDialogButton*> okButton()
{
    return { new TMessageDialogButton(qtTrId("ok"), Button::OK, QKeySequence(QStringLiteral("Return"))) };
}

QList<TMessageDialogButton*> okCancelButtons()
{
    return { new TMessageDialogButton(qtTrId("ok"), Button::OK, QKeySequence(QStringLiteral("Return"))),
             new TMessageDialogButton(qtTrId("cancel"), Button::CANCEL, QKeySequence(QStringLiteral("Esc"))) };
}

void publish(const A type, const int parameter = 0, const int instance = 0)
{
    auto action = input::mutableClone(*input::makeAction(type));
    action->parameter = parameter;
    action->instance = instance;
    input::publish(action);
}

}

TSkillWheelTileQMLInfo::TSkillWheelTileQMLInfo(const int slot, const double fill, const bool unlocked, const int mediumPerk, const int smallPerk,
                                               const QUrl& image, QObject* parent)
    : cip::TSkillWheelTileQMLInfo(parent)
{
    m_id = static_cast<cip::tibia::qmlcomponents::enums::ESkillGridTile>(slot);
    m_fillPercent = fill;
    m_unlocked = unlocked;
    m_mediumPerkId = mediumPerk;
    m_smallPerkId = smallPerk;
    m_largeImage = image;
}

cip::tibia::qmlcomponents::enums::ESkillGridTile TSkillWheelTileQMLInfo::id() const { return m_id; }
double TSkillWheelTileQMLInfo::fillPercent() const { return m_fillPercent; }
bool TSkillWheelTileQMLInfo::unlocked() const { return m_unlocked; }
int TSkillWheelTileQMLInfo::mediumPerkId() const { return m_mediumPerkId; }
int TSkillWheelTileQMLInfo::smallPerkId() const { return m_smallPerkId; }
QUrl TSkillWheelTileQMLInfo::largeImage() const { return m_largeImage; }

TSkillWheelCornerQMLInfo::TSkillWheelCornerQMLInfo(const int domain, const double fill, const int level, const int largePerk, QObject* parent)
    : cip::TSkillWheelCornerQMLInfo(parent)
{
    m_id = static_cast<cip::tibia::qmlcomponents::enums::EPosition>(domain);
    m_fillPercent = fill;
    m_level = level;
    m_largePerkId = largePerk;
}

cip::tibia::qmlcomponents::enums::EPosition TSkillWheelCornerQMLInfo::id() const { return m_id; }
double TSkillWheelCornerQMLInfo::fillPercent() const { return m_fillPercent; }
int TSkillWheelCornerQMLInfo::level() const { return m_level; }
int TSkillWheelCornerQMLInfo::largePerkId() const { return m_largePerkId; }

TQmlSocketedGem::TQmlSocketedGem(const int gemId, const int modSlotCount, const int vesselQuality, const QUrl& bezel, QObject* parent)
    : cip::tibia::skillwheel::TQmlSocketedGem(parent)
{
    m_gemId = gemId;
    m_modSlotCount = modSlotCount;
    m_vesselQuality = vesselQuality;
    m_socketBezelSource = bezel;
}

// ---------------------------------------------------------------------------
// Wheel page

TSkillWheelPageController::TSkillWheelPageController(TSkillWheelStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TSkillWheelPageController(parent)
    , m_storage(storage)
    , m_small(new TSummaryModel(this))
    , m_large(new TSummaryModel(this))
    , m_medium(new TSummaryModel(this))
    , m_vessels(new TSummaryModel(this))
    , m_full(new TSummaryModel(this))
    , m_presets(new TPresetInfoModel(storage, this))
    , m_selectedTile(new cip::tibia::gamewindow::TTileInformation(this))
    , m_hoveredTile(new cip::tibia::gamewindow::TTileInformation(this))
    , m_selectedLarge(new cip::tibia::gamewindow::TLargePerkInformation(this))
    , m_hoveredLarge(new cip::tibia::gamewindow::TLargePerkInformation(this))
    , m_message(new TMessageDialogController(this))
    , m_export(new TMessageDialogController(this))
    , m_info(new TMessageDialogController(this))
    // CIP literal; app::PortalUrls::installCipUrlHandler maps www.tibia.com to the portal.
    , m_plannerUrl(QStringLiteral("https://www.tibia.com/community/?subtopic=wheelofdestinyplanner&code="))
{
    m_smallPerkSummaryModel = m_small;
    m_largePerkSummaryModel = m_large;
    m_mediumPerkSummaryModel = m_medium;
    m_vesselsSummaryModel = m_vessels;
    m_fullSummaryModel = m_full;
    m_presetsInfoModel = m_presets;
    m_selectedTileInformation = m_selectedTile;
    m_hoveredTileInformation = m_hoveredTile;
    m_selectedLargePerkInformation = m_selectedLarge;
    m_hoveredLargePerkInformation = m_hoveredLarge;
    m_rendererSupportsGraphicEffects = true;

    m_message->setCloseOnButtonClicked(true);
    m_export->setCloseOnButtonClicked(true);
    m_info->setCloseOnButtonClicked(true);
    connect(m_message, &TMessageDialogController::acceptClicked, this, &TSkillWheelPageController::onMessageDialogAcceptClicked);
    connect(m_message, &TMessageDialogController::discardClicked, this, &TSkillWheelPageController::onMessageDialogDiscardClicked);
    connect(m_export, &TMessageDialogController::buttonClicked, this,
            &TSkillWheelPageController::requestCopyExportCodeOrUrlToClipboardClicked);

    connect(m_storage, &TSkillWheelStorage::currentSkillWheelChanged, this, &TSkillWheelPageController::onWheelChanged);
    connect(m_storage, &TSkillWheelStorage::serverSkillWheelChanged, this, &TSkillWheelPageController::onServerSkillWheelChanged);
    connect(m_storage, &TSkillWheelStorage::configurationChangedFromUserInput, this, [this] {
        m_presets->reset();
        Q_EMIT presetDataChanged();
        Q_EMIT dataChanged();
    });
    onWheelChanged();
}

int TSkillWheelPageController::totalModGradesSkillPoints() const
{
    return m_storage->hasData() ? totalModGradePoints(m_storage->vocationIndex()) : 0;
}

/// One tooltip line per consumed promotion scroll: its item name and points.
QString TSkillWheelPageController::bonusPointItemNames() const
{
    QStringList lines;
    for (const auto& [typeId, points] : m_storage->promotionScrolls())
        lines << qtTrId("skill_wheel_skill_promotion_points_bonus_consumed_item_tooltip").arg(itemName(typeId)).arg(points);
    return lines.join(QStringLiteral("<br>"));
}

cip::tibia::qmlcomponents::enums::EVocation TSkillWheelPageController::vocation() const
{
    return static_cast<cip::tibia::qmlcomponents::enums::EVocation>(m_storage->vocation());
}

QString TSkillWheelPageController::currentPresetName() const { return canManagePresets() ? m_currentPresetName : QString(); }

/// The own wheel at a temple, not the preview.
bool TSkillWheelPageController::canManagePresets() const
{
    return m_storage->hasData() && m_storage->isOwnCharacter() && !m_storage->isPreview() && m_storage->changeState() == 1;
}

/// The current wheel is not the selected preset's wheel any more.
bool TSkillWheelPageController::isPresetDirty() const
{
    return canManagePresets() && m_presetWheel && m_storage->currentSkillWheel() != m_presetWheel;
}

/// A new server wheel becomes the current wheel; the own character gets the default preset when
/// it has none, and the selected preset stays selected.
void TSkillWheelPageController::onServerSkillWheelChanged()
{
    const std::optional<TWheel> server = m_storage->serverSkillWheel();
    if (!server)
        return;
    m_storage->setCurrentSkillWheel(*server);
    if (m_storage->isOwnCharacter() && !m_storage->isPreview()) {
        if (m_storage->presets().empty())
            m_storage->addOrReplacePreset(TSkillWheelStorage::DefaultPresetName, *server);
        if (!m_storage->hasPreset(m_currentPresetName))
            m_currentPresetName = m_storage->presets().begin()->first;
        m_presetWheel = m_storage->preset(m_currentPresetName);
        if (m_presetWheel)
            m_presetWheel = m_storage->withInventoryGems(*m_presetWheel);
    }
    Q_EMIT initialDataChanged();
    Q_EMIT presetDataChanged();
    onWheelChanged();
}

/// CIP: the slices and corners, the summaries, the selected and hovered
/// information (cleared without data or selection), then dataChanged.
void TSkillWheelPageController::onWheelChanged()
{
    updateParameters();
    updateSummaries();
    updateInformation();
    Q_EMIT dataChanged();
}

/// CIP: the 36 slices ordered by their size, then the four corners; nothing
/// without wheel data. The old objects go first; dataChanged at the end.
void TSkillWheelPageController::updateParameters()
{
    const QList<QObject*> oldTiles = m_skillParameters;
    const QList<QObject*> oldCorners = m_cornerParameters;
    m_skillParameters.clear();
    m_cornerParameters.clear();
    for (QObject* o : oldTiles)
        o->deleteLater();
    for (QObject* o : oldCorners)
        o->deleteLater();

    if (m_storage->hasData()) {
        const TWheel& wheel = m_storage->currentWheel();
        const int voc = m_storage->vocationIndex();
        std::array<int, SlotCount> slots{};
        for (int slot = 0; slot < SlotCount; ++slot)
            slots[slot] = slot;
        std::sort(slots.begin(), slots.end(), [](const int a, const int b) { return maxPoints(a) < maxPoints(b); });
        for (const int slot : slots) {
            const int points = wheel.points[slot];
            const double fill = std::clamp(static_cast<double>(points) / maxPoints(slot), 0.0, 1.0);
            const bool unlocked = points > 0 || isUnlocked(wheel.points, slot);
            const int medium = mediumPerk(slot, voc);
            const std::optional<int> large =
                isMasteryIncreasePerk(medium) ? std::optional<int>(largePerk(domainOfSlot(slot), voc)) : std::nullopt;
            m_skillParameters.append(new TSkillWheelTileQMLInfo(slot, fill, unlocked, medium, static_cast<int>(smallPerk(slot)),
                                                                mediumPerkImage(medium, large), this));
        }
        for (int d = 0; d < DomainCount; ++d) {
            const int points = revelationPoints(wheel, m_storage->gemInventory(), m_storage->supremeGrades(), d, voc);
            const int stage = revelationStage(points);
            const int next = stage < -1 ? 0 : std::min(stage + 1, 3);
            const double from = stage >= 1 && stage <= 3 ? revelationThreshold(stage) : 0.0;
            const double to = next >= 1 && next <= 3 ? revelationThreshold(next) : 0.0;
            double fill = 1.0;
            if (stage < next)
                fill = std::clamp((points - from) / (to - from), 0.0, 1.0);
            m_cornerParameters.append(new TSkillWheelCornerQMLInfo(d, fill, stage, largePerk(d, voc), this));
        }
    }
    Q_EMIT dataChanged();
}

/// The summaries: the dedication totals, the revelation stages, the conviction
/// perks, the vessels and everything. Without wheel data the first three are empty.
void TSkillWheelPageController::updateSummaries()
{
    const bool hasData = m_storage->hasData();
    const TWheel& wheel = m_storage->currentWheel();
    const int voc = m_storage->vocationIndex();
    const TActiveMods mods = activeMods(wheel, m_storage->gemInventory(), m_storage->basicGrades(), m_storage->supremeGrades(), voc);
    const auto byLabel = [](QList<TSummaryEntry>& list) {
        std::ranges::stable_sort(list, [](const TSummaryEntry& a, const TSummaryEntry& b) { return a.label < b.label; });
    };

    QList<TSummaryEntry> smallEntries;
    for (const ESmallPerk perk : { ESmallPerk::Life, ESmallPerk::Mana, ESmallPerk::Capacity, ESmallPerk::Mitigation }) {
        if (!hasData)
            break;
        TSummaryEntry e;
        e.label = smallPerkName(perk);
        e.effect = formatValue(smallPerkTotal(wheel.points, perk, voc), smallPerkFormat(perk));
        if (perk == ESmallPerk::Mitigation)
            e.additionalInfo = qtTrId("skill_wheel_dialog_mitigation_perk_info");
        smallEntries.append(e);
    }

    QList<TSummaryEntry> large;
    int bonus = 0;
    QList<TSummaryEntry> perks;
    for (int d = 0; d < DomainCount; ++d) {
        const int stage = revelationStage(revelationPoints(wheel, m_storage->gemInventory(), m_storage->supremeGrades(), d, voc));
        bonus += largePerkExtraDamageHealing(stage);
        perks.append({ largePerkName(largePerk(d, voc)), perkLevelName(stage), largePerkDetailedInformation(largePerk(d, voc), stage), QUrl(), false });
    }
    std::ranges::sort(perks, [](const TSummaryEntry& a, const TSummaryEntry& b) { return a.label < b.label; });
    if (hasData) {
        large.append({ qtTrId("damage_and_healing"), QStringLiteral("+%1").arg(englishNumber(bonus)),
                       qtTrId("skill_wheel_dialog_unlock_large_perk_extra_damage_healing_info")
                           .arg(largePerkExtraDamageHealing(1))
                           .arg(largePerkExtraDamageHealing(2))
                           .arg(largePerkExtraDamageHealing(3)),
                       QUrl(), false });
        large.append(perks);
    }

    // The conviction perks: the plain perks, the augmented spells (with their
    // augmentation level icons), the leeches, the resistances under a header and
    // Revelation Mastery per domain, each group sorted by name.
    const auto mediumEntry = [&](const QString& label, const int perk, const bool withIcons) -> TSummaryEntry {
        QStringList lines = mediumPerkDescriptions(perk);
        if (withIcons)
            lines = withAugmentationIcons(lines, perk, mediumPerkCount(wheel.points, perk, voc));
        return { label, mediumPerkValueText(wheel.points, perk, voc), lines.join(QStringLiteral("<br>")), QUrl(), false };
    };
    const auto mediumGroup = [&](const std::initializer_list<int> perks, const bool withIcons) {
        QList<TSummaryEntry> group;
        for (const int perk : perks)
            if (mediumPerkCount(wheel.points, perk, voc) > 0)
                group.append(mediumEntry(mediumPerkName(perk), perk, withIcons));
        byLabel(group);
        return group;
    };
    const QList<TSummaryEntry> perkGroup = mediumGroup({ 7, 8, 9, 15, 16, 17, 28, 34, 35, 23, 38, 39, 40, 37 }, false);
    const QList<TSummaryEntry> augmentedGroup =
        mediumGroup({ 10, 11, 14, 13, 12, 18, 19, 20, 21, 22, 24, 25, 26, 27, 29, 30, 31, 32, 33, 36, 41, 42, 43, 44, 45 }, true);
    QList<TSummaryEntry> leechGroup;
    for (const auto& [perk, label] : { std::pair{ 5, "mana_leech" }, std::pair{ 6, "life_leech" } })
        if (mediumPerkCount(wheel.points, perk, voc) > 0)
            leechGroup.append(mediumEntry(qtTrId(label), perk, false));
    byLabel(leechGroup);
    QList<TSummaryEntry> resistanceGroup;
    static constexpr const char* ResistanceElements[] = { "damagetype_fire", "damagetype_energy", "damagetype_ice", "damagetype_earth" };
    for (int perk = 0; perk < 4; ++perk)
        if (mediumPerkCount(wheel.points, perk, voc) > 0)
            resistanceGroup.append(mediumEntry(qtTrId("subentry_prefix").arg(qtTrId(ResistanceElements[perk])), perk, false));
    if (const int holyDeath = mediumPerkCount(wheel.points, 4, voc); holyDeath > 0)
        for (const char* element : { "damagetype_holy", "damagetype_death" })
            resistanceGroup.append({ qtTrId("subentry_prefix").arg(qtTrId(element)), formatValue(holyDeath, 2),
                                     mediumPerkDescriptions(4).join(QStringLiteral("<br>")), QUrl(), false });
    if (!resistanceGroup.isEmpty()) {
        byLabel(resistanceGroup);
        resistanceGroup.prepend({ qtTrId("skill_wheel_dialog_resistance") + u':', {}, {}, QUrl(), false });
    }
    QList<TSummaryEntry> masteryGroup;
    for (int d = 0; d < DomainCount; ++d) {
        int filled = 0;
        for (const int slot : domainSlots(d))
            if (isMasteryIncreasePerk(mediumPerk(slot, voc)) && wheel.points[slot] >= maxPoints(slot))
                ++filled;
        if (filled > 0)
            masteryGroup.append(mediumEntry(mediumPerkName(MasteryIncreasePerk, largePerk(d, voc)), MasteryIncreasePerk, false));
    }
    byLabel(masteryGroup);
    QList<TSummaryEntry> medium;
    if (hasData) {
        medium = perkGroup;
        medium.append(augmentedGroup);
        medium.append(leechGroup);
        medium.append(resistanceGroup);
        medium.append(masteryGroup);
    }

    // CIP entries (resistances), sorted under a "Resistance:" header.
    const auto resistances = [&](const std::array<double, 9>& values) {
        QList<TSummaryEntry> group;
        for (int stat = 0; stat < 9; ++stat)
            if (std::fabs(values[stat]) > 1e-5)
                group.append({ qtTrId("subentry_prefix").arg(basicStatDamageTypeName(stat)), basicStatValueText(values[stat], stat),
                               basicStatInfo(stat), QUrl(), false });
        if (!group.isEmpty()) {
            byLabel(group);
            group.prepend({ qtTrId("skill_wheel_dialog_resistance") + u':', {}, {}, QUrl(), false });
        }
        return group;
    };
    std::array<double, 9> resistanceValues{};
    for (const auto& [stat, value] : mods.basicStats)
        if (stat < 9)
            resistanceValues[stat] = value;

    // CIP : the radiant vessels' spell augmentations.
    QList<TSummaryEntry> augmentations;
    for (const TActiveMods::TAugmentation& aug : mods.augmentations) {
        QUrl image;
        if (aug.augType == 2 || aug.augType == 3)
            image = QUrl(QStringLiteral("/images/skillwheel/icon-spelldamage.png"));
        else if (aug.augType == 6)
            image = QUrl(QStringLiteral("/images/skillwheel/icon-cooldown-reduction.png"));
        else if (aug.augType == 16)
            image = QUrl(QStringLiteral("/images/skillwheel/icon-crit.png"));
        const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(aug.spellId);
        augmentations.append({ spell ? spell->name : qtTrId("dummy_unknown"), supremeModValueText(aug.value, aug.mod),
                               augmentationDescription(aug.value, aug.spellId, aug.augType, mods.momentum, true, false), image, false });
    }
    // CIP .
    QList<TSummaryEntry> revelations;
    for (const TActiveMods::TRevelation& rev : mods.revelations)
        revelations.append({ supremeModName(rev.mod, true, true), supremeModValueText(rev.bonus, rev.mod),
                             qtTrId("skillwheel_gem_mod_revelation_mastery_info"), QUrl(), false });
    byLabel(revelations);
    const auto momentumEntry = [&](const QString& info) -> TSummaryEntry {
        return { qtTrId("momentum_caption"), QStringLiteral("+%1%").arg(mods.momentum), info, QUrl(), false };
    };

    // CIP: damage and healing, momentum, revelations, augmentations, supreme
    // mods, the stats and the resistances; without wheel data it stays empty.
    QList<TSummaryEntry> vessels;
    if (mods.damageHealing > 0)
        vessels.append({ qtTrId("damage_and_healing"), QStringLiteral("+%1").arg(englishNumber(mods.damageHealing)), {}, QUrl(), false });
    if (mods.momentum > 0.0)
        vessels.append(momentumEntry(qtTrId("skill_wheel_dialog_momentum_mod_grade_bonus_information")));
    vessels.append(revelations);
    QList<TSummaryEntry> sortedAugmentations = augmentations;
    byLabel(sortedAugmentations);
    vessels.append(sortedAugmentations);
    QList<TSummaryEntry> supremes;
    for (const auto& [mod, value] : mods.supremeStats)
        supremes.append({ supremeModName(mod, true, true), supremeModValueText(value, mod), {}, QUrl(), false });
    byLabel(supremes);
    vessels.append(supremes);
    // CIP : life, mana, capacity, mitigation, then by name.
    QList<TSummaryEntry> stats;
    for (const auto& [stat, value] : mods.basicStats)
        if (stat > 8)
            stats.append({ basicStatName(stat, true), basicStatValueText(value, stat), basicStatInfo(stat), QUrl(), false });
    std::ranges::stable_sort(stats, [&](const TSummaryEntry& a, const TSummaryEntry& b) {
        const int ra = statRank(a.label), rb = statRank(b.label);
        return ra != rb ? ra < rb : a.label < b.label;
    });
    vessels.append(stats);
    vessels.append(resistances(resistanceValues));
    if (!hasData)
        vessels.clear();

    // CIP : groups joined by separators .
    QList<TSummaryEntry> full;
    const auto appendGroup = [&full](const QList<TSummaryEntry>& group) {
        if (group.isEmpty())
            return;
        if (!full.isEmpty())
            full.append({ {}, {}, {}, QUrl(), true });
        full.append(group);
    };
    QList<TSummaryEntry> totals;
    if (const int damageHealing = bonus + mods.damageHealing; damageHealing > 0)
        totals.append({ qtTrId("damage_and_healing"), QStringLiteral("+%1").arg(englishNumber(damageHealing)), {}, QUrl(), false });
    if (mods.momentum > 0.0)
        totals.append(momentumEntry({}));
    appendGroup(totals);

    int mitigationPoints = 0;
    for (int slot = 0; slot < SlotCount; ++slot)
        if (smallPerk(slot) == ESmallPerk::Mitigation)
            mitigationPoints += wheel.points[slot];
    const std::pair<int, double> statTotals[] = {
        { 10, static_cast<int>(mods.basicStat(10)) + smallPerkTotal(wheel.points, ESmallPerk::Life, voc) },
        { 11, static_cast<int>(mods.basicStat(11)) + smallPerkTotal(wheel.points, ESmallPerk::Mana, voc) },
        { 12, static_cast<int>(mods.basicStat(12)) + smallPerkTotal(wheel.points, ESmallPerk::Capacity, voc) },
        { 9, mods.basicStat(9) + mitigationValue(mitigationPoints) },
    };
    QList<TSummaryEntry> statGroup;
    for (const auto& [stat, value] : statTotals)
        statGroup.append({ basicStatName(stat, true), basicStatValueText(value, stat), basicStatInfo(stat), QUrl(), false });
    const std::pair<int, double> supremeTotals[] = {
        { 0, mods.supremeStat(0) },
        { 1, mods.supremeStat(1) },
        { 3, mods.supremeStat(3) + static_cast<double>(static_cast<float>(mediumPerkCount(wheel.points, 5, voc)) * 0.25f) },
        { 2, mods.supremeStat(2) + static_cast<double>(static_cast<float>(mediumPerkCount(wheel.points, 6, voc)) * 0.75f) },
    };
    QList<TSummaryEntry> supremeGroup;
    for (const auto& [mod, value] : supremeTotals)
        if (std::fabs(value) > 1e-5)
            supremeGroup.append({ supremeModName(mod, true, true), supremeModValueText(value, mod), {}, QUrl(), false });
    byLabel(supremeGroup);
    statGroup.append(supremeGroup);
    appendGroup(statGroup);

    appendGroup(perkGroup);
    QList<TSummaryEntry> augmentedAll = augmentedGroup;
    augmentedAll.append(augmentations);
    byLabel(augmentedAll);
    appendGroup(augmentedAll);
    QList<TSummaryEntry> revelationGroup = perks;
    revelationGroup.append(revelations);
    appendGroup(revelationGroup);
    appendGroup(resistances(resistanceValues));
    appendGroup(masteryGroup);

    m_small->setEntries(smallEntries);
    m_large->setEntries(large);
    m_medium->setEntries(medium);
    m_vessels->setEntries(vessels);
    m_full->setEntries(full);
}

void TSkillWheelPageController::updateInformation()
{
    fillTile(m_selectedTile, m_selectedSlot);
    fillTile(m_hoveredTile, m_hoveredSlot);
    fillLargePerk(m_selectedLarge, m_selectedLargeDomain);
    fillLargePerk(m_hoveredLarge, m_hoveredLargeDomain);
}

void TSkillWheelPageController::clearTile(cip::tibia::gamewindow::TTileInformation* info)
{
    info->setCurrentSkillPoints(0);
    info->setMaximumSkillPoints(0);
    info->setCanBeDecreased(false);
    info->setCanBeIncreased(false);
    info->setShowGemAtelierButton(false);
    info->setSmallPerkSumInfo(QString());
    info->setSmallPerkHover(QString());
    info->setMediumPerkCount(0);
    info->setMediumPerkUnlocked(false);
    info->setMediumPerkInfo(QString());
    info->setMediumPerkHover(QString());
    info->setMediumPerkAdditional(QString());
    info->setAug1Info(QString());
    info->setAug1InfoHover(QString());
    info->setAug2Info(QString());
    info->setAug2InfoHover(QString());
}

void TSkillWheelPageController::clearLargePerk(cip::tibia::gamewindow::TLargePerkInformation* info)
{
    info->setCurrentSkillPoints(0);
    info->setNextSkillPointThreshold(0);
    info->setCurrentLevel(0);
    info->setCurrentLevelName(QString());
    info->setPerkName(QString());
    info->setShortInformation(QString());
    info->setDetailedInformation(QString());
    info->setLargePerkHover(QString());
}

/// Without wheel data or a slice the information is cleared (CIP).
void TSkillWheelPageController::fillTile(cip::tibia::gamewindow::TTileInformation* info, const int slot) const
{
    if (!m_storage->hasData() || slot < 0 || slot >= SlotCount) {
        clearTile(info);
        return;
    }
    const TWheel& wheel = m_storage->currentWheel();
    const int voc = m_storage->vocationIndex();
    const int points = wheel.points[slot];
    const int max = maxPoints(slot);
    const ESmallPerk smallPerkId = smallPerk(slot);
    const int medium = mediumPerk(slot, voc);
    info->setCurrentSkillPoints(points);
    info->setMaximumSkillPoints(max);
    // The grid alone decides: a full slice can be emptied, and a slice can be
    // filled, when the grid stays valid.
    info->setCanBeDecreased(canDecrease(wheel.points, slot));
    info->setCanBeIncreased(points < max && isUnlocked(wheel.points, slot));
    info->setShowGemAtelierButton(isVesselPerk(medium));
    info->setSmallPerkHover(smallPerkInfo(smallPerkId, voc));
    info->setSmallPerkSumInfo(smallPerkValueText(smallPerkId, points, voc));
    // CIP : an augmented spell shows its two levels (plain and with
    // the level icons as hover), any other perk its first description line.
    const int count = mediumPerkCount(wheel.points, medium, voc);
    const bool augmented = isAugmentedPerk(medium);
    const QStringList lines = mediumPerkDescriptions(medium);
    const QStringList iconLines = withAugmentationIcons(lines, medium, count);
    info->setMediumPerkUnlocked(points == max);
    info->setMediumPerkInfo(mediumPerkEffect(medium, largePerk(domainOfSlot(slot), voc)));
    info->setMediumPerkHover(qtTrId("skill_wheel_dialog_unlock_medium_perk_info") + QStringLiteral("<br><br>")
                             + qtTrId(augmented ? "skill_wheel_dialog_how_augmentatios_unlock_info" : "skill_wheel_dialog_medium_perk_more_than_once_info"));
    info->setMediumPerkCount(count);
    if (augmented && lines.size() == 2) {
        info->setMediumPerkAdditional(QString());
        info->setAug1Info(lines[0]);
        info->setAug1InfoHover(iconLines[0]);
        info->setAug2Info(lines[1]);
        info->setAug2InfoHover(iconLines[1]);
    } else {
        info->setMediumPerkAdditional(iconLines.value(0));
        info->setAug1Info(QString());
        info->setAug1InfoHover(QString());
        info->setAug2Info(QString());
        info->setAug2InfoHover(QString());
    }
}

void TSkillWheelPageController::fillLargePerk(cip::tibia::gamewindow::TLargePerkInformation* info, const int domain) const
{
    if (!m_storage->hasData() || domain < 0 || domain >= DomainCount) {
        clearLargePerk(info);
        return;
    }
    const int voc = m_storage->vocationIndex();
    const int points = revelationPoints(m_storage->currentWheel(), m_storage->gemInventory(), m_storage->supremeGrades(), domain, voc);
    const int stage = revelationStage(points);
    info->setCurrentSkillPoints(points);
    info->setNextSkillPointThreshold(revelationThreshold(std::min(stage + 1, 3)));
    info->setCurrentLevel(stage);
    info->setCurrentLevelName(perkLevelName(stage));
    info->setPerkName(largePerkName(largePerk(domain, voc)));
    info->setShortInformation(largePerkShortInformation(largePerk(domain, voc)));
    info->setDetailedInformation(largePerkDetailedInformation(largePerk(domain, voc), stage));
    info->setLargePerkHover(qtTrId("skill_wheel_dialog_unlock_large_perk_info")
                                .arg(revelationThreshold(1))
                                .arg(revelationThreshold(2))
                                .arg(revelationThreshold(3))
                            + QStringLiteral("<br><br>")
                            + qtTrId("skill_wheel_dialog_unlock_large_perk_extra_damage_healing_info")
                                  .arg(largePerkExtraDamageHealing(1))
                                  .arg(largePerkExtraDamageHealing(2))
                                  .arg(largePerkExtraDamageHealing(3)));
}

int TSkillWheelPageController::stepSize(const int modifiers)
{
    const int base = (modifiers & Qt::ShiftModifier) ? 10 : 1;
    return (modifiers & Qt::ControlModifier) ? base * 100 : base;
}

void TSkillWheelPageController::changePoints(const int slot, const int delta)
{
    if (slot < 0 || slot >= SlotCount || delta == 0)
        return;
    TWheel wheel = m_storage->currentWheel();
    wheel.points[slot] = std::clamp(wheel.points[slot] + delta, 0, maxPoints(slot));
    if (!isValid(wheel.points))
        return;
    m_storage->setCurrentSkillWheel(wheel);
}

/// CIP .
void TSkillWheelPageController::addToSkill(const int GridTileQML, const int KeyboardModifiers)
{
    if (!m_storage->hasData() || !canBeIncreased() || GridTileQML < 0 || GridTileQML >= SlotCount
        || !isUnlocked(m_storage->currentWheel().points, GridTileQML))
        return;
    const int amount = std::min(stepSize(KeyboardModifiers), std::max(0, m_storage->freePoints()));
    changePoints(GridTileQML, amount);
}

/// CIP .
void TSkillWheelPageController::fillSkill(const int GridTileQML)
{
    if (!m_storage->hasData() || !canBeIncreased() || GridTileQML < 0 || GridTileQML >= SlotCount
        || !isUnlocked(m_storage->currentWheel().points, GridTileQML))
        return;
    changePoints(GridTileQML, std::min(maxPoints(GridTileQML), std::max(0, m_storage->freePoints())));
}

/// CIP .
void TSkillWheelPageController::removeFromSkill(const int GridTileQML, const int KeyboardModifiers)
{
    if (!m_storage->hasData() || m_storage->changeState() != 1 || GridTileQML < 0 || GridTileQML >= SlotCount
        || !canDecrease(m_storage->currentWheel().points, GridTileQML))
        return;
    changePoints(GridTileQML, -std::min(stepSize(KeyboardModifiers), m_storage->currentWheel().points[GridTileQML]));
}

/// CIP .
void TSkillWheelPageController::clearSkill(const int GridTileQML)
{
    if (!m_storage->hasData() || m_storage->changeState() != 1 || GridTileQML < 0 || GridTileQML >= SlotCount
        || !canDecrease(m_storage->currentWheel().points, GridTileQML))
        return;
    changePoints(GridTileQML, -m_storage->currentWheel().points[GridTileQML]);
}

/// CIP : fill (any modifier: add one step) while points fit,
/// otherwise empty the slice.
void TSkillWheelPageController::onGridTileRightClicked(const int GridTileQML, const int KeyboardModifiers)
{
    if (!m_storage->hasData() || GridTileQML < 0 || GridTileQML >= SlotCount)
        return;
    const auto& points = m_storage->currentWheel().points;
    const int current = points[GridTileQML];
    const int max = maxPoints(GridTileQML);
    const int free = m_storage->freePoints();
    if (current < max && isUnlocked(points, GridTileQML) && free > 0) {
        if (KeyboardModifiers == 0)
            fillSkill(GridTileQML);
        else
            addToSkill(GridTileQML, KeyboardModifiers);
    } else if (canDecrease(points, GridTileQML) && (current == max || free < 1)) {
        clearSkill(GridTileQML);
    }
}

/// CIP: -1 drops the selection; the information follows (cleared without one).
void TSkillWheelPageController::onGridTileSelected(const int GridTileQML)
{
    m_selectedSlot = GridTileQML >= 0 && GridTileQML < SlotCount ? GridTileQML : -1;
    fillTile(m_selectedTile, m_selectedSlot);
}

void TSkillWheelPageController::onGridTileHovered(const int GridTileQML)
{
    m_hoveredSlot = GridTileQML >= 0 && GridTileQML < SlotCount ? GridTileQML : -1;
    fillTile(m_hoveredTile, m_hoveredSlot);
}

void TSkillWheelPageController::onLargePerkSelected(const int QuarterQML)
{
    m_selectedLargeDomain = QuarterQML == -1 ? -1 : QuarterQML;
    fillLargePerk(m_selectedLarge, m_selectedLargeDomain);
}

void TSkillWheelPageController::onLargePerkHovered(const int QuarterQML)
{
    m_hoveredLargeDomain = QuarterQML == -1 ? -1 : QuarterQML;
    fillLargePerk(m_hoveredLarge, m_hoveredLargeDomain);
}

/// Reset: an empty grid (vessel gems stay).
void TSkillWheelPageController::resetSkillGrid()
{
    if (m_storage->changeState() != 1)
        return;
    TWheel wheel = m_storage->currentWheel();
    wheel.points = {};
    m_storage->setCurrentSkillWheel(wheel);
}

/// CIP : the current wheel's export code as COPY_TRUSTED_TEXT_TO_CLIPBOARD.
void TSkillWheelPageController::requestCopyExportCodeToClipboard()
{
    if (!m_storage->hasData())
        return;
    auto action = input::mutableClone(*input::makeAction(A::COPY_TRUSTED_TEXT_TO_CLIPBOARD));
    action->text = encodeWheelCode(m_storage->currentWheel(), m_storage->gemInventory(), m_storage->vocationIndex());
    input::publish(action);
}

/// CIP: the page's message box in one of its modes, with Yes (OK) and No (Cancel).
void TSkillWheelPageController::showMessage(const EMessageMode mode, const QString& caption, const QString& text)
{
    m_messageMode = mode;
    m_message->setButtons({ new TMessageDialogButton(qtTrId("yes"), Button::OK, QKeySequence(QStringLiteral("Return"))),
                            new TMessageDialogButton(qtTrId("no"), Button::CANCEL, QKeySequence(QStringLiteral("Esc"))) });
    m_message->showMessage(caption, text);
}

/// CIP: without a selected preset nothing is selected. A changed current wheel asks first (mode 3)
/// unless `IgnoreChanges`; otherwise the preset's wheel becomes the current wheel and the preset the
/// selected one.
void TSkillWheelPageController::selectPresetByName(const QString& PresetName, const bool IgnoreChanges)
{
    std::optional<TWheel> wheel = m_storage->preset(PresetName);
    if (wheel)
        wheel = m_storage->withInventoryGems(*wheel);
    if (!m_storage->hasPreset(m_currentPresetName)) {
        if (!m_currentPresetName.isEmpty()) {
            m_currentPresetName.clear();
            Q_EMIT presetDataChanged();
        }
        return;
    }
    if (m_currentPresetName == PresetName) {
        m_presetWheel = wheel;
        m_storage->setCurrentSkillWheel(wheel);
    }
    if (wheel) {
        const bool changed = m_storage->currentSkillWheel() != m_presetWheel;
        if (changed && !IgnoreChanges) {
            m_pendingPreset = PresetName;
            showMessage(SwitchPresetMessage, qtTrId("skill_wheel_warning_closing_without_saving_title"),
                        qtTrId("skill_wheel_warning_switch_preset_without_saving").arg(m_currentPresetName));
            return;
        }
    }
    m_presetWheel = wheel;
    m_storage->setCurrentSkillWheel(wheel);
    if (m_currentPresetName != PresetName) {
        m_currentPresetName = PresetName;
        Q_EMIT presetDataChanged();
    }
    Q_EMIT presetDataChanged();
}

/// CIP: the current wheel into the current preset, then apply.
void TSkillWheelPageController::requestSaveCurrentPreset()
{
    if (const std::optional<TWheel> current = m_storage->currentSkillWheel())
        m_storage->addOrReplacePreset(m_currentPresetName, *current);
    apply();
}

void TSkillWheelPageController::requestDeletePresetClicked()
{
    showMessage(DeletePresetMessage, qtTrId("warning"), qtTrId("skill_wheel_dialog_delete_preset_text").arg(m_currentPresetName));
}

/// CIP: drops the current preset and shows the first one.
void TSkillWheelPageController::deleteCurrentPreset()
{
    m_storage->removePreset(m_currentPresetName);
    const std::map<QString, TWheel> presets = m_storage->presets();
    if (!presets.empty())
        selectPresetByName(presets.begin()->first, true);
}

void TSkillWheelPageController::requestRenamePresetClicked() { Q_EMIT managePresets(static_cast<int>(EManageMode::Rename)); }
void TSkillWheelPageController::requestNewPresetClicked() { Q_EMIT managePresets(static_cast<int>(EManageMode::NewEmptyPreset)); }
void TSkillWheelPageController::requestImportPresetClicked() { Q_EMIT managePresets(static_cast<int>(EManageMode::ImportPreset)); }

/// CIP: asks whether to copy the code or the planner link.
void TSkillWheelPageController::requestExportPresetClicked()
{
    m_export->setButtons({ new TMessageDialogButton(qtTrId("skill_wheel_dialog_copy_code"), Button::OK, QKeySequence()),
                           new TMessageDialogButton(qtTrId("skill_wheel_dialog_copy_url"), Button::USER_DEFINED, QKeySequence()),
                           new TMessageDialogButton(qtTrId("cancel"), Button::CANCEL, QKeySequence(QStringLiteral("Esc"))) });
    m_export->showMessage(qtTrId("copy_to_clipboard"), qtTrId("skill_wheel_dialog_copy_exportstring_to_clipboard"));
}

/// CIP: OK copies the code, the user defined button the planner link with it.
void TSkillWheelPageController::requestCopyExportCodeOrUrlToClipboardClicked(
    const cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier, bool)
{
    if (ButtonIdentifier == Button::OK) {
        requestCopyExportCodeToClipboard();
        return;
    }
    if (ButtonIdentifier != Button::USER_DEFINED || !m_storage->hasData())
        return;
    auto action = input::mutableClone(*input::makeAction(A::COPY_TRUSTED_TEXT_TO_CLIPBOARD));
    action->text = m_plannerUrl + encodeWheelCode(m_storage->currentWheel(), m_storage->gemInventory(), m_storage->vocationIndex());
    input::publish(action);
}

void TSkillWheelPageController::setShowFullSummary(const bool Show)
{
    if (Show == m_showFullSummary)
        return;
    m_showFullSummary = Show;
    Q_EMIT summaryModeChanged();
}

void TSkillWheelPageController::useServerSkillWheelAsCurrent() { m_storage->setCurrentSkillWheel(m_storage->serverSkillWheel()); }

/// CIP: drops every selection and clears their information.
void TSkillWheelPageController::clearSelection()
{
    m_selectedSlot = m_hoveredSlot = -1;
    m_selectedLargeDomain = m_hoveredLargeDomain = -1;
    clearTile(m_selectedTile);
    clearTile(m_hoveredTile);
    clearLargePerk(m_selectedLarge);
    clearLargePerk(m_hoveredLarge);
}

/// CIP: the new preset is shown.
void TSkillWheelPageController::onNewPresetName(const QString& PresetName)
{
    m_presets->reset();
    selectPresetByName(PresetName, true);
}

void TSkillWheelPageController::clearData()
{
    clearSelection();
    closeMessages();
}

void TSkillWheelPageController::closeMessages()
{
    m_messageMode = NoMessage;
    for (TMessageDialogController* box : { m_message, m_export, m_info })
        box->dialog()->close();
}

/// CIP: a changed wheel asks before closing (mode 1), otherwise closeConfirmed.
void TSkillWheelPageController::requestClose()
{
    if (!askToSaveBeforeClose()) {
        Q_EMIT closeConfirmed();
        return;
    }
    showMessage(CloseMessage, qtTrId("skill_wheel_warning_closing_without_saving_title"), qtTrId("skill_wheel_warning_closing_without_saving"));
}

/// CIP: delete (4), save and switch (3), apply and close (1).
void TSkillWheelPageController::onMessageDialogAcceptClicked()
{
    const EMessageMode mode = std::exchange(m_messageMode, NoMessage);
    if (mode == DeletePresetMessage) {
        deleteCurrentPreset();
    } else if (mode == SwitchPresetMessage) {
        savePresetChangesThanSwapPreset();
    } else if (mode == CloseMessage) {
        apply();
        Q_EMIT closeConfirmed();
    }
}

/// CIP: switch without saving (3), close without applying (1).
void TSkillWheelPageController::onMessageDialogDiscardClicked()
{
    const EMessageMode mode = std::exchange(m_messageMode, NoMessage);
    if (mode == SwitchPresetMessage)
        discardPresetChangesThanSwapPreset();
    else if (mode == CloseMessage)
        Q_EMIT closeConfirmed();
}

void TSkillWheelPageController::savePresetChangesThanSwapPreset()
{
    requestSaveCurrentPreset();
    selectPresetByName(m_pendingPreset, true);
}

void TSkillWheelPageController::discardPresetChangesThanSwapPreset()
{
    selectPresetByName(m_pendingPreset, true);
    m_pendingPreset.clear();
}

int TSkillWheelPageController::gemAtelierDomain() const
{
    if (m_selectedSlot < 0 || m_selectedSlot >= SlotCount)
        return 0;
    return domainOfSlot(m_selectedSlot);
}

int TSkillWheelPageController::gemAtelierGemId() const
{
    // Prefer a gem already socketed in any vessel; atelier opens without domain affinity.
    for (int v = 0; v < VesselCount; ++v) {
        const int gemId = m_storage->currentWheel().gems[v];
        if (gemId != NoGem)
            return gemId;
    }
    return NoGem;
}

/// CIP: a valid current wheel goes into the current preset and to the server (SAVE_SKILL_WHEEL);
/// the next open of the own wheel is skipped.
void TSkillWheelPageController::apply()
{
    const std::optional<TWheel> current = m_storage->currentSkillWheel();
    if (!current || !isValid(current->points))
        return;
    Q_EMIT preapareToSkipNextOpenForOwnPlayer();
    m_storage->addOrReplacePreset(m_currentPresetName, *current);
    publish(A::SAVE_SKILL_WHEEL);
}

bool TSkillWheelPageController::askToSaveBeforeClose()
{
    return m_storage->hasData() && m_storage->isOwnCharacter() && !m_storage->isPreview() && m_storage->changeState() != 0
        && m_storage->isCurrentSkillWheelChanged();
}

// ---------------------------------------------------------------------------
// Gem atelier

TGemAtelierPageController::TGemAtelierPageController(TSkillWheelStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TGemAtelierPageController(parent)
    , m_storage(storage)
    , m_list(new TInventoryGemListModel(storage, this))
    , m_filter(new TInventoryGemSortFilterProxyModel(storage, this))
    , m_pages(new TPaginationProxyModel(15, this))
    , m_dismantleConfirm(new gamewindow::TMessageDialogController(this))
{
    m_dismantleConfirm->setCloseOnButtonClicked(true);
    connect(m_dismantleConfirm, &gamewindow::TMessageDialogController::acceptClicked, this, [this] { TSkillWheelProtocolMessageHandler::dismantleGem(m_dismantleGemId); });
    m_filter->setSourceModel(m_list);
    m_pages->setSourceModel(m_filter);
    m_gemInventory = m_pages;
    m_maxRevealedGems = MaxRevealedGems;
    m_revealLesserGemCost = revealCost(0);
    m_revealRegularGemCost = revealCost(1);
    m_revealGreaterGemCost = revealCost(2);
    connect(m_filter, &TInventoryGemSortFilterProxyModel::filterChanged, this, [this] {
        if (m_selectedGemId < 0)
            return;
        const int row = m_list->rowOfGem(m_selectedGemId);
        const QModelIndex filtered = row < 0 ? QModelIndex() : m_filter->mapFromSource(m_list->index(row, 0));
        if (filtered.isValid())
            setSelectedGemIndex(m_pages->showSourceRow(filtered.row()));
    });
    connect(m_storage, &TSkillWheelStorage::gemInventoryChanged, this, &TGemAtelierPageController::onGemsChanged);
    connect(m_storage, &TSkillWheelStorage::currentSkillWheelChanged, this, &TGemAtelierPageController::onWheelChanged);
    for (const auto signal : { &TSkillWheelStorage::unrevealedGemsChanged, &TSkillWheelStorage::lesserFragmentsChanged,
                               &TSkillWheelStorage::greaterFragmentsChanged })
        connect(m_storage, signal, this, &TGemAtelierPageController::onResourcesChanged);
    // The gold the atelier costs is the bank balance and the carried gold.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this, [this](qint64, qint64, const int type) {
        if (type == tibia::client::RESOURCE_BANK_BALANCE || type == tibia::client::RESOURCE_GOLD_EQUIPPED)
            onResourcesChanged();
    });
    updateSockets();
}

void TGemAtelierPageController::onGemsChanged()
{
    m_list->reset();
    updateSockets();
    Q_EMIT gemInventoryChanged();
    Q_EMIT unrevealedGemsChanged();
    if (m_selectedGemId >= 0)
        selectGem(m_selectedGemId, false);
}

void TGemAtelierPageController::onWheelChanged()
{
    m_list->refresh();
    updateSockets();
}

/// CipSoft: eight quality-typed vessels unlocked by Gem Vessel perks.
void TGemAtelierPageController::updateSockets()
{
    const int unlocked = activeVesselCount();
    bool unused = false;
    for (int v = 0; v < VesselCount; ++v) {
        const TWheel& wheel = m_storage->currentWheel();
        const int gemId = wheel.gems[v];
        const std::optional<TGem> gem = gemId == NoGem ? std::nullopt : m_storage->gem(gemId);
        const int quality = vesselQualityAt(v);
        const auto [_, bezel] = vesselImages(m_storage, v);
        QObject* old = m_sockets[v];
        m_sockets[v] = new TQmlSocketedGem(gemId, gem ? modCount(*gem) : 0, quality, bezel, this);
        if (old)
            old->deleteLater();
        if (v < unlocked && gemId == NoGem)
            unused = true;
    }
    m_activeVesselCount = unlocked;
    m_showUnusedVesselsNotification = unused;
    Q_EMIT vesselChanged();
}

int TGemAtelierPageController::activeVesselCount() const
{
    return unlockedVesselCount(m_storage->currentWheel().points, m_storage->vocationIndex());
}

bool TGemAtelierPageController::showUnusedVesselsNotification() const
{
    return m_showUnusedVesselsNotification;
}

QString TGemAtelierPageController::unrevealedLesserGemName() const { return itemName(unrevealedGemTypeId(m_storage->vocationIndex(), 0)); }
QString TGemAtelierPageController::unrevealedRegularGemName() const { return itemName(unrevealedGemTypeId(m_storage->vocationIndex(), 1)); }
QString TGemAtelierPageController::unrevealedGreaterGemName() const { return itemName(unrevealedGemTypeId(m_storage->vocationIndex(), 2)); }
int TGemAtelierPageController::unrevealedLesserGemAppearanceType() const { return unrevealedGemTypeId(m_storage->vocationIndex(), 0); }
int TGemAtelierPageController::unrevealedRegularGemAppearanceType() const { return unrevealedGemTypeId(m_storage->vocationIndex(), 1); }
int TGemAtelierPageController::unrevealedGreaterGemAppearanceType() const { return unrevealedGemTypeId(m_storage->vocationIndex(), 2); }

/// Vessels are part of the edited wheel: socketing changes the current wheel,
/// Apply sends it.
void TGemAtelierPageController::socketOrUnsocketGem(const int GemId, const int VesselIndex)
{
    if (!canBeChanged() || VesselIndex < 0 || VesselIndex >= activeVesselCount())
        return;
    const std::optional<TGem> gem = m_storage->gem(GemId);
    TWheel wheel = m_storage->currentWheel();
    if (wheel.gems[VesselIndex] == GemId) {
        wheel.gems[VesselIndex] = NoGem;
    } else if (gem && gem->quality == vesselQualityAt(VesselIndex)) {
        // Unsocket from any previous vessel first.
        for (int v = 0; v < VesselCount; ++v)
            if (wheel.gems[v] == GemId)
                wheel.gems[v] = NoGem;
        wheel.gems[VesselIndex] = GemId;
    } else {
        return;
    }
    m_storage->setCurrentSkillWheel(wheel);
}

void TGemAtelierPageController::closeMessageDialog()
{
    if (m_dismantleConfirm->dialog()->item())
        m_dismantleConfirm->dialog()->close();
}

void TGemAtelierPageController::dismantleGem(const int GemId)
{
    // CIP: asks first (Yes with Return, No with Escape).
    using Button = gamewindow::TMessageDialogController::Button;
    m_dismantleGemId = GemId;
    m_dismantleConfirm->setButtons({ new gamewindow::TMessageDialogButton(Button::YES, qtTrId("yes"), 1u),
                                     new gamewindow::TMessageDialogButton(Button::NO, qtTrId("no"), 2u) });
    m_dismantleConfirm->showMessage(qtTrId("skill_wheel_atelier_dismantle_confirm_dialog_title"),
                                    qtTrId("skill_wheel_atelier_dismantle_confirm_dialog_text"));
}
void TGemAtelierPageController::revealGem(const int GemQuality) { TSkillWheelProtocolMessageHandler::revealGem(GemQuality); }
void TGemAtelierPageController::reshapeGem(const int GemId, const int ModIdx) { TSkillWheelProtocolMessageHandler::reshapeGem(GemId, ModIdx); }
void TGemAtelierPageController::toggleGemLock(const int GemId) { TSkillWheelProtocolMessageHandler::toggleGemLock(GemId); }
/// CIP: an index beyond the active vessels selects none (-1).
void TGemAtelierPageController::selectVessel(const int Index)
{
    if (Index == m_selectedVesselIndex)
        return;
    m_selectedVesselIndex = Index < activeVesselCount() ? Index : -1;
    Q_EMIT selectedVesselIndexChanged();
}

QString TGemAtelierPageController::getModGradeImageSource(const int ModGrade, const bool IsFilled)
{
    return IsFilled ? modGradeImage(ModGrade) : modGradePotentialImage(ModGrade);
}

QObject* TGemAtelierPageController::socketAt(const int Index)
{
    return Index >= 0 && Index < VesselCount ? m_sockets[Index] : nullptr;
}

int TGemAtelierPageController::reshapeGemCost(const cip::tibia::qmlcomponents::enums::EGemQuality Quality, const bool GoldCost)
{
    return ::tibia::skillwheel::reshapeGemCost(static_cast<int>(Quality), GoldCost);
}

int TGemAtelierPageController::unrevealedGemAppearanceType(const cip::tibia::qmlcomponents::enums::EGemQuality Quality)
{
    return unrevealedGemTypeId(m_storage->vocationIndex(), static_cast<int>(Quality));
}

QVariantMap TGemAtelierPageController::gemDataForId(const int GemId)
{
    const int row = m_list->rowOfGem(GemId);
    if (row < 0)
        return {};
    const QModelIndex index = m_list->index(row, 0);
    QVariantMap out;
    const auto roles = m_list->roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        out.insert(QString::fromUtf8(it.value()), m_list->data(index, it.key()));
    return out;
}

void TGemAtelierPageController::selectGem(const int GemId) { selectGem(GemId, false); }

/// Shows the gem: with `NarrowDownFilters` the filters are cleared if they hide it.
void TGemAtelierPageController::selectGem(const int GemId, const bool NarrowDownFilters)
{
    m_selectedGemId = GemId;
    const std::optional<TGem> gem = m_storage->gem(GemId);
    if (!gem)
        return;
    if (NarrowDownFilters && !m_filter->accepts(*gem)) {
        m_filter->setFilterModName(QString());
        m_filter->setFilterGemQuality(-1);
        m_filter->setShowLockedOnly(false);
        m_filter->setFilterDomainAffinity(-1);
    }
    const int row = m_list->rowOfGem(GemId);
    const QModelIndex filtered = row < 0 ? QModelIndex() : m_filter->mapFromSource(m_list->index(row, 0));
    if (filtered.isValid())
        setSelectedGemIndex(m_pages->showSourceRow(filtered.row()));
    Q_EMIT selectedGemIndexChanged();
}

void TGemAtelierPageController::showDomain(const int /*domain*/, const int gemId)
{
    // Domain affinity removed — open atelier on the gem / first unlocked vessel.
    if (gemId != NoGem) {
        const TWheel& wheel = m_storage->currentWheel();
        for (int v = 0; v < VesselCount; ++v) {
            if (wheel.gems[v] == gemId) {
                selectVessel(v);
                break;
            }
        }
        selectGem(gemId, true);
        return;
    }
    if (activeVesselCount() > 0)
        selectVessel(0);
}

void TGemAtelierPageController::selectModInFragmentWorkshop(const int ModId, const bool IsSupremeMod) { Q_EMIT requestWorkshopMod(!IsSupremeMod, ModId); }

// ---------------------------------------------------------------------------
// Fragment workshop

TFragmentWorkshopPageController::TFragmentWorkshopPageController(TSkillWheelStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TFragmentWorkshopPageController(parent)
    , m_storage(storage)
    , m_list(new TModListModel(storage, this))
    , m_filter(new TModListSortFilterProxyModel(m_list, this))
    , m_pages(new TPaginationProxyModel(30, this))
{
    m_pages->setSourceModel(m_filter);
    m_modList = m_pages;
    m_enhanceCostsFragmentsFirstGrade = fragmentCost(0);
    m_enhanceCostsFragmentsSecondGrade = fragmentCost(1);
    m_enhanceCostsFragmentsThirdGrade = fragmentCost(2);
    m_enhanceGoldCostsBasicFirstGrade = basicGoldCost(0);
    m_enhanceGoldCostsBasicSecondGrade = basicGoldCost(1);
    m_enhanceGoldCostsBasicThirdGrade = basicGoldCost(2);
    m_enhanceGoldCostsSupremeFirstGrade = supremeGoldCost(0);
    m_enhanceGoldCostsSupremeSecondGrade = supremeGoldCost(1);
    m_enhanceGoldCostsSupremeThirdGrade = supremeGoldCost(2);
    connect(m_filter, &TModListSortFilterProxyModel::filterChanged, this, [this] {
        if (m_filter->filterMods() != m_filterMods) {
            m_filterMods = m_filter->filterMods();
            Q_EMIT filterChanged();
        }
    });
    connect(m_storage, &TSkillWheelStorage::modGradesChanged, this, &TFragmentWorkshopPageController::onModsChanged);
    connect(m_storage, &TSkillWheelStorage::modListChanged, this, &TFragmentWorkshopPageController::onModsChanged);
    onModsChanged();
}

int TFragmentWorkshopPageController::bonusBaseStats() const
{
    return ::tibia::skillwheel::bonusBaseStats(m_storage->basicGrades(), m_storage->supremeGrades(), m_storage->vocationIndex());
}

int TFragmentWorkshopPageController::bonusPromotionPoints() const
{
    return ::tibia::skillwheel::bonusPromotionPoints(m_storage->basicGrades(), m_storage->supremeGrades(), m_storage->vocationIndex());
}

void TFragmentWorkshopPageController::setFilterMods(const int v)
{
    m_filter->setFilterMods(v);
    cip::tibia::gamewindow::TFragmentWorkshopPageController::setFilterMods(v);
}

/// New grades: keep the selected mod (CIP keeps the pending selection when a grade filter hides it).
void TFragmentWorkshopPageController::onModsChanged()
{
    const QModelIndex current = m_selectedModIndex;
    const TModEntry* selected = nullptr;
    if (current.isValid()) {
        const QModelIndex source = m_filter->mapToSource(m_pages->mapToSource(current));
        selected = m_list->entry(source.row());
    }
    const bool basic = selected ? selected->isBasic : m_pendingBasic;
    const int mod = selected ? selected->modId : m_pendingMod;
    m_list->reset();
    Q_EMIT modGradesChanged();
    if (mod >= 0)
        selectMod(basic, mod);
}

void TFragmentWorkshopPageController::selectMod(const bool isBasicMod, const int ModID)
{
    m_pendingBasic = isBasicMod;
    m_pendingMod = ModID;
    const int row = m_list->rowOfMod(isBasicMod, ModID);
    if (row < 0)
        return;
    QModelIndex filtered = m_filter->mapFromSource(m_list->index(row, 0));
    if (!filtered.isValid()) {
        m_filter->setFilterModName(QString());
        setFilterMods(0);
        filtered = m_filter->mapFromSource(m_list->index(row, 0));
    }
    if (filtered.isValid())
        setSelectedModIndex(m_pages->showSourceRow(filtered.row()));
}

void TFragmentWorkshopPageController::enhanceModGrade(const bool isBasicMod, const int ModID)
{
    m_pendingBasic = isBasicMod;
    m_pendingMod = ModID;
    TSkillWheelProtocolMessageHandler::enhanceModGrade(isBasicMod ? 0 : 1, ModID);
}

// ---------------------------------------------------------------------------
// Preset management dialog

TSkillWheelPresetManagmentDialogController::TSkillWheelPresetManagmentDialogController(TSkillWheelStorage* storage, const EManageMode mode,
                                                                                       const QString& currentPresetName, QObject* parent)
    : TDialogController(QStringLiteral("SkillWheelPresetManagmentDialog.qml"), parent)
    , m_storage(storage)
{
    m_manageMode = mode;
    m_currentPresetName = currentPresetName;
    m_maxNameLength = 29;
    m_nameIsValid = mode == EManageMode::Rename ? isValidName(m_currentPresetName) : false;
}

bool TSkillWheelPresetManagmentDialogController::isValidName(const QString& name) const
{
    return !name.isEmpty() && (!m_storage->preset(name) || (m_manageMode == EManageMode::Rename && name == m_currentPresetName));
}

void TSkillWheelPresetManagmentDialogController::setNewName(const QString& NewName)
{
    m_name = NewName.trimmed();
    const bool valid = isValidName(m_name);
    if (valid != m_nameIsValid) {
        m_nameIsValid = valid;
        Q_EMIT changed();
    }
}

/// CIP : the trimmed text after the last "&code=" (a planner link).
void TSkillWheelPresetManagmentDialogController::setImportString(const QString& ImportString)
{
    static const QString CodeParameter = QStringLiteral("&code=");
    m_importString = ImportString.trimmed();
    if (const qsizetype at = m_importString.lastIndexOf(CodeParameter); at >= 0)
        m_importString = m_importString.mid(at + CodeParameter.size());
    const QString error = importError(nullptr);
    if (error != m_importStringError) {
        m_importStringError = error;
        Q_EMIT changed();
    }
}

/// CIP : an invalid code or grid, or another vocation's code.
QString TSkillWheelPresetManagmentDialogController::importError(TWheel* wheel) const
{
    const std::optional<TWheelCode> code = decodeWheelCode(m_importString);
    if (!code || !isValidWheelCode(*code))
        return qtTrId("skill_wheel_manage_presets_invalid_import_string");
    if (code->vocationIndex != m_storage->vocationIndex())
        return qtTrId("skill_wheel_manage_presets_vocation_mismatch");
    if (wheel) {
        wheel->vocation = code->vocationIndex + 1;
        wheel->points = code->points;
        for (int v = 0; v < VesselCount; ++v) {
            const TCodeGem& mods = code->gems[v];
            if (mods.basic1 < 0)
                continue;
            const int quality = mods.supreme >= 0 ? 2 : mods.basic2 >= 0 ? 1 : 0;
            if (quality != vesselQualityAt(v))
                continue;
            const auto it = std::ranges::find_if(m_storage->gemInventory(), [&](const TGem& g) {
                return g.quality == quality && g.basic1 == mods.basic1 && (quality < 1 || g.basic2 == mods.basic2)
                    && (quality < 2 || g.supreme == mods.supreme);
            });
            if (it != m_storage->gemInventory().end())
                wheel->gems[v] = it->id;
        }
    }
    return {};
}

/// CIP : rename, new empty, copy or import (: only a
/// code without error), then newPresetName.
void TSkillWheelPresetManagmentDialogController::onOkClicked(const EManageMode ManageMod)
{
    if (!isValidName(m_name))
        return;
    bool done = false;
    switch (ManageMod) {
        case EManageMode::Rename: done = m_name == m_currentPresetName || m_storage->renamePreset(m_currentPresetName, m_name); break;
        case EManageMode::NewEmptyPreset: {
            TWheel empty;
            empty.vocation = m_storage->vocation();
            m_storage->addOrReplacePreset(m_name, empty);
            done = true;
            break;
        }
        case EManageMode::CopyPreset:
            m_storage->addOrReplacePreset(m_name, m_storage->currentWheel());
            done = true;
            break;
        case EManageMode::ImportPreset: {
            TWheel wheel;
            const QString error = importError(&wheel);
            if (!error.isEmpty()) {
                if (error != m_importStringError) {
                    m_importStringError = error;
                    Q_EMIT changed();
                }
                return;
            }
            m_storage->addOrReplacePreset(m_name, wheel);
            done = true;
            break;
        }
    }
    if (done)
        Q_EMIT newPresetName(m_name);
    dialog()->close();
}

void TSkillWheelPresetManagmentDialogController::onCloseClicked() { dialog()->close(); }

// ---------------------------------------------------------------------------
// Dialog

TSkillWheelDialogController::TSkillWheelDialogController(TSkillWheelStorage* storage, QObject* parent)
    : TDialogController(QStringLiteral("SkillWheelDialog.qml"), parent)
    , m_storage(storage)
    , m_page(new TSkillWheelPageController(storage, this))
    , m_atelier(new TGemAtelierPageController(storage, this))
    , m_workshop(new TFragmentWorkshopPageController(storage, this))
    , m_message(new TMessageDialogController(this))
{
    m_wheelPageController = m_page;
    m_gemAtelierPageController = m_atelier;
    m_fragmentWorkshopPageController = m_workshop;
    m_message->setCloseOnButtonClicked(true);
    connect(m_message, &TMessageDialogController::acceptClicked, this, [this] {
        if (m_message->property("offerPreview").toBool())
            requestSkillWheelPreview();
    });
    connect(m_page, &cip::tibia::gamewindow::TSkillWheelPageController::requestShowGemAtelier, this, [this] {
        openGemAtelier(m_page->gemAtelierDomain(), m_page->gemAtelierGemId());
    });
    connect(m_page, &TSkillWheelPageController::managePresets, this, &TSkillWheelDialogController::managePresets);
    connect(m_page, &TSkillWheelPageController::closeConfirmed, this, &TSkillWheelDialogController::closeDialog);
    connect(m_atelier, &TGemAtelierPageController::requestWorkshopMod, this, &TSkillWheelDialogController::openWorkshopMod);
}

TSkillWheelDialogController::~TSkillWheelDialogController() = default;

bool TSkillWheelDialogController::isPremium() const
{
    return tibia::game::localPlayerIsPremium();
}

void TSkillWheelDialogController::showDialog()
{
    if (!dialog()->isShown()) {
        dialog()->show();
        m_dialogTab = NoTabInitialState;
        requestTabSwitch(Wheel);
    }
    Q_EMIT premiumStatusChanged();
}

void TSkillWheelDialogController::closeDialog()
{
    if (m_dialogTab == Wheel)
        m_page->hideDialogTab();
    else if (m_dialogTab == GemAtelier)
        m_atelier->hideDialogTab();
    else if (m_dialogTab == FragmentWorkshop)
        m_workshop->hideDialogTab();
    m_dialogTab = NoTabInitialState;
    Q_EMIT dialogTabChanged();
    m_atelier->closeMessageDialog();
    m_page->clearData();
    if (m_presetDialog)
        m_presetDialog->dialog()->close();
    dialog()->close();
    m_storage->setCurrentSkillWheel(m_storage->serverSkillWheel());
}

/// CIP: the page asks about unsaved changes (closeConfirmed closes).
void TSkillWheelDialogController::requestClose() { m_page->requestClose(); }

void TSkillWheelDialogController::requestTabSwitch(const EDialogTab NewTab)
{
    if (NewTab == m_dialogTab)
        return;
    if ((NewTab == GemAtelier || NewTab == FragmentWorkshop) && (!m_storage->isOwnCharacter() || m_storage->isPreview()))
        return;
    if (m_dialogTab == Wheel)
        m_page->hideDialogTab();
    else if (m_dialogTab == GemAtelier)
        m_atelier->hideDialogTab();
    else if (m_dialogTab == FragmentWorkshop)
        m_workshop->hideDialogTab();
    m_dialogTab = NewTab;
    if (NewTab == Wheel)
        m_page->showDialogTab();
    else if (NewTab == GemAtelier)
        m_atelier->showDialogTab();
    else if (NewTab == FragmentWorkshop)
        m_workshop->showDialogTab();
    Q_EMIT dialogTabChanged();
}

void TSkillWheelDialogController::onOkClicked()
{
    m_page->requestSaveCurrentPreset();
    closeDialog();
}

void TSkillWheelDialogController::onApplyClicked() { m_page->requestSaveCurrentPreset(); }
void TSkillWheelDialogController::onResetClicked() { m_page->resetSkillGrid(); }
void TSkillWheelDialogController::getPremiumClicked() { publish(A::OPEN_STORE, 1, 0); }
void TSkillWheelDialogController::requestCopyExportCodeToClipboarClicked() { m_page->requestCopyExportCodeToClipboard(); }

/// CIP (308 accepted): an empty wheel of the own vocation.
void TSkillWheelDialogController::requestSkillWheelPreview()
{
    if (!tibia::game::hasLocalPlayer())
        return;
    const int vocation = tibia::game::localPlayerVocation() % 10;
    if (vocation < 1 || vocation > VocationCount)
        return;
    m_storage->setPreview(QString::fromStdString(tibia::game::localPlayerName()), vocation);
    m_page->clearData();
    showDialog();
}

void TSkillWheelDialogController::showMessage(const QString& caption, const QString& text, const bool offerPreview)
{
    m_message->setProperty("offerPreview", offerPreview);
    m_message->setButtons(offerPreview ? okCancelButtons() : okButton());
    m_message->showMessage(caption, text);
}

void TSkillWheelDialogController::openGemAtelier(const int domain, const int gemId)
{
    if (!m_storage->isOwnCharacter() || m_storage->isPreview())
        return;
    requestTabSwitch(GemAtelier);
    m_atelier->showDomain(domain, gemId);
}

void TSkillWheelDialogController::openWorkshopMod(const bool isBasic, const int modId)
{
    if (!m_storage->isOwnCharacter() || m_storage->isPreview())
        return;
    requestTabSwitch(FragmentWorkshop);
    m_workshop->selectMod(isBasic, modId);
}

void TSkillWheelDialogController::managePresets(const int mode)
{
    if (!m_page->canManagePresets())
        return;
    if (m_presetDialog) {
        m_presetDialog->dialog()->close();
        m_presetDialog->deleteLater();
    }
    m_presetDialog = new TSkillWheelPresetManagmentDialogController(m_storage, static_cast<EManageMode>(mode), m_page->currentPresetName(), this);
    connect(m_presetDialog, &TSkillWheelPresetManagmentDialogController::newPresetName, m_page, &TSkillWheelPageController::onNewPresetName);
    m_presetDialog->dialog()->show();
}

void TSkillWheelDialogController::clear()
{
    m_message->dialog()->close();
    closeDialog();
}

}
