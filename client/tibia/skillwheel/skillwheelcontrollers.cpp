#include "skillwheelcontrollers.h"

#include "input/gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QGuiApplication>

#include <algorithm>

namespace tibia::skillwheel {

namespace {

using A = input::EGameAction;
using gamewindow::TMessageDialogButton;
using gamewindow::TMessageDialogController;
using Button = TMessageDialogController::Button;
using EManageMode = cip::tibia::gamewindow::TSkillWheelPresetManagmentDialogController::EManageMode;

/// CIP 0x100f9a1e0 / 0x100f9a290: the socketed gem image and the vessel bezel.
std::pair<QUrl, QUrl> vesselImages(const TSkillWheelStorage* storage, const int domain)
{
    const TWheel& wheel = storage->currentWheel();
    const int res = resonance(wheel.points, domain);
    const TGem* gem = wheel.gems[domain] == NoGem ? nullptr : storage->gem(wheel.gems[domain]);
    return { gem ? gemImage(*gem, storage->vocationIndex()) : QUrl(), socketBezel(domain, res, gem ? gem->quality : 0, gem != nullptr) };
}

QString itemName(const int typeId)
{
    if (typeId <= 0)
        return {};
    const ThingTypePtr& type = g_things.getThingType(static_cast<uint16_t>(typeId), ThingCategoryItem);
    return type ? QString::fromStdString(type->getName()) : QString();
}

/// CIP 0x100927020.
QUrl modGradeIcon(const int grade) { return QUrl(QStringLiteral("/images/skillwheel/icon-modgrade%1.png").arg(grade + 1)); }

/// CIP "skill_wheel_modinfo_with_grade": "<effect> (<Roman grade>)".
QString modInfo(const QString& effect, const int grade) { return qtTrId("skill_wheel_modinfo_with_grade").arg(effect, romanNumeral(grade + 1)); }

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
                                               const QUrl& image, const int modType, QObject* parent)
    : cip::TSkillWheelTileQMLInfo(parent)
{
    m_id = static_cast<cip::tibia::qmlcomponents::enums::ESkillGridTile>(slot);
    m_fillPercent = fill;
    m_unlocked = unlocked;
    m_mediumPerkId = mediumPerk;
    m_smallPerkId = smallPerk;
    m_largeImage = image;
    m_modType = static_cast<cip::tibia::qmlcomponents::enums::EGemModType>(modType);
}

TSkillWheelCornerQMLInfo::TSkillWheelCornerQMLInfo(const int domain, const double fill, const int level, const int largePerk, const QUrl& gem,
                                                   const QUrl& bezel, QObject* parent)
    : cip::TSkillWheelCornerQMLInfo(parent)
{
    m_id = static_cast<cip::tibia::qmlcomponents::enums::EPosition>(domain);
    m_fillPercent = fill;
    m_level = level;
    m_largePerkId = largePerk;
    m_gemImageSource = gem;
    m_socketBezelSource = bezel;
}

TQmlSocketedGem::TQmlSocketedGem(const int gemId, const QUrl& gem, const QUrl& bezel, QObject* parent)
    : cip::tibia::skillwheel::TQmlSocketedGem(parent)
{
    m_gemId = gemId;
    m_gemImageSource = gem;
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
    , m_selectedVessel(new cip::tibia::gamewindow::TVesselInformation(this))
    , m_hoveredVessel(new cip::tibia::gamewindow::TVesselInformation(this))
    , m_switchPreset(new TMessageDialogController(this))
    , m_deletePreset(new TMessageDialogController(this))
    , m_info(new TMessageDialogController(this))
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
    m_selectedVesselInformation = m_selectedVessel;
    m_hoveredVesselInformation = m_hoveredVessel;
    m_rendererSupportsGraphicEffects = true;

    m_switchPreset->setCloseOnButtonClicked(true);
    m_deletePreset->setCloseOnButtonClicked(true);
    m_info->setCloseOnButtonClicked(true);
    connect(m_switchPreset, &TMessageDialogController::acceptClicked, this, [this] {
        m_storage->saveCurrentPreset();
        selectPresetByName(m_pendingPreset, true);
    });
    connect(m_switchPreset, &TMessageDialogController::discardClicked, this, [this] { selectPresetByName(m_pendingPreset, true); });
    connect(m_deletePreset, &TMessageDialogController::acceptClicked, this, [this] {
        if (m_storage->deleteCurrentPreset())
            selectPresetByName(m_storage->currentPresetName(), true);
    });

    connect(m_storage, &TSkillWheelStorage::currentWheelChanged, this, &TSkillWheelPageController::onWheelChanged);
    connect(m_storage, &TSkillWheelStorage::serverDataReceived, this, &TSkillWheelPageController::onInitialData);
    connect(m_storage, &TSkillWheelStorage::presetsChanged, this, [this] {
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

/// CIP 0x100de0c70: one tooltip line per consumed promotion scroll.
QString TSkillWheelPageController::bonusPointItemNames() const
{
    QStringList lines;
    for (const auto& [typeId, points] : m_storage->promotionScrolls())
        lines << qtTrId("skill_wheel_skill_promotion_points_bonus_consumed_item_tooltip").arg(itemName(typeId)).arg(points);
    return lines.join(u'\n');
}

cip::tibia::qmlcomponents::enums::EVocation TSkillWheelPageController::vocation() const
{
    return static_cast<cip::tibia::qmlcomponents::enums::EVocation>(m_storage->vocation());
}

QString TSkillWheelPageController::currentPresetName() const { return m_storage->canManagePresets() ? m_storage->currentPresetName() : QString(); }

void TSkillWheelPageController::onInitialData()
{
    Q_EMIT initialDataChanged();
    Q_EMIT presetDataChanged();
    onWheelChanged();
}

void TSkillWheelPageController::onWheelChanged()
{
    updateParameters();
    updateSummaries();
    updateInformation();
    Q_EMIT dataChanged();
    Q_EMIT presetDataChanged();
}

/// CIP 0x100de5150: the slices and the four corners.
void TSkillWheelPageController::updateParameters()
{
    const TWheel& wheel = m_storage->currentWheel();
    const int voc = m_storage->vocationIndex();

    std::array<QUrl, SlotCount> vesselImage{};
    std::array<int, SlotCount> vesselMod{};
    for (int d = 0; d < DomainCount; ++d) {
        const TGem* gem = wheel.gems[d] == NoGem ? nullptr : m_storage->gem(wheel.gems[d]);
        const int res = resonance(wheel.points, d);
        if (!gem)
            continue;
        // CIP 0x100de5150 sorts the slices by maximum points (0x100df0a50 with
        // 0x101122690), so the mods go to the 50/75, then 100, then 150 vessel.
        auto slots = domainSlots(d);
        std::ranges::stable_sort(slots, {}, [](const int slot) { return maxPoints(slot); });
        int k = 0;
        for (const int slot : slots) {
            if (!isVesselPerk(mediumPerk(slot, voc)) || wheel.points[slot] < maxPoints(slot))
                continue;
            if (k < res) {
                if (k == 2) {
                    if (gem->quality >= 2) {
                        vesselImage[slot] = supremeModImage(gem->supreme);
                        vesselMod[slot] = 2;
                    }
                } else {
                    vesselImage[slot] = basicModImage(k == 1 && gem->quality >= 1 ? gem->basic2 : gem->basic1);
                    vesselMod[slot] = 1;
                }
            }
            ++k;
        }
    }

    const QList<QObject*> oldTiles = m_skillParameters;
    const QList<QObject*> oldCorners = m_cornerParameters;
    m_skillParameters.clear();
    m_cornerParameters.clear();
    for (int slot = 0; slot < SlotCount; ++slot) {
        const int points = wheel.points[slot];
        const double fill = std::clamp(static_cast<double>(points) / maxPoints(slot), 0.0, 1.0);
        const bool unlocked = points > 0 || isUnlocked(wheel.points, slot);
        const int medium = mediumPerk(slot, voc);
        const QUrl image = vesselMod[slot] != 0 ? vesselImage[slot] : imageUrl("icons-skillwheel-mediumperks", medium);
        m_skillParameters.append(
            new TSkillWheelTileQMLInfo(slot, fill, unlocked, medium, static_cast<int>(smallPerk(slot)), image, vesselMod[slot], this));
    }
    for (int d = 0; d < DomainCount; ++d) {
        const int points = revelationPoints(wheel, m_storage->gems(), m_storage->supremeGrades(), d, voc);
        const int stage = revelationStage(points);
        const int next = std::min(stage + 1, 3);
        double fill = 1.0;
        if (stage < next) {
            const int from = revelationThreshold(stage);
            fill = std::clamp(static_cast<double>(points - from) / (revelationThreshold(next) - from), 0.0, 1.0);
        }
        const auto [gem, bezel] = vesselImages(m_storage, d);
        m_cornerParameters.append(new TSkillWheelCornerQMLInfo(d, fill, stage, largePerk(d, voc), gem, bezel, this));
    }
    for (QObject* o : oldTiles)
        o->deleteLater();
    for (QObject* o : oldCorners)
        o->deleteLater();
}

/// CIP 0x100fa1430 summaries: the dedication totals and the revelation stages;
/// 0x100de6500 conviction perks, 0x100de7ae0 vessels, 0x100de8d30 everything.
void TSkillWheelPageController::updateSummaries()
{
    const TWheel& wheel = m_storage->currentWheel();
    const int voc = m_storage->vocationIndex();
    const TActiveMods mods = activeMods(wheel, m_storage->gems(), m_storage->basicGrades(), m_storage->supremeGrades(), voc);
    const auto byLabel = [](QList<TSummaryEntry>& list) {
        std::ranges::stable_sort(list, [](const TSummaryEntry& a, const TSummaryEntry& b) { return a.label < b.label; });
    };

    QList<TSummaryEntry> smallEntries;
    for (const ESmallPerk perk : { ESmallPerk::Life, ESmallPerk::Mana, ESmallPerk::Capacity, ESmallPerk::Mitigation }) {
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
        const int stage = revelationStage(revelationPoints(wheel, m_storage->gems(), m_storage->supremeGrades(), d, voc));
        bonus += largePerkExtraDamageHealing(stage);
        perks.append({ largePerkName(largePerk(d, voc)), perkLevelName(stage), largePerkDetailedInformation(largePerk(d, voc), stage), QUrl(), false });
    }
    std::ranges::sort(perks, [](const TSummaryEntry& a, const TSummaryEntry& b) { return a.label < b.label; });
    large.append({ qtTrId("damage_and_healing"), QStringLiteral("+%1").arg(bonus),
                   qtTrId("skill_wheel_dialog_unlock_large_perk_extra_damage_healing_info")
                       .arg(largePerkExtraDamageHealing(1))
                       .arg(largePerkExtraDamageHealing(2))
                       .arg(largePerkExtraDamageHealing(3)),
                   QUrl(), false });
    large.append(perks);

    // CIP 0x100de6500: the perk groups 0x100debd50, 0x100deba20 and the
    // leeches sorted by name, then the vessels in id order. Resistance perks
    // 0..4 are not in any vocation's wheel, so their group stays empty.
    const auto mediumGroup = [&](const std::initializer_list<int> perks, const bool sorted) {
        QList<TSummaryEntry> group;
        for (const int perk : perks) {
            const int count = mediumPerkCount(wheel.points, perk, voc);
            if (count <= 0)
                continue;
            group.append({ mediumPerkName(perk), mediumPerkValueText(wheel.points, perk, voc),
                           withAugmentationIcons(mediumPerkDescriptions(perk), perk, count).join(QStringLiteral("<br>")), QUrl(), false });
        }
        if (sorted)
            std::ranges::stable_sort(group, [](const TSummaryEntry& a, const TSummaryEntry& b) { return a.label < b.label; });
        return group;
    };
    const QList<TSummaryEntry> perkGroup = mediumGroup({ 7, 8, 9, 15, 16, 17, 28, 34, 35, 23, 41, 42, 43 }, true);
    const QList<TSummaryEntry> augmentedGroup =
        mediumGroup({ 10, 11, 14, 13, 12, 18, 19, 20, 21, 22, 24, 25, 26, 27, 29, 30, 31, 32, 33, 36, 44, 45, 46, 47, 48 }, true);
    const QList<TSummaryEntry> vesselGroup = mediumGroup({ 37, 38, 39, 40 }, false);
    QList<TSummaryEntry> medium = perkGroup;
    medium.append(augmentedGroup);
    medium.append(mediumGroup({ 5, 6 }, true));
    medium.append(vesselGroup);

    // CIP 0x101126c40 entries (resistances), sorted under a "Resistance:" header.
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

    // CIP 0x100dec350: the radiant vessels' spell augmentations.
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
    // CIP 0x100deca10.
    QList<TSummaryEntry> revelations;
    for (const TActiveMods::TRevelation& rev : mods.revelations)
        revelations.append({ supremeModName(rev.mod, true, true), supremeModValueText(rev.bonus, rev.mod),
                             qtTrId("skillwheel_gem_mod_revelation_mastery_info"), QUrl(), false });
    byLabel(revelations);
    const auto momentumEntry = [&](const QString& info) -> TSummaryEntry {
        return { qtTrId("momentum_caption"), QStringLiteral("+%1%").arg(mods.momentum), info, QUrl(), false };
    };

    // CIP 0x100de7ae0.
    QList<TSummaryEntry> vessels;
    if (mods.damageHealing > 0)
        vessels.append({ qtTrId("damage_and_healing"), QStringLiteral("+%1").arg(englishNumber(mods.damageHealing)), vesselBonusInformation(),
                         QUrl(), false });
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
    // CIP 0x100df5cb0: life, mana, capacity, mitigation, then by name.
    QList<TSummaryEntry> stats;
    for (const auto& [stat, value] : mods.basicStats)
        if (stat > 8)
            stats.append({ basicStatName(stat, true), basicStatValueText(value, stat), basicStatInfo(stat), QUrl(), false });
    const auto statRank = [](const QString& label) {
        for (const int stat : { 10, 11, 12, 9 })
            if (label == basicStatName(stat, true))
                return stat == 9 ? 3 : stat - 10;
        return 99;
    };
    std::ranges::stable_sort(stats, [&](const TSummaryEntry& a, const TSummaryEntry& b) {
        const int ra = statRank(a.label), rb = statRank(b.label);
        return ra != rb ? ra < rb : a.label < b.label;
    });
    vessels.append(stats);
    vessels.append(resistances(resistanceValues));

    // CIP 0x100de8d30: groups joined by separators (0x100decc60).
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
    appendGroup(vesselGroup);

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
    fillVessel(m_selectedVessel, m_selectedVesselDomain);
    fillVessel(m_hoveredVessel, m_hoveredVesselDomain);
}

void TSkillWheelPageController::fillTile(cip::tibia::gamewindow::TTileInformation* info, const int slot) const
{
    if (slot < 0 || slot >= SlotCount)
        return;
    const TWheel& wheel = m_storage->currentWheel();
    const int voc = m_storage->vocationIndex();
    const int points = wheel.points[slot];
    const int max = maxPoints(slot);
    const ESmallPerk smallPerkId = smallPerk(slot);
    const int medium = mediumPerk(slot, voc);
    info->setCurrentSkillPoints(points);
    info->setMaximumSkillPoints(max);
    info->setCanBeDecreased(m_storage->changeState() == 1 && canDecrease(wheel.points, slot));
    info->setCanBeIncreased(canBeIncreased() && points < max && m_storage->freePoints() > 0 && isUnlocked(wheel.points, slot));
    info->setSmallPerkHover(smallPerkInfo(smallPerkId, voc));
    info->setSmallPerkSumInfo(smallPerkValueText(smallPerkId, points, voc));
    // CIP 0x100ded010: an augmented spell shows its two levels (plain and with
    // the level icons as hover), any other perk its first description line.
    const int count = mediumPerkCount(wheel.points, medium, voc);
    const bool augmented = isAugmentedPerk(medium);
    const QStringList lines = mediumPerkDescriptions(medium);
    const QStringList iconLines = withAugmentationIcons(lines, medium, count);
    info->setMediumPerkUnlocked(points >= max);
    info->setMediumPerkInfo(mediumPerkEffect(medium));
    info->setMediumPerkHover(qtTrId("skill_wheel_dialog_unlock_medium_perk_info") + QStringLiteral("<br><br>")
                             + qtTrId(augmented ? "skill_wheel_dialog_how_augmentatios_unlock_info" : "skill_wheel_dialog_medim_perk_more_than_once_info"));
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
    if (domain < 0 || domain >= DomainCount)
        return;
    const int voc = m_storage->vocationIndex();
    const int points = revelationPoints(m_storage->currentWheel(), m_storage->gems(), m_storage->supremeGrades(), domain, voc);
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
                                .arg(revelationThreshold(3)));
}

/// CIP 0x100ded780.
void TSkillWheelPageController::fillVessel(cip::tibia::gamewindow::TVesselInformation* info, const int domain) const
{
    if (domain < 0 || domain >= DomainCount)
        return;
    const TWheel& wheel = m_storage->currentWheel();
    const int voc = m_storage->vocationIndex();
    const int res = resonance(wheel.points, domain);
    const TGem* gem = wheel.gems[domain] == NoGem ? nullptr : m_storage->gem(wheel.gems[domain]);
    const TGrades& basic = m_storage->basicGrades();
    const TGrades& supreme = m_storage->supremeGrades();
    info->setVesselActivation(vesselActivation(res, false));
    info->setVesselBonusInfo(vesselBonusInformation());
    info->setGemInfo(gemInformation());
    info->setNumberOfActiveMods(res);
    if (!gem) {
        info->setGemName(QString());
        info->setMod1Info(QString());
        info->setMod2Info(QString());
        info->setMod3Name(QString());
        info->setMod3Info(QString());
        info->setMod1GradeImage(QUrl());
        info->setMod2GradeImage(QUrl());
        info->setMod3GradeImage(QUrl());
        info->setHasVesselBonus(false);
        info->setVesselBonus(QString());
        return;
    }
    const int count = modCount(*gem);
    info->setGemName(itemName(unrevealedGemTypeId(voc, gem->quality)));
    const int g0 = effectiveGrade(*gem, basic, supreme, 0);
    info->setMod1Info(modInfo(basicModEffect(gem->basic1, voc, g0), g0));
    info->setMod1GradeImage(modGradeIcon(g0));
    if (count > 1) {
        const int g1 = effectiveGrade(*gem, basic, supreme, 1);
        info->setMod2Info(modInfo(basicModEffect(gem->basic2, voc, g1), g1));
        info->setMod2GradeImage(modGradeIcon(g1));
    } else {
        info->setMod2Info(QString());
        info->setMod2GradeImage(QUrl());
    }
    if (count > 2) {
        const int g2 = effectiveGrade(*gem, basic, supreme, 2);
        info->setMod3Name(isAugmentationMod(gem->supreme) ? supremeModName(gem->supreme, false, true) : QString());
        info->setMod3Info(modInfo(supremeModEffect(gem->supreme, g2), g2));
        info->setMod3GradeImage(modGradeIcon(g2));
    } else {
        info->setMod3Name(QString());
        info->setMod3Info(QString());
        info->setMod3GradeImage(QUrl());
    }
    // CIP 0x100dee038: the bonus is 1 + (greater gem), active only when the
    // resonance equals quality + 1 (0x100fb0f20).
    const bool match = vesselMatchesGem(gem->quality, res);
    info->setHasVesselBonus(match);
    info->setVesselBonus(QStringLiteral("%1 %2").arg(QStringLiteral("+%1").arg(gem->quality == 2 ? 2 : 1), qtTrId("damage_and_healing")));
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
    m_storage->setCurrentWheel(wheel);
}

/// CIP 0x100de20d0.
void TSkillWheelPageController::addToSkill(const int GridTileQML, const int KeyboardModifiers)
{
    if (!canBeIncreased() || GridTileQML < 0 || GridTileQML >= SlotCount || !isUnlocked(m_storage->currentWheel().points, GridTileQML))
        return;
    const int amount = std::min(stepSize(KeyboardModifiers), std::max(0, m_storage->freePoints()));
    changePoints(GridTileQML, amount);
}

/// CIP 0x100de22d0.
void TSkillWheelPageController::fillSkill(const int GridTileQML)
{
    if (!canBeIncreased() || GridTileQML < 0 || GridTileQML >= SlotCount || !isUnlocked(m_storage->currentWheel().points, GridTileQML))
        return;
    changePoints(GridTileQML, std::min(maxPoints(GridTileQML), std::max(0, m_storage->freePoints())));
}

/// CIP 0x100de2650.
void TSkillWheelPageController::removeFromSkill(const int GridTileQML, const int KeyboardModifiers)
{
    if (m_storage->changeState() != 1 || GridTileQML < 0 || GridTileQML >= SlotCount || !canDecrease(m_storage->currentWheel().points, GridTileQML))
        return;
    changePoints(GridTileQML, -std::min(stepSize(KeyboardModifiers), m_storage->currentWheel().points[GridTileQML]));
}

/// CIP 0x100de24e0.
void TSkillWheelPageController::clearSkill(const int GridTileQML)
{
    if (m_storage->changeState() != 1 || GridTileQML < 0 || GridTileQML >= SlotCount || !canDecrease(m_storage->currentWheel().points, GridTileQML))
        return;
    changePoints(GridTileQML, -m_storage->currentWheel().points[GridTileQML]);
}

/// CIP 0x100de1e40: fill (any modifier: add one step) while points fit,
/// otherwise empty the slice.
void TSkillWheelPageController::onGridTileRightClicked(const int GridTileQML, const int KeyboardModifiers)
{
    if (GridTileQML < 0 || GridTileQML >= SlotCount)
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

void TSkillWheelPageController::changedGem(const int QuarterQML)
{
    if (QuarterQML < 0 || QuarterQML >= DomainCount)
        return;
    Q_EMIT showGemAtelier(QuarterQML, m_storage->currentWheel().gems[QuarterQML]);
}

void TSkillWheelPageController::onGridTileSelected(const int GridTileQML)
{
    m_selectedSlot = GridTileQML;
    fillTile(m_selectedTile, m_selectedSlot);
}

void TSkillWheelPageController::onGridTileHovered(const int GridTileQML)
{
    m_hoveredSlot = GridTileQML;
    fillTile(m_hoveredTile, m_hoveredSlot);
}

void TSkillWheelPageController::onLargePerkSelected(const int QuarterQML)
{
    m_selectedLargeDomain = QuarterQML;
    fillLargePerk(m_selectedLarge, QuarterQML);
}

void TSkillWheelPageController::onLargePerkHovered(const int QuarterQML)
{
    m_hoveredLargeDomain = QuarterQML;
    fillLargePerk(m_hoveredLarge, QuarterQML);
}

void TSkillWheelPageController::onVesselSelected(const int QuarterQML)
{
    m_selectedVesselDomain = QuarterQML;
    fillVessel(m_selectedVessel, QuarterQML);
}

void TSkillWheelPageController::onVesselHovered(const int QuarterQML)
{
    m_hoveredVesselDomain = QuarterQML;
    fillVessel(m_hoveredVessel, QuarterQML);
}

/// Reset: an empty grid (vessel gems stay).
void TSkillWheelPageController::restSkillGrid()
{
    if (m_storage->changeState() != 1)
        return;
    TWheel wheel = m_storage->currentWheel();
    wheel.points = {};
    m_storage->setCurrentWheel(wheel);
}

/// CIP 0x100de3270: the current wheel's export code as COPY_TRUSTED_TEXT_TO_CLIPBOARD.
void TSkillWheelPageController::requestCopyExportCodeToClipboard()
{
    if (!m_storage->hasData())
        return;
    auto action = input::mutableClone(*input::makeAction(A::COPY_TRUSTED_TEXT_TO_CLIPBOARD));
    action->text = encodeWheelCode(m_storage->currentWheel(), m_storage->gems(), m_storage->vocationIndex());
    input::publish(action);
}

/// CIP switch-preset prompt (mode 3).
void TSkillWheelPageController::selectPresetByName(const QString& PresetName, const bool IgnoreChanges)
{
    if (!m_storage->canManagePresets() || PresetName == m_storage->currentPresetName())
        return;
    if (!IgnoreChanges && m_storage->isPresetDirty()) {
        m_pendingPreset = PresetName;
        m_switchPreset->setYesNoButtons();
        m_switchPreset->showMessage(qtTrId("skill_wheel_warning_closing_without_saving_title"),
                                    qtTrId("skill_wheel_warning_switch_preset_without_saving").arg(m_storage->currentPresetName()));
        return;
    }
    if (!m_storage->selectPreset(PresetName)) {
        m_info->setButtons(okButton());
        m_info->showMessage(qtTrId("info"), qtTrId("skill_wheel_dialog_presets_adjusted"));
    }
}

void TSkillWheelPageController::requestSaveCurrentPreset()
{
    if (m_storage->canManagePresets())
        m_storage->saveCurrentPreset();
}

void TSkillWheelPageController::requestDeletePresetClicked()
{
    if (!m_storage->canManagePresets() || !deletePresetsPossible())
        return;
    m_deletePreset->setYesNoButtons();
    m_deletePreset->showMessage(qtTrId("skill_wheel_manage_presets_caption_rename"),
                                qtTrId("skill_wheel_dialog_delete_preset_text").arg(m_storage->currentPresetName()));
}

void TSkillWheelPageController::requestRenamePresetClicked() { Q_EMIT managePresets(static_cast<int>(EManageMode::Rename)); }
void TSkillWheelPageController::requestNewPresetClicked() { Q_EMIT managePresets(static_cast<int>(EManageMode::NewEmptyPreset)); }
void TSkillWheelPageController::requestImportPresetClicked() { Q_EMIT managePresets(static_cast<int>(EManageMode::ImportPreset)); }
void TSkillWheelPageController::requestExportPresetClicked() { requestCopyExportCodeToClipboard(); }

void TSkillWheelPageController::setShowFullSummary(const bool Show)
{
    if (Show == m_showFullSummary)
        return;
    m_showFullSummary = Show;
    Q_EMIT summaryModeChanged();
}

void TSkillWheelPageController::useServerSkillWheelAsCurrent() { m_storage->useServerWheelAsCurrent(); }

void TSkillWheelPageController::clearSelection()
{
    m_selectedSlot = m_selectedLargeDomain = m_selectedVesselDomain = -1;
}

void TSkillWheelPageController::onNewPresetName(const QString&) { Q_EMIT presetDataChanged(); }

void TSkillWheelPageController::clearData()
{
    clearSelection();
    m_hoveredSlot = m_hoveredLargeDomain = m_hoveredVesselDomain = -1;
    closeMessages();
}

void TSkillWheelPageController::closeMessages()
{
    for (TMessageDialogController* box : { m_switchPreset, m_deletePreset, m_info })
        box->dialog()->close();
}

void TSkillWheelPageController::requestClose() { Q_EMIT closeConfirmed(); }

/// CIP onApplyClicked: the preset keeps the applied wheel, then SAVE_SKILL_WHEEL.
void TSkillWheelPageController::apply()
{
    if (!m_storage->isLocalPlayer() || m_storage->isPreview() || !m_storage->isWheelDirty())
        return;
    if (m_storage->canManagePresets())
        m_storage->saveCurrentPreset();
    Q_EMIT preapareToSkipNextOpenForOwnPlayer();
    publish(A::SAVE_SKILL_WHEEL);
}

bool TSkillWheelPageController::askToSaveBeforeClose()
{
    return m_storage->isLocalPlayer() && !m_storage->isPreview() && m_storage->changeState() != 0 && m_storage->isWheelDirty();
}

// ---------------------------------------------------------------------------
// Gem atelier

TGemAtelierPageController::TGemAtelierPageController(TSkillWheelStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TGemAtelierPageController(parent)
    , m_storage(storage)
    , m_list(new TInventoryGemListModel(storage, this))
    , m_filter(new TInventoryGemSortFilterProxyModel(storage, this))
    , m_pages(new TPaginationProxyModel(15, this))
{
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
    connect(m_storage, &TSkillWheelStorage::serverDataReceived, this, &TGemAtelierPageController::onGemsChanged);
    connect(m_storage, &TSkillWheelStorage::currentWheelChanged, this, &TGemAtelierPageController::onWheelChanged);
    connect(m_storage, &TSkillWheelStorage::resourcesChanged, this, &TGemAtelierPageController::onResourcesChanged);
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

/// CIP 0x100f9a160: the four vessels.
void TGemAtelierPageController::updateSockets()
{
    QObject** sockets[] = { &m_topLeftSocket, &m_topRightSocket, &m_bottomLeftSocket, &m_bottomRightSocket };
    for (int d = 0; d < DomainCount; ++d) {
        const auto [gem, bezel] = vesselImages(m_storage, d);
        QObject* old = *sockets[d];
        *sockets[d] = new TQmlSocketedGem(m_storage->currentWheel().gems[d], gem, bezel, this);
        if (old)
            old->deleteLater();
    }
    Q_EMIT vesselChanged();
}

QString TGemAtelierPageController::unrevealedLesserGemName() const { return itemName(unrevealedGemTypeId(m_storage->vocationIndex(), 0)); }
QString TGemAtelierPageController::unrevealedRegularGemName() const { return itemName(unrevealedGemTypeId(m_storage->vocationIndex(), 1)); }
QString TGemAtelierPageController::unrevealedGreaterGemName() const { return itemName(unrevealedGemTypeId(m_storage->vocationIndex(), 2)); }
int TGemAtelierPageController::unrevealedLesserGemAppearanceType() const { return unrevealedGemTypeId(m_storage->vocationIndex(), 0); }
int TGemAtelierPageController::unrevealedRegularGemAppearanceType() const { return unrevealedGemTypeId(m_storage->vocationIndex(), 1); }
int TGemAtelierPageController::unrevealedGreaterGemAppearanceType() const { return unrevealedGemTypeId(m_storage->vocationIndex(), 2); }

int TGemAtelierPageController::changeAffinityCost() const
{
    const int id = m_selectedGemIndex.isValid() ? m_selectedGemIndex.data(TInventoryGemListModel::GemId).toInt() : m_selectedGemId;
    const TGem* gem = m_storage->gem(id);
    return gem ? ::tibia::skillwheel::changeAffinityCost(gem->quality) : 0;
}

void TGemAtelierPageController::gemAction(const int type, const int param, const int pos)
{
    g_game.gemAction(static_cast<uint8_t>(type), static_cast<uint16_t>(param), static_cast<uint8_t>(pos));
}

/// Vessels are part of the edited wheel: socketing changes the current wheel,
/// Apply sends it.
void TGemAtelierPageController::socketOrUnsocketGem(const int GemId)
{
    const TGem* gem = m_storage->gem(GemId);
    if (!gem || !canBeChanged())
        return;
    TWheel wheel = m_storage->currentWheel();
    wheel.gems[gem->domain] = wheel.gems[gem->domain] == GemId ? NoGem : GemId;
    m_storage->setCurrentWheel(wheel);
}

void TGemAtelierPageController::dismantleGem(const int GemId) { gemAction(0, GemId); }
void TGemAtelierPageController::revealGem(const int GemQuality) { gemAction(1, GemQuality); }
void TGemAtelierPageController::changeAffinityOfGem(const int GemId) { gemAction(2, GemId); }
void TGemAtelierPageController::toggleGemLock(const int GemId) { gemAction(3, GemId); }

void TGemAtelierPageController::selectGem(const int GemId) { selectGem(GemId, false); }

/// Shows the gem: with `NarrowDownFilters` the filters are cleared if they hide it.
void TGemAtelierPageController::selectGem(const int GemId, const bool NarrowDownFilters)
{
    m_selectedGemId = GemId;
    const TGem* gem = m_storage->gem(GemId);
    if (!gem)
        return;
    if (NarrowDownFilters && !m_filter->accepts(*gem)) {
        m_filter->setFilterModName(QString());
        m_filter->setFilterGemQuality(-1);
        m_filter->setShowLockedOnly(false);
        m_filter->setFilterDomainAffinity(gem->domain);
    }
    const int row = m_list->rowOfGem(GemId);
    const QModelIndex filtered = row < 0 ? QModelIndex() : m_filter->mapFromSource(m_list->index(row, 0));
    if (filtered.isValid())
        setSelectedGemIndex(m_pages->showSourceRow(filtered.row()));
    Q_EMIT selectedGemIndexChanged();
}

void TGemAtelierPageController::showDomain(const int domain, const int gemId)
{
    if (gemId != NoGem) {
        selectGem(gemId, true);
        return;
    }
    m_filter->setFilterDomainAffinity(domain);
}

void TGemAtelierPageController::selectModInFragmentWorkshop(const int ModId, const bool IsSupremeMod) { Q_EMIT requestWorkshopMod(!IsSupremeMod, ModId); }

// ---------------------------------------------------------------------------
// Fragment workshop

TFragmentWorkshopPageController::TFragmentWorkshopPageController(TSkillWheelStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TFragmentWorkshopPageController(parent)
    , m_storage(storage)
    , m_list(new TModListModel(storage, this))
    , m_filter(new TModFilterProxyModel(m_list, this))
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
    connect(m_filter, &TModFilterProxyModel::filterChanged, this, [this] {
        if (m_filter->filterMods() != m_filterMods) {
            m_filterMods = m_filter->filterMods();
            Q_EMIT filterChanged();
        }
    });
    connect(m_storage, &TSkillWheelStorage::serverDataReceived, this, &TFragmentWorkshopPageController::onModsChanged);
    onModsChanged();
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
    if (!canBeChanged())
        return;
    m_pendingBasic = isBasicMod;
    m_pendingMod = ModID;
    g_game.gemAction(4, isBasicMod ? 0 : 1, static_cast<uint8_t>(ModID));
}

// ---------------------------------------------------------------------------
// Preset management dialog

TSkillWheelPresetManagmentDialogController::TSkillWheelPresetManagmentDialogController(TSkillWheelStorage* storage, const EManageMode mode,
                                                                                       QObject* parent)
    : TDialogController(QStringLiteral("SkillWheelPresetManagmentDialog.qml"), parent)
    , m_storage(storage)
{
    m_manageMode = mode;
    m_currentPresetName = storage->currentPresetName();
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

/// CIP 0x100dddab0: the trimmed text after the last "&code=" (a planner link).
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

/// CIP 0x100dddcf0: an invalid code or grid, or another vocation's code.
QString TSkillWheelPresetManagmentDialogController::importError(TWheel* wheel) const
{
    const std::optional<TWheelCode> code = decodeWheelCode(m_importString);
    if (!code || !isValidWheelCode(*code))
        return qtTrId("skill_wheel_manage_presets_invalid_import_string");
    if (code->vocationIndex != m_storage->vocationIndex())
        return qtTrId("skill_wheel_manage_presets_vocation_mismatch");
    if (wheel) {
        wheel->points = code->points;
        for (int d = 0; d < DomainCount; ++d) {
            const TCodeGem& mods = code->gems[d];
            if (mods.basic1 < 0)
                continue;
            const int quality = mods.supreme >= 0 ? 2 : mods.basic2 >= 0 ? 1 : 0;
            const auto it = std::ranges::find_if(m_storage->gems(), [&](const TGem& g) {
                return g.domain == d && g.quality == quality && g.basic1 == mods.basic1 && (quality < 1 || g.basic2 == mods.basic2)
                    && (quality < 2 || g.supreme == mods.supreme);
            });
            if (it != m_storage->gems().end())
                wheel->gems[d] = it->id;
        }
    }
    return {};
}

/// CIP 0x100ddd1e0: rename, new empty, copy or import (0x100ddd840: only a
/// code without error), then newPresetName.
void TSkillWheelPresetManagmentDialogController::onOkClicked(const EManageMode ManageMod)
{
    if (!isValidName(m_name))
        return;
    bool done = false;
    switch (ManageMod) {
        case EManageMode::Rename: done = m_name == m_currentPresetName || m_storage->renameCurrentPreset(m_name); break;
        case EManageMode::NewEmptyPreset: done = m_storage->addPreset(m_name, TWheel{}); break;
        case EManageMode::CopyPreset: done = m_storage->addPreset(m_name, m_storage->currentWheel()); break;
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
            done = m_storage->addPreset(m_name, wheel);
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
    , m_closePrompt(new TMessageDialogController(this))
{
    m_wheelPageController = m_page;
    m_gemAtelierPageController = m_atelier;
    m_fragmentWorkshopPageController = m_workshop;
    m_message->setCloseOnButtonClicked(true);
    m_closePrompt->setCloseOnButtonClicked(true);
    connect(m_message, &TMessageDialogController::acceptClicked, this, [this] {
        if (m_message->property("offerPreview").toBool())
            requestSkillWheelPreview();
    });
    connect(m_closePrompt, &TMessageDialogController::acceptClicked, this, [this] {
        m_page->apply();
        closeDialog();
    });
    connect(m_closePrompt, &TMessageDialogController::discardClicked, this, [this] { closeDialog(); });
    connect(m_page, &TSkillWheelPageController::showGemAtelier, this, &TSkillWheelDialogController::openGemAtelier);
    connect(m_page, &TSkillWheelPageController::managePresets, this, &TSkillWheelDialogController::managePresets);
    connect(m_page, &TSkillWheelPageController::closeConfirmed, this, &TSkillWheelDialogController::requestClose);
    connect(m_atelier, &TGemAtelierPageController::requestWorkshopMod, this, &TSkillWheelDialogController::openWorkshopMod);
}

TSkillWheelDialogController::~TSkillWheelDialogController() = default;

bool TSkillWheelDialogController::isPremium() const
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player && player->isPremium();
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
    m_page->clearData();
    m_closePrompt->dialog()->close();
    if (m_presetDialog)
        m_presetDialog->dialog()->close();
    dialog()->close();
    m_storage->useServerWheelAsCurrent();
}

/// CIP close-without-saving prompt (mode 1).
void TSkillWheelDialogController::requestClose()
{
    if (m_page->askToSaveBeforeClose()) {
        m_closePrompt->setYesNoButtons();
        m_closePrompt->showMessage(qtTrId("skill_wheel_warning_closing_without_saving_title"), qtTrId("skill_wheel_warning_closing_without_saving"));
        return;
    }
    closeDialog();
}

void TSkillWheelDialogController::requestTabSwitch(const EDialogTab NewTab)
{
    if (NewTab == m_dialogTab)
        return;
    if ((NewTab == GemAtelier || NewTab == FragmentWorkshop) && (!m_storage->isLocalPlayer() || m_storage->isPreview()))
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
    m_page->apply();
    closeDialog();
}

void TSkillWheelDialogController::onApplyClicked() { m_page->apply(); }
void TSkillWheelDialogController::onResetClicked() { m_page->restSkillGrid(); }
void TSkillWheelDialogController::getPremiumClicked() { publish(A::OPEN_STORE, 1, 0); }
void TSkillWheelDialogController::requestCopyExportCodeToClipboarClicked() { m_page->requestCopyExportCodeToClipboard(); }

/// CIP 0x100ddc1f0 (308 accepted): an empty wheel of the own vocation.
void TSkillWheelDialogController::requestSkillWheelPreview()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (!player)
        return;
    const int vocation = player->getVocation() % 10;
    if (vocation < 1 || vocation > VocationCount)
        return;
    m_storage->setPreview(QString::fromStdString(player->getName()), vocation);
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
    if (!m_storage->isLocalPlayer() || m_storage->isPreview())
        return;
    requestTabSwitch(GemAtelier);
    m_atelier->showDomain(domain, gemId);
}

void TSkillWheelDialogController::openWorkshopMod(const bool isBasic, const int modId)
{
    if (!m_storage->isLocalPlayer() || m_storage->isPreview())
        return;
    requestTabSwitch(FragmentWorkshop);
    m_workshop->selectMod(isBasic, modId);
}

void TSkillWheelDialogController::managePresets(const int mode)
{
    if (!m_storage->canManagePresets())
        return;
    if (m_presetDialog) {
        m_presetDialog->dialog()->close();
        m_presetDialog->deleteLater();
    }
    m_presetDialog = new TSkillWheelPresetManagmentDialogController(m_storage, static_cast<EManageMode>(mode), this);
    connect(m_presetDialog, &TSkillWheelPresetManagmentDialogController::newPresetName, m_page, &TSkillWheelPageController::onNewPresetName);
    m_presetDialog->dialog()->show();
}

void TSkillWheelDialogController::clear()
{
    m_message->dialog()->close();
    closeDialog();
}

}
