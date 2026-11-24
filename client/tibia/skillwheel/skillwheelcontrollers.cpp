#include "skillwheelcontrollers.h"
#include "app/bootstrap.h"
#include "skillwheel/skillwheelciptypes.h"
#include "skillwheel/skillwheelgameactions.h"

#include "config/characterconfigurationmanager.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "input/gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <QCollator>
#include <string_view>
#include <QJSEngine>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <utility>
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
using gamewindow::okDialogButtons;
using Button = TMessageDialogController::Button;
using EManageMode = cip::tibia::gamewindow::TSkillWheelPresetManagmentDialogController::EManageMode;

/// The slice is below its maximum and the grid stays valid with the slice
/// filled to its maximum.
bool canIncrease(const std::array<int, SlotCount>& points, const int slot)
{
    const int max = gridConfiguration().maxPoints(slot);
    if (points[slot] >= max)
        return false;
    auto filled = points;
    filled[slot] = max;
    return isValid(filled);
}

/// Socketed gem image and vessel bezel for atelier vessel index 0..7.
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

/// CIP: the session's configuration manager's skill wheel storage. Host: without a session (before
/// the first login, at shutdown) an empty process-wide storage, so the pages' QML properties never
/// read a null storage (the old host skill wheel manager owned one for the client's life).
TSkillWheelStorage* sessionSkillWheelStorage(const std::shared_ptr<game::IGameSession>& session)
{
    static TSkillWheelStorage* const emptyStorage = new TSkillWheelStorage();
    const auto* base = dynamic_cast<const game::TGameSessionBase*>(session.get());
    TSkillWheelStorage* storage =
        base && base->characterConfigurationManager() ? base->characterConfigurationManager()->skillWheelStorage() : nullptr;
    return storage ? storage : emptyStorage;
}

/// The session's skill wheel protocol handler; null without a game session.
TSkillWheelProtocolMessageHandler* sessionSkillWheelHandler(const std::shared_ptr<game::IGameSession>& session)
{
    const auto* base = dynamic_cast<const game::TGameSessionBase*>(session.get());
    return base ? base->skillWheelProtocolMessageHandler().get() : nullptr;
}

int spentPoints(const std::array<int, SlotCount>& points) { return std::accumulate(points.begin(), points.end(), 0); }

/// CIP: one slice of the trimmed points, from the centre outwards, gives up points so that the
/// spent points reach the available ones (never below 0). The first pass (fullSlices false) takes
/// slices with points below their maximum and keeps a reduction that cannot reach the target; the
/// second takes full slices. A reduction that reaches the target, and every one of the second
/// pass, stays only when the grid stays valid; true when one stayed that way.
bool reduceSlice(std::array<int, SlotCount>& points, const int available, const bool fullSlices)
{
    for (const int slot : gridConfiguration().tilesFromCentre()) {
        if (points[slot] <= 0 || (points[slot] == gridConfiguration().maxPoints(slot)) != fullSlices)
            continue;
        const int old = points[slot];
        points[slot] = std::max(0, available - spentPoints(points) + old);
        if (fullSlices || spentPoints(points) == available) {
            if (isValid(points))
                return true;
            points[slot] = old;
        }
    }
    return false;
}

/// CIP: a valid preset that spends more than the available points is cut down to them: cleared
/// when none are available, else slices are reduced until it fits (the preset stays as it was when
/// no reduction fits). True when the preset changed.
bool trimToAvailablePoints(TWheel& preset, const int available)
{
    if (!isValid(preset.points) || spentPoints(preset.points) <= available)
        return false;
    if (available < 1) {
        preset.points.fill(0);
        return true;
    }
    std::array<int, SlotCount> trimmed = preset.points;
    while (spentPoints(trimmed) > available) {
        if (!reduceSlice(trimmed, available, false) && !reduceSlice(trimmed, available, true))
            return false;
    }
    preset.points = trimmed;
    return true;
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

TQmlSocketedGem::TQmlSocketedGem(const int vesselIndex, const int vocationIndex, const std::optional<TGem>& gem, QObject* parent)
    : cip::tibia::skillwheel::TQmlSocketedGem(parent)
    , m_vesselIndex(vesselIndex)
    , m_vocationIndex(vocationIndex)
    , m_gem(gem)
{
    if (vesselIndex < 0 || vesselIndex >= VesselCount)
        throw std::invalid_argument("TVessels::Index: value out of range");
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
}

int TQmlSocketedGem::gemId() const { return m_gem ? m_gem->id : -1; }

int TQmlSocketedGem::modSlotCount() const
{
    if (m_gem)
        return hasSupreme(*m_gem) ? 3 : (hasBasic2(*m_gem) ? 2 : 1);
    const int quality = VesselQualities[m_vesselIndex];
    return quality == 2 ? 3 : (quality == 1 ? 2 : 1);
}

int TQmlSocketedGem::vesselQuality() const { return VesselQualities[m_vesselIndex]; }

QUrl TQmlSocketedGem::socketBezelSource() const
{
    const int slots = modSlotCount();
    return imageUrl("icons-skillwheel-sockets", m_gem ? m_vocationIndex * 3 + slots - 1 : slots + 14);
}

// ---------------------------------------------------------------------------
// Wheel page

TSkillWheelPageController::TSkillWheelPageController(const QString& plannerUrl, QQmlEngine* engine, QQuickItem* dialogParentItem,
                                                     QObject* parent)
    : cip::tibia::gamewindow::TSkillWheelPageController(parent)
    , m_small(new TPerkInfoModel(this))
    , m_large(new TPerkInfoModel(this))
    , m_medium(new TPerkInfoModel(this))
    , m_vessels(new TPerkInfoModel(this))
    , m_full(new TPerkInfoModel(this))
    , m_presets(new TPresetInfoModel(this))
    , m_selectedTile(new cip::tibia::gamewindow::TTileInformation(this))
    , m_hoveredTile(new cip::tibia::gamewindow::TTileInformation(this))
    , m_selectedLarge(new cip::tibia::gamewindow::TLargePerkInformation(this))
    , m_hoveredLarge(new cip::tibia::gamewindow::TLargePerkInformation(this))
    , m_message(new TMessageDialogController(engine, dialogParentItem, this))
    , m_export(new TMessageDialogController(engine, dialogParentItem, this))
    , m_presetManagmentDialog(new TSkillWheelPresetManagmentDialogController(engine, dialogParentItem, this))
    , m_plannerUrl(plannerUrl)
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

    m_message->setCloseOnButtonClicked(true);
    m_export->setCloseOnButtonClicked(true);
    connect(m_message, &TMessageDialogController::acceptClicked, this, &TSkillWheelPageController::onMessageDialogAcceptClicked);
    connect(m_message, &TMessageDialogController::discardClicked, this, &TSkillWheelPageController::onMessageDialogDiscardClicked);
    connect(m_export, &TMessageDialogController::buttonClicked, this,
            &TSkillWheelPageController::requestCopyExportCodeOrUrlToClipboardClicked);
    connect(m_presetManagmentDialog, &TSkillWheelPresetManagmentDialogController::newPresetName, this,
            &TSkillWheelPageController::onNewPresetName);
}

/// CIP: the session's storage replaces the previous one; the preset dialog works on the session's
/// presets; a user change of the configuration updates the presets, a new server wheel changes the
/// initial data, the server wheel, the current wheel and the mod grades refresh the page.
void TSkillWheelPageController::connectToGameSession(std::shared_ptr<game::IGameSession> GameSession)
{
    m_gameSessionConnections.disconnectAll();
    m_storage = nullptr;
    m_gameSession = std::move(GameSession);
    m_presetManagmentDialog->setGameSession(m_gameSession);
    m_storage = sessionSkillWheelStorage(m_gameSession);
    if (m_storage) {
        const auto add = [this](const QMetaObject::Connection& connection) { m_gameSessionConnections.add(connection); };
        add(connect(m_storage, &TSkillWheelStorage::configurationChangedFromUserInput, this,
                    &TSkillWheelPageController::updatePresetsInfoModelIfNeeded));
        add(connect(m_storage, &TSkillWheelStorage::serverSkillWheelChanged, this, &TSkillWheelPageController::initialDataChanged));
        add(connect(m_storage, &TSkillWheelStorage::serverSkillWheelChanged, this, &TSkillWheelPageController::onWheelChanged));
        add(connect(m_storage, &TSkillWheelStorage::currentSkillWheelChanged, this, &TSkillWheelPageController::onWheelChanged));
        add(connect(m_storage, &TSkillWheelStorage::modGradesChanged, this, &TSkillWheelPageController::onWheelChanged));
    }
    onWheelChanged();
    updatePresetsInfoModelIfNeeded();
}

/// CIP: the own character's presets (with a known change state) sorted by name, numbers by value and
/// without case; the rows' available points are the regular, extra and mod grade points.
void TSkillWheelPageController::updatePresetsInfoModelIfNeeded()
{
    std::vector<TPresetInfo> presets;
    int totalPoints = 0;
    if (m_storage && m_storage->isOwnCharacter() && m_storage->changeState() == 1) {
        for (const auto& [name, wheel] : m_storage->presets())
            presets.push_back({ name, wheel });
        totalPoints = m_storage->regularPoints() + m_storage->extraPoints() + m_storage->modGradesPoints();
    }
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::ranges::sort(presets, [&collator](const TPresetInfo& a, const TPresetInfo& b) { return collator.compare(a.name, b.name) < 0; });
    m_presets->setTotalPoints(totalPoints);
    m_presets->setItems(presets);
}

/// CIP: the page's data goes with it.
TSkillWheelPageController::~TSkillWheelPageController() { clearData(); }

/// CIP: graphic effects need a renderer other than the software one (renderer index 3, the
/// graphics engine the process started with).
bool TSkillWheelPageController::rendererSupportsGraphicEffects() const
{
    return !app::isSoftwareGraphicsEngineActive();
}

/// The mod grade points the current wheel's vocation can reach; 0 without a current wheel.
int TSkillWheelPageController::totalModGradesSkillPoints() const
{
    const std::optional<TWheel> current = m_storage->currentSkillWheel();
    return current ? totalModGradePoints(current->vocation - 1) : 0;
}

int TSkillWheelPageController::spentSkillPoints() const
{
    const std::optional<TWheel> current = m_storage->currentSkillWheel();
    return current ? spentPoints(current->points) : 0;
}

/// One tooltip line per consumed promotion scroll: its item name and points.
QString TSkillWheelPageController::bonusPointItemNames() const
{
    QStringList lines;
    for (const auto& [typeId, points] : m_storage->promotionScrolls())
        lines << qtTrId("skill_wheel_skill_promotion_points_bonus_consumed_item_tooltip").arg(itemName(typeId)).arg(points);
    return lines.join(QStringLiteral("<br>"));
}

/// The current wheel's vocation; none without a current wheel.
cip::tibia::qmlcomponents::enums::EVocation TSkillWheelPageController::vocation() const
{
    const std::optional<TWheel> current = m_storage->currentSkillWheel();
    return static_cast<cip::tibia::qmlcomponents::enums::EVocation>(current ? current->vocation : 0);
}

QString TSkillWheelPageController::currentPresetName() const { return m_currentPresetName; }

/// The own wheel at a temple.
bool TSkillWheelPageController::canManagePresets() const { return m_storage->isOwnCharacter() && m_storage->changeState() == 1; }

/// The current wheel is not the selected preset's wheel any more.
bool TSkillWheelPageController::isPresetDirty() const { return m_storage->currentSkillWheel() != m_presetWheel; }

void TSkillWheelPageController::setCurrentPresetName(const QString& name)
{
    if (m_currentPresetName == name)
        return;
    m_currentPresetName = name;
    Q_EMIT presetDataChanged();
}

bool TSkillWheelPageController::hasOwnWheel() const
{
    // Host: no storage before the page knows a session.
    return m_storage && m_storage->isOwnCharacter() && totalSkillPoints() != 0 && m_storage->currentSkillWheel().has_value();
}

/// CIP: the selection goes; unless the show was asked to keep them, the presets are checked against
/// the character, the preset matching the current wheel is selected, then a pending one.
void TSkillWheelPageController::showDialogTab()
{
    clearSelection();
    if (!m_skipNextShowUpdate) {
        validatePresets();
        updatePresetsInfoModelIfNeeded();
        selectMatchingPreset();
        selectPendingPreset();
    }
    m_skipNextShowUpdate = false;
}

void TSkillWheelPageController::hideDialogTab()
{
    clearSelection();
    closeMessages();
    m_skipNextShowUpdate = false;
}

/// CIP: presets of another vocation or with an invalid grid are dropped, the others are trimmed to
/// the character's skill points and their vessels' gems are matched against the gem inventory (a
/// gem that is gone leaves its vessel); at most ten presets stay. When none stays the current wheel
/// becomes the default preset; a change is reported.
void TSkillWheelPageController::validatePresets()
{
    if (!hasOwnWheel())
        return;
    const std::map<QString, TWheel> presets = m_storage->presets();
    const int vocation = tibia::game::localPlayerVocation() % 10;
    std::map<QString, TWheel> valid;
    bool changed = false;
    for (const auto& [name, preset] : presets) {
        if (preset.vocation != vocation || !isValid(preset.points)) {
            changed = true;
            continue;
        }
        TWheel resolved = preset;
        if (trimToAvailablePoints(resolved, totalSkillPoints()))
            changed = true;
        for (int vessel = 0; vessel < VesselCount; ++vessel) {
            if (!resolved.vessels[vessel])
                continue;
            if (const std::optional<TGem> found = m_storage->findGem(*resolved.vessels[vessel])) {
                socketGem(resolved, *found, vessel);
            } else {
                unsocketGem(resolved, vessel);
                changed = true;
            }
        }
        valid.emplace(name, resolved);
        if (valid.size() >= static_cast<size_t>(MaxPresets))
            break;
    }
    m_storage->setPresets(valid);
    if (valid.empty())
        ensureDefaultPreset();
    if (changed)
        showPresetsAdjustedMessage();
}

/// CIP: the current wheel is stored as the default preset (making room by dropping the preset with
/// the fewest spent points when ten exist), reported, and selected.
void TSkillWheelPageController::ensureDefaultPreset()
{
    if (!hasOwnWheel())
        return;
    std::map<QString, TWheel> presets = m_storage->presets();
    if (presets.size() >= static_cast<size_t>(MaxPresets) && !presets.contains(TSkillWheelStorage::DefaultPresetName)) {
        QString fewest;
        int fewestPoints = std::numeric_limits<int>::max();
        for (const auto& [name, preset] : presets) {
            if (const int points = spentPoints(preset.points); points < fewestPoints) {
                fewest = name;
                fewestPoints = points;
            }
        }
        presets.erase(fewest);
    }
    presets.insert_or_assign(TSkillWheelStorage::DefaultPresetName, *m_storage->currentSkillWheel());
    m_storage->setPresets(presets);
    showPresetsAdjustedMessage();
    selectPresetByName(TSkillWheelStorage::DefaultPresetName, true);
}

/// CIP: the selected preset when it is the current wheel, else the preset the current wheel grew
/// from (every slice at least the preset's points) with the fewest points and vessel changes to it;
/// without one the current wheel becomes the default preset. The chosen preset takes the current
/// wheel. Without the own wheel nothing is selected.
void TSkillWheelPageController::selectMatchingPreset()
{
    if (!hasOwnWheel()) {
        selectPresetByName(QString(), true);
        return;
    }
    const TWheel current = *m_storage->currentSkillWheel();
    QString name;
    if (const std::optional<TWheel> selected = m_storage->preset(m_currentPresetName); selected && *selected == current) {
        name = m_currentPresetName;
    } else {
        int bestPoints = std::numeric_limits<int>::max();
        int bestVessels = std::numeric_limits<int>::max();
        for (const auto& [presetName, preset] : m_storage->presets()) {
            bool grown = true;
            for (int slot = 0; slot < SlotCount && grown; ++slot)
                grown = current.points[slot] >= preset.points[slot];
            if (!grown)
                continue;
            const int points = spentPoints(current.points) - spentPoints(preset.points);
            int vessels = 0;
            for (int vessel = 0; vessel < VesselCount; ++vessel)
                vessels += current.vessels[vessel].has_value() != preset.vessels[vessel].has_value()
                        || (current.vessels[vessel] && !sameGem(*current.vessels[vessel], *preset.vessels[vessel]))
                    ? 1
                    : 0;
            if (points < bestPoints || (points == bestPoints && vessels < bestVessels)) {
                name = presetName;
                bestPoints = points;
                bestVessels = vessels;
            }
        }
        if (name.isEmpty()) {
            ensureDefaultPreset();
            return;
        }
    }
    if (m_storage->preset(name) != current) {
        m_storage->addOrReplacePreset(name, current);
        showPresetsAdjustedMessage();
    }
    selectPresetByName(name, false);
}

/// CIP: a preset waiting from a switch is selected without asking.
void TSkillWheelPageController::selectPendingPreset()
{
    if (m_pendingPreset.isEmpty() || !m_storage || !m_storage->isOwnCharacter() || totalSkillPoints() == 0)
        return;
    selectPresetByName(m_pendingPreset, true);
    m_pendingPreset.clear();
}

void TSkillWheelPageController::showPresetsAdjustedMessage()
{
    m_messageMode = PresetsAdjustedMessage;
    m_message->setButtons(okDialogButtons());
    m_message->showMessage(qtTrId("info"), qtTrId("skill_wheel_dialog_presets_adjusted"));
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

    if (m_storage && m_storage->hasData()) {
        const TWheel& wheel = m_storage->currentWheel();
        const int voc = m_storage->vocationIndex();
        std::array<int, SlotCount> slots{};
        for (int slot = 0; slot < SlotCount; ++slot)
            slots[slot] = slot;
        std::sort(slots.begin(), slots.end(), [](const int a, const int b) { return gridConfiguration().maxPoints(a) < gridConfiguration().maxPoints(b); });
        for (const int slot : slots) {
            const int points = wheel.points[slot];
            const double fill = std::clamp(static_cast<double>(points) / gridConfiguration().maxPoints(slot), 0.0, 1.0);
            const bool unlocked = points > 0 || isUnlocked(wheel.points, slot);
            const int medium = gridConfiguration().mediumPerk(slot, voc);
            const std::optional<int> large =
                isMasteryIncreasePerk(medium) ? std::optional<int>(gridConfiguration().largePerk(domainOfSlot(slot), voc)) : std::nullopt;
            m_skillParameters.append(new TSkillWheelTileQMLInfo(slot, fill, unlocked, medium, gridConfiguration().smallPerk(slot, voc),
                                                                mediumPerkImage(medium, large), this));
        }
        for (int d = 0; d < DomainCount; ++d) {
            const int points = revelationPoints(wheel, m_storage->supremeGrades(), d, voc);
            const int stage = gridConfiguration().revelationStage(points);
            const int next = stage < -1 ? 0 : std::min(stage + 1, 3);
            const double from = stage >= 1 && stage <= 3 ? revelationThreshold(stage) : 0.0;
            const double to = next >= 1 && next <= 3 ? revelationThreshold(next) : 0.0;
            double fill = 1.0;
            if (stage < next)
                fill = std::clamp((points - from) / (to - from), 0.0, 1.0);
            m_cornerParameters.append(new TSkillWheelCornerQMLInfo(d, fill, stage, gridConfiguration().largePerk(d, voc), this));
        }
    }
    Q_EMIT dataChanged();
}

/// The summaries: the dedication totals, the revelation stages, the conviction
/// perks, the vessels and everything. Without wheel data the first three are empty.
void TSkillWheelPageController::updateSummaries()
{
    // Without a session's storage there is no wheel to sum up.
    if (!m_storage) {
        for (TPerkInfoModel* model : { m_small, m_large, m_medium, m_vessels, m_full })
            model->setItems({});
        return;
    }
    const bool hasData = m_storage->hasData();
    const TWheel& wheel = m_storage->currentWheel();
    const int voc = m_storage->vocationIndex();
    const TActiveMods mods = activeMods(wheel, m_storage->basicGrades(), m_storage->supremeGrades());
    const auto byLabel = [](QList<TPerkInfo>& list) {
        std::ranges::stable_sort(list, [](const TPerkInfo& a, const TPerkInfo& b) { return a.label < b.label; });
    };

    QList<TPerkInfo> smallEntries;
    for (const ESmallPerk perk : { ESmallPerk::Life, ESmallPerk::Mana, ESmallPerk::Capacity, ESmallPerk::Mitigation }) {
        if (!hasData)
            break;
        TPerkInfo e;
        e.label = smallPerkName(perk);
        e.effect = formatValue(smallPerkTotal(wheel.points, perk, voc), smallPerkFormat(perk));
        if (perk == ESmallPerk::Mitigation)
            e.additionalInfo = qtTrId("skill_wheel_dialog_mitigation_perk_info");
        smallEntries.append(e);
    }

    QList<TPerkInfo> large;
    int bonus = 0;
    QList<TPerkInfo> perks;
    for (int d = 0; d < DomainCount; ++d) {
        const int stage = gridConfiguration().revelationStage(revelationPoints(wheel, m_storage->supremeGrades(), d, voc));
        bonus += largePerkExtraDamageHealing(stage);
        perks.append({ largePerkName(gridConfiguration().largePerk(d, voc)), perkLevelName(stage), largePerkDetailedInformation(gridConfiguration().largePerk(d, voc), stage), QUrl(), false });
    }
    std::ranges::sort(perks, [](const TPerkInfo& a, const TPerkInfo& b) { return a.label < b.label; });
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
    const auto mediumEntry = [&](const QString& label, const int perk, const bool withIcons) -> TPerkInfo {
        QStringList lines = mediumPerkDescriptions(perk);
        if (withIcons)
            lines = withAugmentationIcons(lines, perk, mediumPerkCount(wheel.points, perk, voc));
        return { label, mediumPerkValueText(wheel.points, perk, voc), lines.join(QStringLiteral("<br>")), QUrl(), false };
    };
    const auto mediumGroup = [&](const std::initializer_list<int> perks, const bool withIcons) {
        QList<TPerkInfo> group;
        for (const int perk : perks)
            if (mediumPerkCount(wheel.points, perk, voc) > 0)
                group.append(mediumEntry(mediumPerkName(perk), perk, withIcons));
        byLabel(group);
        return group;
    };
    const QList<TPerkInfo> perkGroup = mediumGroup({ 7, 8, 9, 15, 16, 17, 28, 34, 35, 23, 38, 39, 40, 37 }, false);
    const QList<TPerkInfo> augmentedGroup =
        mediumGroup({ 10, 11, 14, 13, 12, 18, 19, 20, 21, 22, 24, 25, 26, 27, 29, 30, 31, 32, 33, 36, 41, 42, 43, 44, 45 }, true);
    QList<TPerkInfo> leechGroup;
    for (const auto& [perk, label] : { std::pair{ 5, "mana_leech" }, std::pair{ 6, "life_leech" } })
        if (mediumPerkCount(wheel.points, perk, voc) > 0)
            leechGroup.append(mediumEntry(qtTrId(label), perk, false));
    byLabel(leechGroup);
    QList<TPerkInfo> resistanceGroup;
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
    QList<TPerkInfo> masteryGroup;
    for (int d = 0; d < DomainCount; ++d) {
        int filled = 0;
        for (const int slot : domainSlots(d))
            if (isMasteryIncreasePerk(gridConfiguration().mediumPerk(slot, voc)) && wheel.points[slot] >= gridConfiguration().maxPoints(slot))
                ++filled;
        if (filled > 0)
            masteryGroup.append(mediumEntry(mediumPerkName(MasteryIncreasePerk, gridConfiguration().largePerk(d, voc)), MasteryIncreasePerk, false));
    }
    byLabel(masteryGroup);
    QList<TPerkInfo> medium;
    if (hasData) {
        medium = perkGroup;
        medium.append(augmentedGroup);
        medium.append(leechGroup);
        medium.append(resistanceGroup);
        medium.append(masteryGroup);
    }

    // CIP entries (resistances), sorted under a "Resistance:" header.
    const auto resistances = [&](const std::array<double, 9>& values) {
        QList<TPerkInfo> group;
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
    QList<TPerkInfo> augmentations;
    for (const TActiveMods::TAugmentation& aug : mods.augmentations) {
        QUrl image;
        switch (aug.augType) {
            case 2:
            case 3: image = QUrl(QStringLiteral("/images/skillwheel/icon-spelldamage.png")); break;
            case 5: image = QUrl(QStringLiteral("/images/skillwheel/icon-jumptargets.png")); break;
            case 6: image = QUrl(QStringLiteral("/images/skillwheel/icon-cooldown-reduction.png")); break;
            case 10: image = QUrl(QStringLiteral("/images/skillwheel/icon-range.png")); break;
            case 16: image = QUrl(QStringLiteral("/images/skillwheel/icon-crit.png")); break;
            default: break;
        }
        const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(aug.spellId);
        augmentations.append({ spell ? spell->name : qtTrId("dummy_unknown"), augmentationValueText(aug.value, aug.augType),
                               augmentationDescription(aug.value, aug.spellId, aug.augType, false), image, false });
    }
    // CIP .
    QList<TPerkInfo> revelations;
    for (const TActiveMods::TRevelation& rev : mods.revelations)
        revelations.append({ supremeModName(rev.mod, true, true), supremeModValueText(rev.bonus, rev.mod),
                             qtTrId("skillwheel_gem_mod_revelation_mastery_info"), QUrl(), false });
    byLabel(revelations);
    const auto momentumEntry = [&](const QString& info) -> TPerkInfo {
        return { qtTrId("momentum_caption"), QStringLiteral("+%1%").arg(mods.momentum), info, QUrl(), false };
    };

    // CIP: damage and healing, momentum, revelations, augmentations, supreme
    // mods, the stats and the resistances; without wheel data it stays empty.
    QList<TPerkInfo> vessels;
    if (mods.damageHealing > 0)
        vessels.append({ qtTrId("damage_and_healing"), QStringLiteral("+%1").arg(englishNumber(mods.damageHealing)), {}, QUrl(), false });
    if (mods.momentum > 0.0)
        vessels.append(momentumEntry(qtTrId("skill_wheel_dialog_momentum_mod_grade_bonus_information")));
    vessels.append(revelations);
    QList<TPerkInfo> sortedAugmentations = augmentations;
    byLabel(sortedAugmentations);
    vessels.append(sortedAugmentations);
    QList<TPerkInfo> supremes;
    for (const auto& [mod, value] : mods.supremeStats)
        supremes.append({ supremeModName(mod, true, true), supremeModValueText(value, mod), {}, QUrl(), false });
    byLabel(supremes);
    vessels.append(supremes);
    // CIP : life, mana, capacity, mitigation, then by name.
    QList<TPerkInfo> stats;
    for (const auto& [stat, value] : mods.basicStats)
        if (stat > 8)
            stats.append({ basicStatName(stat, true), basicStatValueText(value, stat), basicStatInfo(stat), QUrl(), false });
    std::ranges::stable_sort(stats, [&](const TPerkInfo& a, const TPerkInfo& b) {
        const int ra = statRank(a.label), rb = statRank(b.label);
        return ra != rb ? ra < rb : a.label < b.label;
    });
    vessels.append(stats);
    vessels.append(resistances(resistanceValues));
    if (!hasData)
        vessels.clear();

    // CIP : groups joined by separators .
    QList<TPerkInfo> full;
    const auto appendGroup = [&full](const QList<TPerkInfo>& group) {
        if (group.isEmpty())
            return;
        if (!full.isEmpty())
            full.append({ {}, {}, {}, QUrl(), true });
        full.append(group);
    };
    QList<TPerkInfo> totals;
    if (const int damageHealing = bonus + mods.damageHealing; damageHealing > 0)
        totals.append({ qtTrId("damage_and_healing"), QStringLiteral("+%1").arg(englishNumber(damageHealing)), {}, QUrl(), false });
    if (mods.momentum > 0.0)
        totals.append(momentumEntry({}));
    appendGroup(totals);

    int mitigationPoints = 0;
    for (int slot = 0; slot < SlotCount; ++slot)
        if (static_cast<ESmallPerk>(gridConfiguration().smallPerk(slot, voc)) == ESmallPerk::Mitigation)
            mitigationPoints += wheel.points[slot];
    const std::pair<int, double> statTotals[] = {
        { 10, static_cast<int>(mods.basicStat(10)) + smallPerkTotal(wheel.points, ESmallPerk::Life, voc) },
        { 11, static_cast<int>(mods.basicStat(11)) + smallPerkTotal(wheel.points, ESmallPerk::Mana, voc) },
        { 12, static_cast<int>(mods.basicStat(12)) + smallPerkTotal(wheel.points, ESmallPerk::Capacity, voc) },
        { 9, mods.basicStat(9) + mitigationValue(mitigationPoints) },
    };
    QList<TPerkInfo> statGroup;
    for (const auto& [stat, value] : statTotals)
        statGroup.append({ basicStatName(stat, true), basicStatValueText(value, stat), basicStatInfo(stat), QUrl(), false });
    const std::pair<int, double> supremeTotals[] = {
        { 0, mods.supremeStat(0) },
        { 1, mods.supremeStat(1) },
        { 3, mods.supremeStat(3) + static_cast<double>(static_cast<float>(mediumPerkCount(wheel.points, 5, voc)) * 0.25f) },
        { 2, mods.supremeStat(2) + static_cast<double>(static_cast<float>(mediumPerkCount(wheel.points, 6, voc)) * 0.75f) },
    };
    QList<TPerkInfo> supremeGroup;
    for (const auto& [mod, value] : supremeTotals)
        if (std::fabs(value) > 1e-5)
            supremeGroup.append({ supremeModName(mod, true, true), supremeModValueText(value, mod), {}, QUrl(), false });
    byLabel(supremeGroup);
    statGroup.append(supremeGroup);
    appendGroup(statGroup);

    appendGroup(perkGroup);
    QList<TPerkInfo> augmentedAll = augmentedGroup;
    augmentedAll.append(augmentations);
    byLabel(augmentedAll);
    appendGroup(augmentedAll);
    QList<TPerkInfo> revelationGroup = perks;
    revelationGroup.append(revelations);
    appendGroup(revelationGroup);
    appendGroup(resistances(resistanceValues));
    appendGroup(masteryGroup);

    m_small->setItems({ smallEntries.begin(), smallEntries.end() });
    m_large->setItems({ large.begin(), large.end() });
    m_medium->setItems({ medium.begin(), medium.end() });
    m_vessels->setItems({ vessels.begin(), vessels.end() });
    m_full->setItems({ full.begin(), full.end() });
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
    if ((!m_storage || !m_storage->hasData()) || slot < 0 || slot >= SlotCount) {
        clearTile(info);
        return;
    }
    const TWheel& wheel = m_storage->currentWheel();
    const int voc = m_storage->vocationIndex();
    const int points = wheel.points[slot];
    const int max = gridConfiguration().maxPoints(slot);
    const ESmallPerk smallPerkId = static_cast<ESmallPerk>(gridConfiguration().smallPerk(slot, voc));
    const int medium = gridConfiguration().mediumPerk(slot, voc);
    info->setCurrentSkillPoints(points);
    info->setMaximumSkillPoints(max);
    // The grid alone decides: a full slice can be emptied, and a slice can be
    // filled, when the grid stays valid.
    info->setCanBeDecreased(canDecrease(wheel.points, slot));
    info->setCanBeIncreased(canIncrease(wheel.points, slot));
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
    info->setMediumPerkInfo(mediumPerkEffect(medium, gridConfiguration().largePerk(domainOfSlot(slot), voc)));
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
    if ((!m_storage || !m_storage->hasData()) || domain < 0 || domain >= DomainCount) {
        clearLargePerk(info);
        return;
    }
    const int voc = m_storage->vocationIndex();
    const int points = revelationPoints(m_storage->currentWheel(), m_storage->supremeGrades(), domain, voc);
    const int stage = gridConfiguration().revelationStage(points);
    info->setCurrentSkillPoints(points);
    info->setNextSkillPointThreshold(revelationThreshold(std::min(stage + 1, 3)));
    info->setCurrentLevel(stage);
    info->setCurrentLevelName(perkLevelName(stage));
    info->setPerkName(largePerkName(gridConfiguration().largePerk(domain, voc)));
    info->setShortInformation(largePerkShortInformation(gridConfiguration().largePerk(domain, voc)));
    info->setDetailedInformation(largePerkDetailedInformation(gridConfiguration().largePerk(domain, voc), stage));
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
    const int points = std::clamp(wheel.points[slot] + delta, 0, gridConfiguration().maxPoints(slot));
    if (points == wheel.points[slot])
        return;
    wheel.points[slot] = points;
    if (!isValid(wheel.points))
        return;
    m_storage->setCurrentSkillWheel(wheel);
}

/// CIP .
void TSkillWheelPageController::addToSkill(const int GridTileQML, const int KeyboardModifiers)
{
    if ((!m_storage || !m_storage->hasData()) || !canBeIncreased() || GridTileQML < 0 || GridTileQML >= SlotCount
        || !canIncrease(m_storage->currentWheel().points, GridTileQML))
        return;
    const int amount = std::min(stepSize(KeyboardModifiers), std::max(0, m_storage->freePoints()));
    changePoints(GridTileQML, amount);
}

/// CIP .
void TSkillWheelPageController::fillSkill(const int GridTileQML)
{
    if ((!m_storage || !m_storage->hasData()) || !canBeIncreased() || GridTileQML < 0 || GridTileQML >= SlotCount
        || !canIncrease(m_storage->currentWheel().points, GridTileQML))
        return;
    changePoints(GridTileQML, std::min(gridConfiguration().maxPoints(GridTileQML), std::max(0, m_storage->freePoints())));
}

/// CIP .
void TSkillWheelPageController::removeFromSkill(const int GridTileQML, const int KeyboardModifiers)
{
    if ((!m_storage || !m_storage->hasData()) || m_storage->changeState() != 1 || GridTileQML < 0 || GridTileQML >= SlotCount
        || !canDecrease(m_storage->currentWheel().points, GridTileQML))
        return;
    changePoints(GridTileQML, -std::min(stepSize(KeyboardModifiers), m_storage->currentWheel().points[GridTileQML]));
}

/// CIP .
void TSkillWheelPageController::clearSkill(const int GridTileQML)
{
    if ((!m_storage || !m_storage->hasData()) || m_storage->changeState() != 1 || GridTileQML < 0 || GridTileQML >= SlotCount
        || !canDecrease(m_storage->currentWheel().points, GridTileQML))
        return;
    changePoints(GridTileQML, -m_storage->currentWheel().points[GridTileQML]);
}

/// CIP : fill (any modifier: add one step) while points fit,
/// otherwise empty the slice.
void TSkillWheelPageController::onGridTileRightClicked(const int GridTileQML, const int KeyboardModifiers)
{
    if ((!m_storage || !m_storage->hasData()) || GridTileQML < 0 || GridTileQML >= SlotCount)
        return;
    const auto& points = m_storage->currentWheel().points;
    const int current = points[GridTileQML];
    const int max = gridConfiguration().maxPoints(GridTileQML);
    const int free = m_storage->freePoints();
    if (canIncrease(points, GridTileQML) && free > 0) {
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

/// CIP: the current wheel without points (its vessel gems stay).
void TSkillWheelPageController::resetSkillGrid()
{
    std::optional<TWheel> wheel = m_storage->currentSkillWheel();
    if (!wheel)
        return;
    wheel->points = {};
    m_storage->setCurrentSkillWheel(*wheel);
}

/// CIP: the current wheel's export code as COPY_TRUSTED_TEXT_TO_CLIPBOARD.
void TSkillWheelPageController::requestCopyExportCodeToClipboard()
{
    const std::optional<TWheel> wheel = m_storage->currentSkillWheel();
    if (!wheel)
        return;
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCopyTrustedTextToClipboard>(encodeWheelCode(*wheel)));
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
    // Host: no storage before the page knows a session.
    if (!m_storage)
        return;
    const std::optional<TWheel> wheel = m_storage->preset(PresetName);
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

void TSkillWheelPageController::requestRenamePresetClicked() { m_presetManagmentDialog->open(m_currentPresetName, EManageMode::Rename); }
void TSkillWheelPageController::requestNewPresetClicked()
{
    m_presetManagmentDialog->open(m_currentPresetName, EManageMode::NewEmptyPreset);
}
void TSkillWheelPageController::requestImportPresetClicked() { m_presetManagmentDialog->open(m_currentPresetName, EManageMode::ImportPreset); }

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
    if (ButtonIdentifier != Button::USER_DEFINED || (!m_storage || !m_storage->hasData()))
        return;
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCopyTrustedTextToClipboard>(m_plannerUrl + encodeWheelCode(m_storage->currentWheel())));
}

void TSkillWheelPageController::setShowFullSummary(const bool Show)
{
    if (Show == m_showFullSummary)
        return;
    m_showFullSummary = Show;
    Q_EMIT summaryModeChanged();
}

/// CIP: with a server wheel it becomes the current one (the dialog opens the wheel tab this way); the
/// presets are checked, their model refreshed, and the matching then the pending preset selected.
void TSkillWheelPageController::useServerSkillWheelAsCurrent()
{
    const std::optional<TWheel> server = m_storage->serverSkillWheel();
    if (!server)
        return;
    m_storage->setCurrentSkillWheel(*server);
    validatePresets();
    updatePresetsInfoModelIfNeeded();
    selectMatchingPreset();
    selectPendingPreset();
}

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

/// CIP: the presets are checked, then the new preset is shown.
void TSkillWheelPageController::onNewPresetName(const QString& PresetName)
{
    validatePresets();
    selectPresetByName(PresetName, true);
}

/// CIP: the current wheel goes back to the server's, the slices and corners, the selected and
/// hovered information and the totals summaries are emptied; then the presets are checked and the
/// matching (then the pending) preset is selected again.
void TSkillWheelPageController::clearData()
{
    if (m_storage)
        m_storage->resetCurrentSkillWheel();
    clearParameters();
    m_selectedSlot = m_hoveredSlot = -1;
    clearTile(m_selectedTile);
    clearTile(m_hoveredTile);
    m_selectedLargeDomain = m_hoveredLargeDomain = -1;
    clearLargePerk(m_selectedLarge);
    clearLargePerk(m_hoveredLarge);
    m_small->setItems({});
    m_large->setItems({});
    m_medium->setItems({});
    Q_EMIT dataChanged();
    validatePresets();
    updatePresetsInfoModelIfNeeded();
    selectMatchingPreset();
    selectPendingPreset();
}

/// CIP: the slice and corner objects go (dataChanged first, then they are deleted later).
void TSkillWheelPageController::clearParameters()
{
    if (m_skillParameters.isEmpty() && m_cornerParameters.isEmpty())
        return;
    const QList<QObject*> oldTiles = std::exchange(m_skillParameters, {});
    const QList<QObject*> oldCorners = std::exchange(m_cornerParameters, {});
    Q_EMIT dataChanged();
    for (QObject* o : oldTiles)
        if (o)
            o->deleteLater();
    for (QObject* o : oldCorners)
        if (o)
            o->deleteLater();
}

/// CIP: the message box, the export question and the preset management dialog close.
void TSkillWheelPageController::closeMessages()
{
    m_messageMode = NoMessage;
    m_message->dialog()->close();
    m_export->dialog()->close();
    m_presetManagmentDialog->dialog()->close();
}

/// CIP: a changed wheel asks before closing (mode 1), otherwise closeConfirmed.
void TSkillWheelPageController::requestClose()
{
    if (!m_storage->isCurrentSkillWheelChanged()) {
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

/// CIP: a valid current wheel goes into the current preset and to the server (SAVE_SKILL_WHEEL);
/// the next open of the own wheel is skipped.
void TSkillWheelPageController::apply()
{
    const std::optional<TWheel> current = m_storage->currentSkillWheel();
    if (!current || !isValid(current->points))
        return;
    Q_EMIT preapareToSkipNextOpenForOwnPlayer();
    m_storage->addOrReplacePreset(m_currentPresetName, *current);
    Q_EMIT publishGameAction(std::make_shared<input::gameactions::TGameActionSaveSkillWheel>(*current));
}

// ---------------------------------------------------------------------------
// Gem atelier

TGemAtelierPageController::TGemAtelierPageController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent)
    : cip::tibia::gamewindow::TGemAtelierPageController(parent)
    , m_dismantleConfirm(new gamewindow::TMessageDialogController(engine, dialogParentItem, this))
    , m_list(new TInventoryGemListModel(this))
    , m_filter(new TInventoryGemSortFilterProxyModel(m_list, this))
    , m_pages(new TPaginationProxyModel(15, this))
    , m_reshapeModDialog(new TGemAtelierReshapeModDialogController(engine, dialogParentItem, this))
{
    m_dismantleConfirm->setCloseOnButtonClicked(true);
    connect(m_dismantleConfirm, &gamewindow::TMessageDialogController::acceptClicked, this, [this] {
        if (TSkillWheelProtocolMessageHandler* handler = sessionSkillWheelHandler(m_gameSession))
            handler->dismantleGem(m_dismantleGemId);
    });
    m_pages->setSourceModel(m_filter);
    m_gemInventory = m_pages;
    m_maxRevealedGems = MaxRevealedGems;
    m_revealLesserGemCost = revealCost(0);
    m_revealRegularGemCost = revealCost(1);
    m_revealGreaterGemCost = revealCost(2);
    // The gold the atelier costs is the bank balance and the carried gold.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this, [this](qint64, qint64, const int type) {
        if (type == tibia::client::RESOURCE_BANK_BALANCE || type == tibia::client::RESOURCE_GOLD_EQUIPPED)
            onResourcesChanged();
    });
}

/// CIP: the reshape dialog and the dismantle question close, the session's storage replaces the
/// previous one: the wheels, the gem inventory and the mod grades refresh the vessels and the list,
/// the resources the reveal costs, a rolled reshape opens the reshape dialog; its confirmation
/// reshapes. Then the page shows the storage's gems and wheels.
void TGemAtelierPageController::connectToGameSession(std::shared_ptr<game::IGameSession> GameSession)
{
    closeMessageDialog();
    m_gameSession = std::move(GameSession);
    m_gameSessionConnections.disconnectAll();
    m_storage = sessionSkillWheelStorage(m_gameSession);
    if (!m_storage)
        return;
    const auto add = [this](const QMetaObject::Connection& connection) { m_gameSessionConnections.add(connection); };
    add(connect(m_storage, &TSkillWheelStorage::currentSkillWheelChanged, this, &TGemAtelierPageController::onWheelChanged));
    add(connect(m_storage, &TSkillWheelStorage::serverSkillWheelChanged, this, &TGemAtelierPageController::onWheelChanged));
    add(connect(m_storage, &TSkillWheelStorage::gemInventoryChanged, this, &TGemAtelierPageController::onGemsChanged));
    add(connect(m_storage, &TSkillWheelStorage::gemInventoryChanged, this, &TGemAtelierPageController::onWheelChanged));
    add(connect(m_storage, &TSkillWheelStorage::modGradesChanged, this, &TGemAtelierPageController::onWheelChanged));
    for (const auto signal : { &TSkillWheelStorage::unrevealedGemsChanged, &TSkillWheelStorage::lesserFragmentsChanged,
                               &TSkillWheelStorage::greaterFragmentsChanged })
        add(connect(m_storage, signal, this, &TGemAtelierPageController::onResourcesChanged));
    add(connect(m_storage, &TSkillWheelStorage::temporaryReshapedGemChanged, this, &TGemAtelierPageController::onTemporaryReshapedGemChanged));
    add(connect(m_reshapeModDialog, &TGemAtelierReshapeModDialogController::confirmReshape, this, &TGemAtelierPageController::onConfirmReshape));
    m_list->setGems(m_storage->gemInventory());
    m_list->setConfiguration(m_storage->currentSkillWheel(), m_storage->serverSkillWheel(), m_storage->basicGrades(),
                             m_storage->supremeGrades());
    updateSockets();
}

/// The storage's gems go to the list, the filters run again, the gem picked
/// before a socket change is selected again (or forgotten when it is gone).
void TGemAtelierPageController::onGemsChanged()
{
    m_list->setGems(m_storage->gemInventory());
    m_filter->invalidateRows();
    if (m_gemToReselect) {
        if (m_storage->gem(*m_gemToReselect))
            selectGem(*m_gemToReselect, false);
        else
            m_gemToReselect.reset();
    }
    Q_EMIT gemInventoryChanged();
}

/// With a current and a server wheel: the vessels are made again from the
/// current wheel, then the list learns the wheels and grades.
void TGemAtelierPageController::onWheelChanged()
{
    if (!m_storage->currentSkillWheel() || !m_storage->serverSkillWheel())
        return;
    updateSockets();
    // The list takes the wheels, the gems and the grades; it repaints only when one of them changed.
    m_list->setConfiguration(*m_storage->currentSkillWheel(), *m_storage->serverSkillWheel(), m_storage->gemInventory(),
                             m_storage->basicGrades(), m_storage->supremeGrades());
}

/// CipSoft: eight quality-typed vessels unlocked by Gem Vessel perks.
void TGemAtelierPageController::updateSockets()
{
    if (!m_storage)
        return;
    for (int v = 0; v < VesselCount; ++v) {
        const TWheel& wheel = m_storage->currentWheel();
        const std::optional<TGem>& gem = wheel.vessels[v];
        QObject* old = m_sockets[v];
        m_sockets[v] = new TQmlSocketedGem(v, wheel.vocation - 1, gem, this);
        if (old)
            old->deleteLater();
    }
    Q_EMIT vesselChanged();
}

/// The Gem Vessel perks of the current wheel; none without one.
int TGemAtelierPageController::activeVesselCount() const
{
    const std::optional<TWheel> wheel = m_storage->currentSkillWheel();
    if (!wheel)
        return 0;
    return static_cast<uint8_t>(mediumPerkCount(wheel->points, VesselNodePerk, m_storage->vocationIndex()));
}

/// One of the active vessels is empty while the wheel can be changed.
bool TGemAtelierPageController::showUnusedVesselsNotification() const
{
    const std::optional<TWheel> wheel = m_storage->currentSkillWheel();
    if (!wheel)
        return false;
    const auto active = static_cast<uint8_t>(mediumPerkCount(wheel->points, VesselNodePerk, m_storage->vocationIndex()));
    return hasEmptyVessel(*wheel, active) && canBeChanged();
}

QString TGemAtelierPageController::unrevealedLesserGemName() const { return itemName(unrevealedGemTypeId(m_storage->vocationIndex(), 0)); }
QString TGemAtelierPageController::unrevealedRegularGemName() const { return itemName(unrevealedGemTypeId(m_storage->vocationIndex(), 1)); }
QString TGemAtelierPageController::unrevealedGreaterGemName() const { return itemName(unrevealedGemTypeId(m_storage->vocationIndex(), 2)); }
int TGemAtelierPageController::unrevealedLesserGemAppearanceType() const { return unrevealedGemTypeId(m_storage->vocationIndex(), 0); }
int TGemAtelierPageController::unrevealedRegularGemAppearanceType() const { return unrevealedGemTypeId(m_storage->vocationIndex(), 1); }
int TGemAtelierPageController::unrevealedGreaterGemAppearanceType() const { return unrevealedGemTypeId(m_storage->vocationIndex(), 2); }

/// Vessels are part of the edited wheel: socketing changes the current wheel,
/// Apply sends it.
/// VesselIndex -1 takes the gem out of its vessel; a vessel index sockets it there, or takes it
/// out when it already is there. The gem is selected again once the inventory changes.
void TGemAtelierPageController::socketOrUnsocketGem(const int GemId, const int VesselIndex)
{
    std::optional<TWheel> wheel = m_storage->currentSkillWheel();
    if (!wheel)
        return;
    const std::optional<TGem> gem = m_storage->gem(GemId);
    if (!gem)
        return;
    const std::optional<int> socketedIn = vesselOfGem(*wheel, gem->id);
    if (VesselIndex == -1 && socketedIn) {
        unsocketGem(*wheel, *socketedIn);
    } else {
        const auto vessel = static_cast<uint8_t>(VesselIndex);
        if (vessel >= VesselCount)
            throw std::invalid_argument("TVessels::Index: value out of range");
        const std::optional<TGem>& occupant = wheel->vessels[vessel];
        if (socketedIn && *socketedIn == vessel && (!occupant || occupant->id == GemId))
            unsocketGem(*wheel, *socketedIn);
        else
            socketGem(*wheel, *gem, vessel);
    }
    m_gemToReselect = gem->id;
    m_storage->setCurrentSkillWheel(*wheel);
}

void TGemAtelierPageController::closeMessageDialog()
{
    m_reshapeModDialog->closeDialog();
    if (m_dismantleConfirm->dialog()->item())
        m_dismantleConfirm->dialog()->close();
}

/// The skill wheel dialog closing: first page, no filters, no vessel or gem selected.
void TGemAtelierPageController::clearData()
{
    m_pages->setCurrentPage(1);
    m_filter->resetFilters();
    m_selectedVesselIndex = -1;
    m_selectedGemIndex = QPersistentModelIndex();
}

void TGemAtelierPageController::resetVessels()
{
    std::optional<TWheel> wheel = m_storage->currentSkillWheel();
    if (!wheel)
        return;
    wheel->vessels = {};
    m_storage->setCurrentSkillWheel(*wheel);
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
void TGemAtelierPageController::revealGem(const int GemQuality)
{
    if (TSkillWheelProtocolMessageHandler* handler = sessionSkillWheelHandler(m_gameSession))
        handler->revealGem(GemQuality);
}
void TGemAtelierPageController::reshapeGem(const int GemId, const int ModIdx)
{
    if (!m_storage)
        return;
    m_storage->setReshapeModIdx(-1);
    m_storage->setReshapeGemId(-1);
    if (TSkillWheelProtocolMessageHandler* handler = sessionSkillWheelHandler(m_gameSession))
        handler->reshapeGem(GemId, ModIdx);
}

void TGemAtelierPageController::attemptReshapeGem(const int GemId, const int ModIdx)
{
    if (!m_storage)
        return;
    m_storage->setReshapeModIdx(ModIdx);
    m_storage->setReshapeGemId(GemId);
    m_storage->rememberGemModsBeforeReshape();
    if (TSkillWheelProtocolMessageHandler* handler = sessionSkillWheelHandler(m_gameSession))
        handler->attemptReshapeGem(GemId, ModIdx);
}

void TGemAtelierPageController::onTemporaryReshapedGemChanged()
{
    if (!m_storage)
        return;
    const std::optional<TWheel> wheel = m_storage->currentSkillWheel();
    const int vocationIndex = wheel ? transformCreatureToSkillWheelVocation(wheel->vocation) : 0;
    m_reshapeModDialog->setData(m_storage->gemModsBeforeReshape(), m_storage->temporaryReshapedGem(), m_storage->reshapeModIdx(),
                                vocationIndex, m_storage->basicGrades(), m_storage->supremeGrades());
    m_reshapeModDialog->showDialog();
}

void TGemAtelierPageController::onConfirmReshape()
{
    if (!m_storage)
        return;
    reshapeGem(m_storage->reshapeGemId(), m_storage->reshapeModIdx());
}
void TGemAtelierPageController::toggleGemLock(const int GemId)
{
    if (TSkillWheelProtocolMessageHandler* handler = sessionSkillWheelHandler(m_gameSession))
        handler->toggleGemLock(GemId);
}
/// CIP: an index beyond the active vessels selects none (-1).
void TGemAtelierPageController::selectVessel(const int Index)
{
    if (Index == m_selectedVesselIndex)
        return;
    m_selectedVesselIndex = Index < activeVesselCount() ? Index : -1;
    Q_EMIT selectedVesselIndexChanged();
}

/// A filled grade shows its backdrop, an unfilled one the empty backdrop.
QString TGemAtelierPageController::getModGradeImageSource(const int ModGrade, const bool IsFilled)
{
    return IsFilled ? modGradeImage(ModGrade) : modGradeImage(8);
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
    if (GemId < 0 || !m_list)
        return {};
    return m_list->dataForId(GemId);
}

void TGemAtelierPageController::selectGem(const int GemId) { selectGem(GemId, false); }

/// Shows the gem: with `NarrowDownFilters` the quality filter becomes the gem's; when the filters
/// still hide it, the mod name and locked-only filters are cleared. The gem's page opens.
void TGemAtelierPageController::selectGem(const int GemId, const bool NarrowDownFilters)
{
    const std::optional<TGem> gem = m_storage->gem(GemId);
    if (!gem)
        return;
    if (NarrowDownFilters)
        m_filter->setFilterGemQuality(hasBasic2(*gem) ? (hasSupreme(*gem) ? 2 : 1) : 0);
    QModelIndex filtered = m_filter->indexOfGem(GemId);
    if (!filtered.isValid()) {
        m_filter->setFilterModName(QString());
        m_filter->setShowLockedOnly(false);
        filtered = m_filter->indexOfGem(GemId);
    }
    setSelectedGemIndex(m_pages->showSourceRow(filtered.row()));
}

void TGemAtelierPageController::setSelectedGemIndex(const QPersistentModelIndex& Index)
{
    if (Index == m_selectedGemIndex)
        return;
    m_selectedGemIndex = Index;
    m_gemToReselect.reset();
    Q_EMIT selectedGemIndexChanged();
}

void TGemAtelierPageController::showDomain(const int /*domain*/, const int gemId)
{
    // Domain affinity removed — open atelier on the gem / first unlocked vessel.
    if (gemId >= 0) {
        if (const std::optional<int> vessel = vesselOfGem(m_storage->currentWheel(), gemId))
            selectVessel(*vessel);
        selectGem(gemId, true);
        return;
    }
    if (activeVesselCount() > 0)
        selectVessel(0);
}

void TGemAtelierPageController::selectModInFragmentWorkshop(const int ModId, const bool IsSupremeMod) { Q_EMIT requestWorkshopMod(!IsSupremeMod, ModId); }

// ---------------------------------------------------------------------------
// Fragment workshop

TFragmentWorkshopPageController::TFragmentWorkshopPageController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent)
    : cip::tibia::gamewindow::TFragmentWorkshopPageController(parent)
    , m_message(new gamewindow::TMessageDialogController(engine, dialogParentItem, this))
    , m_list(new TModListModel(this))
    , m_filter(new TModListSortFilterProxyModel(m_list, this))
    , m_pages(new TPaginationProxyModel(30, this))
{
    m_message->setCloseOnButtonClicked(true);
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
}

void TFragmentWorkshopPageController::closeMessageDialog() { m_message->dialog()->close(); }

/// CIP: the message box closes, the session's storage replaces the previous one: its mod grades refresh the mods, a new
/// server wheel the list's configuration (host: the storage's mod list also refreshes the mods).
void TFragmentWorkshopPageController::connectToGameSession(std::shared_ptr<game::IGameSession> GameSession)
{
    closeMessageDialog();
    m_gameSession = std::move(GameSession);
    m_gameSessionConnections.disconnectAll();
    m_storage = sessionSkillWheelStorage(m_gameSession);
    if (!m_storage)
        return;
    const auto add = [this](const QMetaObject::Connection& connection) { m_gameSessionConnections.add(connection); };
    add(connect(m_storage, &TSkillWheelStorage::modGradesChanged, this, &TFragmentWorkshopPageController::onModsChanged));
    add(connect(m_storage, &TSkillWheelStorage::serverSkillWheelChanged, this, &TFragmentWorkshopPageController::updateConfiguration));
    add(connect(m_storage, &TSkillWheelStorage::modListChanged, this, &TFragmentWorkshopPageController::onModsChanged));
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

/// New grades: the list and its configuration refresh; a mod enhanced under a
/// grade filter is selected again.
/// The skill wheel dialog closing: first page, no filters, no mod selected and
/// no mod waiting to be selected.
void TFragmentWorkshopPageController::clearData()
{
    m_pages->setCurrentPage(1);
    m_filter->resetFilters();
    m_filterMods = 0;
    m_selectedModIndex = QPersistentModelIndex();
    m_modToReselect.reset();
}

void TFragmentWorkshopPageController::onModsChanged()
{
    if (!m_storage)
        return;
    m_list->setMods(m_storage->modList());
    updateConfiguration();
    // The mod enhanced under a grade filter is selected again (the filter
    // may hide it now).
    if (m_modToReselect) {
        const auto [isBasicMod, modId] = std::visit(
            [](const auto& mod) {
                using TMod = std::decay_t<decltype(mod)>;
                return std::pair<bool, int>(std::is_same_v<TMod, TBasicModToReselect>, mod.id);
            },
            *m_modToReselect);
        selectMod(isBasicMod, modId);
        m_modToReselect.reset();
    }
    m_filter->invalidateRows();
    Q_EMIT modGradesChanged();
}

void TFragmentWorkshopPageController::updateConfiguration()
{
    if (!m_storage)
        return;
    if (const auto configuration = m_storage->currentSkillWheel())
        m_list->setConfiguration(*configuration, m_storage->basicGrades(), m_storage->supremeGrades());
}

void TFragmentWorkshopPageController::selectMod(const bool isBasicMod, const int ModID)
{
    const auto find = [this, isBasicMod, ModID] { return isBasicMod ? m_filter->indexOfBasicMod(ModID) : m_filter->indexOfSupremeMod(ModID); };
    QModelIndex filtered = find();
    if (!filtered.isValid()) {
        m_filter->setFilterModName(QString());
        setFilterMods(0);
        filtered = find();
    }
    if (filtered.isValid())
        setSelectedModIndex(m_pages->showSourceRow(filtered.row()));
}

void TFragmentWorkshopPageController::enhanceModGrade(const bool isBasicMod, const int ModID)
{
    if (TSkillWheelProtocolMessageHandler* handler = sessionSkillWheelHandler(m_gameSession))
        handler->enhanceModGrade(isBasicMod ? 0 : 1, ModID);
    // Under a grade filter (4..7) the enhanced mod may drop out of the list:
    // it is selected again when the new grades arrive.
    if ((m_filter->filterMods() & ~3) == 4) {
        if (isBasicMod)
            m_modToReselect = TBasicModToReselect{ ModID };
        else
            m_modToReselect = TSupremeModToReselect{ ModID };
    }
}

// ---------------------------------------------------------------------------
// Preset management dialog

namespace {

/// The local player's vocation as a wheel stores it (CIP: the session player's profession); none
/// without a player.
std::optional<int> playerWheelVocation()
{
    if (!tibia::game::hasLocalPlayer())
        return std::nullopt;
    return tibia::game::localPlayerVocation() % 10;
}

/// CIP: the code decodes and is of the current format, a vocation letter followed by version 1 (the
/// legacy two letter codes and version 0 codes are older formats).
bool isCurrentFormatWheelCode(const QString& code)
{
    if (!decodeWheelCode(code))
        return false;
    const QByteArray text = code.toLatin1();
    static constexpr const char* LegacyPrefixes[] = { "EK", "ED", "MS", "RP", "EM" };
    for (const char* prefix : LegacyPrefixes)
        if (text.startsWith(prefix))
            return false;
    static constexpr std::string_view Letters = "DKMPS";
    return !text.isEmpty() && Letters.find(text.front()) != std::string_view::npos && text.size() > 1 && text.at(1) == '1';
}

}

TSkillWheelPresetManagmentDialogController::TSkillWheelPresetManagmentDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem,
                                                                                       QObject* parent)
    : TDialogController(QStringLiteral("SkillWheelPresetManagmentDialog.qml"), engine, dialogParentItem, parent)
    , m_message(new TMessageDialogController(engine, dialogParentItem, this))
{
    m_message->setCloseOnButtonClicked(true);
}

TSkillWheelPresetManagmentDialogController::~TSkillWheelPresetManagmentDialogController() { clear(); }

void TSkillWheelPresetManagmentDialogController::setGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    m_storage = sessionSkillWheelStorage(m_gameSession);
}

void TSkillWheelPresetManagmentDialogController::clear()
{
    m_manageMode = EManageMode::Rename;
    m_currentPresetName.clear();
    m_newName.clear();
    m_importString.clear();
    m_nameIsValid = false;
    m_importStringError.clear();
}

/// CIP: the dialog is made again for the preset and mode.
void TSkillWheelPresetManagmentDialogController::open(const QString& presetName, const EManageMode mode)
{
    m_currentPresetName = presetName;
    m_manageMode = mode;
    m_newName = presetName.trimmed();
    if (const bool valid = isValidName(m_newName); valid != m_nameIsValid) {
        m_nameIsValid = valid;
        Q_EMIT changed();
    }
    setImportString(QString());
    dialog()->close();
    dialog()->create();
    dialog()->show();
}

bool TSkillWheelPresetManagmentDialogController::isValidName(const QString& name) const
{
    return !name.isEmpty() && (!m_storage || !m_storage->hasPreset(name));
}

void TSkillWheelPresetManagmentDialogController::setNewName(const QString& NewName)
{
    m_newName = NewName.trimmed();
    if (const bool valid = isValidName(m_newName); valid != m_nameIsValid) {
        m_nameIsValid = valid;
        Q_EMIT changed();
    }
}

/// CIP: the trimmed text after the last "&code=" (a planner link).
void TSkillWheelPresetManagmentDialogController::setImportString(const QString& ImportString)
{
    static const QString CodeParameter = QStringLiteral("&code=");
    m_importString = ImportString.trimmed();
    if (const qsizetype at = m_importString.lastIndexOf(CodeParameter); at >= 0)
        m_importString = m_importString.mid(at + CodeParameter.size());
    const QString error = importError(m_importString);
    if (error == m_importStringError)
        return;
    m_importStringError = error;
    Q_EMIT changed();
}

/// CIP: a code that does not decode or has an invalid grid; else a code of another vocation.
QString TSkillWheelPresetManagmentDialogController::importError(const QString& importString) const
{
    if (!m_storage)
        return {};
    const std::optional<int> vocation = playerWheelVocation();
    const std::optional<TWheel> code = decodeWheelCode(importString);
    if (!code || !isValidWheelCode(*code))
        return qtTrId("skill_wheel_manage_presets_invalid_import_string");
    if (code->vocation != vocation)
        return qtTrId("skill_wheel_manage_presets_vocation_mismatch");
    return {};
}

void TSkillWheelPresetManagmentDialogController::renamePreset()
{
    if (!m_storage || !isValidName(m_newName))
        return;
    if (m_storage->renamePreset(m_currentPresetName, m_newName))
        Q_EMIT newPresetName(m_newName);
}

/// CIP: an empty wheel of the player's vocation.
void TSkillWheelPresetManagmentDialogController::createEmptyPreset()
{
    if (!m_storage || !isValidName(m_newName))
        return;
    TWheel empty;
    empty.vocation = playerWheelVocation().value_or(0);
    m_storage->addOrReplacePreset(m_newName, empty);
}

void TSkillWheelPresetManagmentDialogController::copyPreset()
{
    if (!m_storage || !isValidName(m_newName))
        return;
    if (const std::optional<TWheel> wheel = m_storage->preset(m_currentPresetName))
        m_storage->addOrReplacePreset(m_newName, *wheel);
}

/// CIP: a code without error becomes the preset; a code of an older format is still imported and
/// the player is told.
void TSkillWheelPresetManagmentDialogController::importPreset()
{
    if (!m_storage || !isValidName(m_newName) || !importError(m_importString).isEmpty())
        return;
    if (const std::optional<TWheel> wheel = decodeWheelCode(m_importString))
        m_storage->addOrReplacePreset(m_newName, *wheel);
    if (!isCurrentFormatWheelCode(m_importString)) {
        m_message->setButtons(okDialogButtons());
        m_message->showMessage(qtTrId("info"), qtTrId("skill_wheel_dialog_imported_old_string"));
    }
}

/// CIP: the mode's change, then (for a valid new name, checked before the change) newPresetName;
/// the dialog closes.
void TSkillWheelPresetManagmentDialogController::onOkClicked(const EManageMode ManageMod)
{
    const bool valid = isValidName(m_newName);
    switch (ManageMod) {
        case EManageMode::Rename: renamePreset(); break;
        case EManageMode::NewEmptyPreset: createEmptyPreset(); break;
        case EManageMode::CopyPreset: copyPreset(); break;
        case EManageMode::ImportPreset: importPreset(); break;
    }
    if (valid)
        Q_EMIT newPresetName(m_newName);
    dialog()->close();
}

void TSkillWheelPresetManagmentDialogController::onCloseClicked() { dialog()->close(); }

// ---------------------------------------------------------------------------
// Dialog

TSkillWheelDialogController::TSkillWheelDialogController(const QString& plannerUrl, QQmlEngine* engine, QQuickItem* dialogParentItem,
                                                         QObject* parent)
    : TDialogController(QStringLiteral("SkillWheelDialog.qml"), engine, dialogParentItem, parent)
    , m_page(new TSkillWheelPageController(plannerUrl, engine, dialogParentItem, this))
    , m_atelier(new TGemAtelierPageController(engine, dialogParentItem, this))
    , m_workshop(new TFragmentWorkshopPageController(engine, dialogParentItem, this))
    , m_message(new TMessageDialogController(engine, dialogParentItem, this))
{
    m_wheelPageController = m_page;
    m_gemAtelierPageController = m_atelier;
    m_fragmentWorkshopPageController = m_workshop;
    m_message->setCloseOnButtonClicked(true);
    connect(m_message, &TMessageDialogController::acceptClicked, this, &TSkillWheelDialogController::onMessageDialogAcceptClicked);
    connect(m_message, &TMessageDialogController::discardClicked, this, &TSkillWheelDialogController::onMessageDialogDiscardClicked);
    m_skipNextOpenForOwnPlayerTimer.setInterval(1000);
    m_skipNextOpenForOwnPlayerTimer.setSingleShot(true);
    connect(&m_skipNextOpenForOwnPlayerTimer, &QTimer::timeout, this, &TSkillWheelDialogController::clearSkipNextOpenForOwnPlayer);
    connect(m_page, &TSkillWheelPageController::publishGameAction, this, &TSkillWheelDialogController::publishGameAction);
    connect(m_page, &TSkillWheelPageController::closeConfirmed, this, &TSkillWheelDialogController::closeDialog);
    connect(m_page, &TSkillWheelPageController::preapareToSkipNextOpenForOwnPlayer, this, &TSkillWheelDialogController::skipNextOpenForOwnPlayer);
    connect(m_page, &TSkillWheelPageController::requestShowGemAtelier, this, &TSkillWheelDialogController::onRequestShowGemAtelier);
    connect(m_page, &TSkillWheelPageController::requestShowFragmentWorkshop, this, &TSkillWheelDialogController::onRequestShowFragmentWorkshop);
    connect(m_atelier, &TGemAtelierPageController::publishGameAction, this, &TSkillWheelDialogController::publishGameAction);
    connect(m_workshop, &TFragmentWorkshopPageController::publishGameAction, this, &TSkillWheelDialogController::publishGameAction);
    // Host: the atelier's mod links open the workshop at the mod (CIP: the select mod game action).
    connect(m_atelier, &TGemAtelierPageController::requestWorkshopMod, this, [this](const bool isBasic, const int modId) {
        if (m_dialogTab != FragmentWorkshop)
            openDialogTab(FragmentWorkshop);
        m_workshop->selectMod(isBasic, modId);
    });
}

/// CIP: the dialog closes and the skipped open ends.
TSkillWheelDialogController::~TSkillWheelDialogController()
{
    closeDialog();
    clearSkipNextOpenForOwnPlayer();
}

/// CIP: a new session closes the dialog; the wheel page, the gem atelier and the fragment workshop
/// follow the session's skill wheel storage; the dialog keeps the session.
void TSkillWheelDialogController::connectToGameSession(std::shared_ptr<game::IGameSession> GameSession)
{
    closeDialog();
    m_page->connectToGameSession(GameSession);
    m_atelier->connectToGameSession(GameSession);
    m_workshop->connectToGameSession(GameSession);
    m_gameSession = std::move(GameSession);
}

TSkillWheelStorage* TSkillWheelDialogController::skillWheelStorage() const { return sessionSkillWheelStorage(m_gameSession); }

void TSkillWheelDialogController::setPremiumTimeStorage(game::TPremiumTimeStorage* PremiumTimeStorage)
{
    if (m_premiumTimeStorage)
        disconnect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this,
                   &TSkillWheelDialogController::premiumStatusChanged);
    m_premiumTimeStorage = PremiumTimeStorage;
    if (m_premiumTimeStorage)
        connect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this,
                &TSkillWheelDialogController::premiumStatusChanged);
}

void TSkillWheelDialogController::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
        case A::OPEN_OR_UPDATE_SKILL_WHEEL_DIALOG: openDialogTab(m_dialogTab == NoTabInitialState ? Wheel : m_dialogTab); return;
        case A::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE:
            m_messageMode = NoWheelMessage;
            m_message->setButtons(okDialogButtons());
            m_message->showMessage(qtTrId("info"), qtTrId("skill_wheel_warning_no_skill_wheel").arg(RequiredLevel));
            return;
        case A::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE:
            m_messageMode = PreviewPossibleMessage;
            m_message->setButtons(gamewindow::okCancelDialogButtons());
            m_message->showMessage(qtTrId("info"), qtTrId("skill_wheel_warning_no_skill_wheel").arg(RequiredLevel)
                                                       + qtTrId("skill_wheel_warning_no_skill_wheel_show_preview_part").arg(qtTrId("ok")));
            return;
        case A::REQUEST_OWN_SKILL_WHEEL: clearSkipNextOpenForOwnPlayer(); return;
        case A::GEM_ATELIER_SELECT_GEM:
            if (m_dialogTab != GemAtelier)
                openDialogTab(GemAtelier);
            if (const auto* select = input::gameActionAs<input::gameactions::TSelectAtelierGemGameAction>(pGameAction.get()))
                m_atelier->selectGem(static_cast<uint16_t>(select->gemId()), true);
            return;
        case A::FRAGMENT_WORKSHOP_SELECT_MOD:
            if (m_dialogTab != FragmentWorkshop)
                openDialogTab(FragmentWorkshop);
            if (const auto* select = input::gameActionAs<input::gameactions::TSelectFragmentWorkshopModGameAction>(pGameAction.get()))
                m_workshop->selectMod(select->supremeMod(), select->modId());
            return;
        default: return;
    }
}

/// CIP: the premium state of the session's player.
bool TSkillWheelDialogController::isPremium() const { return tibia::game::localPlayerIsPremium(); }

/// CIP: the message boxes of the dialog and its pages close; a dialog without its item is made and
/// shown.
void TSkillWheelDialogController::showDialog()
{
    m_message->dialog()->close();
    m_page->closeMessages();
    m_atelier->closeMessageDialog();
    m_workshop->closeMessageDialog();
    if (!dialog()->item()) {
        dialog()->create();
        dialog()->show();
    }
}

/// CIP: a dialog that is not in the stack is shown with no selection (unless the own wheel's open
/// after a save is skipped); the wheel tab takes the server wheel as the current one; then the tab.
void TSkillWheelDialogController::openDialogTab(const EDialogTab tab)
{
    const bool skipped = m_skipNextOpenForOwnPlayer && skillWheelStorage()->isOwnCharacter();
    if (!skipped && !dialog()->isInStack()) {
        showDialog();
        m_page->clearSelection();
    }
    if (tab == Wheel)
        m_page->useServerSkillWheelAsCurrent();
    switchTab(tab);
    clearSkipNextOpenForOwnPlayer();
}

/// CIP: the current tab's page hides and the new one shows (the wheel keeps its presets when the
/// dialog comes back to it from another tab); another value leaves no tab.
void TSkillWheelDialogController::switchTab(const EDialogTab tab)
{
    const auto page = [this](const EDialogTab t) -> cip::tibia::gamewindow::IDialogTabPage* {
        switch (t) {
            case Wheel: return m_page;
            case GemAtelier: return m_atelier;
            case FragmentWorkshop: return m_workshop;
            default: return nullptr;
        }
    };
    if (m_dialogTab == tab && page(tab))
        return;
    if (cip::tibia::gamewindow::IDialogTabPage* current = page(m_dialogTab))
        current->hideDialogTab();
    EDialogTab shown = NoTabInitialState;
    if (cip::tibia::gamewindow::IDialogTabPage* next = page(tab)) {
        if (tab == Wheel && m_dialogTab != NoTabInitialState)
            m_page->skipNextShowDialogTabUpdate();
        next->showDialogTab();
        shown = tab;
    }
    m_dialogTab = shown;
    Q_EMIT dialogTabChanged();
}

/// CIP: the message boxes close, the shown page hides, the dialog closes and the pages drop their
/// data; the session's server data goes and its current wheel is reset.
void TSkillWheelDialogController::closeDialog()
{
    m_message->dialog()->close();
    m_page->closeMessages();
    m_atelier->closeMessageDialog();
    m_workshop->closeMessageDialog();
    if (m_dialogTab == Wheel)
        m_page->hideDialogTab();
    else if (m_dialogTab == GemAtelier)
        m_atelier->hideDialogTab();
    else if (m_dialogTab == FragmentWorkshop)
        m_workshop->hideDialogTab();
    m_dialogTab = NoTabInitialState;
    Q_EMIT dialogTabChanged();
    dialog()->close();
    m_page->clearData();
    m_atelier->clearData();
    m_workshop->clearData();
    if (TSkillWheelStorage* storage = skillWheelStorage()) {
        storage->clearServerData();
        storage->resetCurrentSkillWheel();
    }
}

/// CIP: unsaved changes are asked about by the page (closeConfirmed closes), else the dialog closes.
void TSkillWheelDialogController::requestClose()
{
    if (!m_page->isWheelDirty()) {
        closeDialog();
        return;
    }
    m_page->requestClose();
}

void TSkillWheelDialogController::requestTabSwitch(const EDialogTab NewTab)
{
    if (NewTab == m_dialogTab)
        return;
    switchTab(NewTab);
}

void TSkillWheelDialogController::onOkClicked()
{
    m_page->requestSaveCurrentPreset();
    closeDialog();
}

void TSkillWheelDialogController::onApplyClicked() { m_page->requestSaveCurrentPreset(); }

/// CIP: the gem atelier empties the vessels, the wheel tab the grid.
void TSkillWheelDialogController::onResetClicked()
{
    if (m_dialogTab == GemAtelier)
        m_atelier->resetVessels();
    else if (m_dialogTab == Wheel)
        m_page->resetSkillGrid();
}

/// CIP: the store opens; the dialog closes (asking first about unsaved changes).
void TSkillWheelDialogController::getPremiumClicked()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenStore>(store::TStoreOpenParameters{.openAction = store::OPEN_PREMIUM_OR_BOOSTS, .premiumDetail = 0}));
    requestClose();
}

void TSkillWheelDialogController::requestCopyExportCodeToClipboarClicked() { m_page->requestCopyExportCodeToClipboard(); }

/// CIP: an empty wheel of the player's vocation (a placeholder name without a player) is shown
/// through the open action, and the resource balances are asked for.
void TSkillWheelDialogController::requestSkillWheelPreview()
{
    QString name = qtTrId("skill_wheel_dialog_name_dummy");
    int vocation = 1;
    if (tibia::game::hasLocalPlayer()) {
        name = QString::fromStdString(tibia::game::localPlayerName());
        vocation = tibia::game::localPlayerVocation() % 10;
    }
    skillWheelStorage()->setPreview(name, vocation);
    Q_EMIT publishGameAction(input::makeGameAction(A::OPEN_OR_UPDATE_SKILL_WHEEL_DIALOG));
    for (const int resource : { 1, 0, 0x54, 0x55 })
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionRequestResourceBalance>(resource));
}

/// CIP: OK on the preview question shows the preview.
void TSkillWheelDialogController::onMessageDialogAcceptClicked()
{
    if (m_messageMode == PreviewPossibleMessage)
        requestSkillWheelPreview();
}

/// CIP: the no wheel information closes the dialog.
void TSkillWheelDialogController::onMessageDialogDiscardClicked()
{
    if (m_messageMode == NoWheelMessage)
        closeDialog();
}

void TSkillWheelDialogController::onRequestShowGemAtelier() { openDialogTab(GemAtelier); }

/// CIP: the workshop tab; with both values given the workshop selects with them as CipSoft passes
/// them (whether the mod id is set as the first argument, the flag as the mod id).
void TSkillWheelDialogController::onRequestShowFragmentWorkshop(const std::optional<int> ModId, const std::optional<bool> IsBasicMod)
{
    openDialogTab(FragmentWorkshop);
    if (!ModId || !IsBasicMod)
        return;
    m_workshop->selectMod(*ModId != 0, *IsBasicMod ? 1 : 0);
}

/// CIP: the own wheel's next open is skipped for a second.
void TSkillWheelDialogController::skipNextOpenForOwnPlayer()
{
    m_skipNextOpenForOwnPlayer = true;
    m_skipNextOpenForOwnPlayerTimer.start();
}

void TSkillWheelDialogController::clearSkipNextOpenForOwnPlayer()
{
    m_skipNextOpenForOwnPlayerTimer.stop();
    m_skipNextOpenForOwnPlayer = false;
}

}
