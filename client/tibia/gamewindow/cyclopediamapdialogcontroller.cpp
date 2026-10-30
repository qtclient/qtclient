#include "cyclopediamapdialogcontroller.h"

#include "cyclopedia/cyclopediamapdata.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/messagedialogcontroller.h"
#include "mods/instance_backend.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/minimaprendererquickitem.h"
#include "worldmap/ambientlightstorage.h"

#include <client/const.h>
#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <QLocale>

#include <algorithm>
#include <cmath>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using SelectionType = cip::tibia::qmlcomponents::enums::ECyclopediaMapSelectionType;

constexpr int SubfieldsPerField = 32;
constexpr int GroundLayer = 7;
/// CIP 0x100d511d0: the hover delay before showTooltip.
constexpr int TooltipDelayMs = 500;

QString englishNumber(const qulonglong value) { return QLocale(QLocale::English).toString(value); }

qulonglong balance(const Otc::ResourceTypes_t type)
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? player->getResourceBalance(type) : 0;
}

/// Soft cyclopedia-map field menu (select / walk). GOD_TELEPORT is Shift+Ctrl only.
constexpr A SoftCyclopediaMapOrder[] = {
    A::CYCLOPEDIA_MAP_SELECT_ITEM,
    A::SET_PLAYER_MOVEMENT_INTENT,
};

}

TCyclopediaMapSelectionInfoArea::TCyclopediaMapSelectionInfoArea(const TCyclopediaMapAreaValues& values, QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaMapSelectionInfoArea(parent)
{
    m_selectionType = SelectionType::SelectionTypeArea;
    m_areaName = values.name;
    m_areaID = values.id;
    m_percentExplored = values.percentExplored;
    m_areaDonationString = values.donation;
    m_areaDonationThreshold = values.threshold;
    m_areaDonationsTooltip = values.donationsTooltip;
    m_percentUnlockedImprovedRespawn = values.percentUnlockedImprovedRespawn;
    m_rejectDonations = values.rejectDonations;
}

TCyclopediaMapSelectionInfoSubArea::TCyclopediaMapSelectionInfoSubArea(const TCyclopediaMapAreaValues& area, const Values& values, QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaMapSelectionInfoSubArea(parent)
{
    m_selectionType = SelectionType::SelectionTypeSubArea;
    m_areaName = area.name;
    m_areaID = area.id;
    m_percentExplored = area.percentExplored;
    m_areaDonationString = area.donation;
    m_areaDonationThreshold = area.threshold;
    m_areaDonationsTooltip = area.donationsTooltip;
    m_percentUnlockedImprovedRespawn = area.percentUnlockedImprovedRespawn;
    m_rejectDonations = area.rejectDonations;
    m_subAreaName = values.name;
    m_subAreaID = values.id;
    m_viewpointsCount = values.viewpointsCount;
    m_viewpointsFound = values.viewpointsFound;
    m_unlockable = values.unlockable;
    m_unlocked = values.unlocked;
    m_viewpointInfoDelayed = values.viewpointInfoDelayed;
    auto* occurrences = new TCyclopediaCreatureOccurrencesInfo(this);
    occurrences->setCreatures({ new TCyclopediaCreatureInfo(0, QString(), true, occurrences) }, {}, {});
    m_creatureOccurrences = occurrences;
}

TCyclopediaMapDialogController::TCyclopediaMapDialogController(QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaMapDialogController(parent)
    , m_storage(new cyclopedia::TCyclopediaMapStorage(this))
    , m_confirmDialog(new TMessageDialogController(this))
{
    input::setActionHandler(A::CYCLOPEDIA_MAP_SELECT_ITEM, [this](const input::TGameAction& action) {
        if (action.instance > 0)
            select(action.instance, false);
        return true;
    });

    m_tooltipTimer.setSingleShot(true);
    m_tooltipTimer.setInterval(TooltipDelayMs);
    connect(&m_tooltipTimer, &QTimer::timeout, this, &TCyclopediaMapDialogController::showTooltip);

    m_confirmDialog->setCloseOnButtonClicked(true);
    m_confirmDialog->setYesNoButtons();
    connect(m_confirmDialog, &TMessageDialogController::acceptClicked, this, [this] {
        if (m_resetSubAreaId > 0) {
            g_game.sendCyclopediaMapResetViewpoints(static_cast<uint16_t>(m_resetSubAreaId));
            m_resetSubAreaId = 0;
            return;
        }
        if (m_donationAreaId > 0 && m_donationAmount > 0)
            g_game.sendCyclopediaMapDonate(static_cast<uint16_t>(m_donationAreaId), static_cast<uint32_t>(m_donationAmount));
        m_donationAreaId = 0;
        m_donationAmount = 0;
    });
    // CIP DAT for exploration reward Go Strength shown in the info tooltip.
    m_explorationRewardAmountGoStrength = 25;

    // CIP 0x100d4eaf0 / 0x100d4eb40 / 0x100d4eb90: viewpoint, area and
    // donation changes rebuild the selection's info.
    connect(m_storage, &cyclopedia::TCyclopediaMapStorage::changed, this, [this] {
        rebuildSelection();
        updatePlayerArea();
    });

    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const std::vector<std::any>& args) {
        const auto* type = args.size() > 2 ? std::any_cast<Otc::ResourceTypes_t>(&args[2]) : nullptr;
        if (type && (*type == Otc::RESOURCE_BANK_BALANCE || *type == Otc::RESOURCE_GOLD_EQUIPPED)) {
            m_totalGoldBalance = static_cast<qlonglong>(balance(Otc::RESOURCE_BANK_BALANCE) + balance(Otc::RESOURCE_GOLD_EQUIPPED));
            Q_EMIT goldBalanceChanged();
        }
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const std::vector<std::any>&) {
        m_selectedId = 0;
        rebuildSelection();
        return std::any{};
    });
}

TCyclopediaMapDialogController::~TCyclopediaMapDialogController() = default;

void TCyclopediaMapDialogController::setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight)
{
    if (m_ambientLight)
        disconnect(m_ambientLight, nullptr, this, nullptr);
    m_ambientLight = ambientLight;
    if (ambientLight) {
        connect(ambientLight, &worldmap::TAmbientLightStorage::normalizedTibiaTimeOfDayWasChanged, this, &TCyclopediaMapDialogController::updateTimeDisplay);
        connect(ambientLight, &worldmap::TAmbientLightStorage::interpolatedTibiaTimeOfDayChanged, this, &TCyclopediaMapDialogController::updateTimeDisplay);
    }
    updateTimeDisplay();
}

void TCyclopediaMapDialogController::setInstanceStorage(mods::InstanceBackend* instance) { m_instance = instance; }

// CIP 0x100d4eab0.
void TCyclopediaMapDialogController::updateTimeDisplay()
{
    if (!m_ambientLight)
        return;
    m_timeOfDayNormalized = m_ambientLight->normalizedTibiaTimeOfDay();
    Q_EMIT timeOfDayChanged();
}

void TCyclopediaMapDialogController::showDialogTab()
{
    m_totalGoldBalance = static_cast<qlonglong>(balance(Otc::RESOURCE_BANK_BALANCE) + balance(Otc::RESOURCE_GOLD_EQUIPPED));
    Q_EMIT goldBalanceChanged();
    updateTimeDisplay();
    updatePlayerArea();
    if (m_selectedId == 0 && m_instance) {
        const int subareaId = m_instance->subareaId();
        if (subareaId > 0 && cyclopedia::TCyclopediaMapStaticData::instance().area(subareaId))
            select(subareaId, false);
    }
}

void TCyclopediaMapDialogController::hideDialogTab()
{
    m_tooltipTimer.stop();
    setTooltip({});
}

void TCyclopediaMapDialogController::onSelectedItemChanged()
{
    if (!m_renderer)
        return;
    std::set<int> highlighted;
    if (m_selectedId > 0) {
        if (!m_selectedIsArea) {
            highlighted.insert(m_selectedId);
        } else if (const auto* area = cyclopedia::TCyclopediaMapStaticData::instance().area(m_selectedId)) {
            highlighted.insert(area->subareaIds.begin(), area->subareaIds.end());
        }
    }
    // Explore overlay: keep the active discovery / exploring subarea filled.
    if (m_renderer->showExploreOverlay()) {
        if (const int exploring = m_storage->exploringSubarea(); exploring > 0)
            highlighted.insert(exploring);
        if (const int discovery = m_storage->discoverySubarea(); discovery > 0)
            highlighted.insert(discovery);
    }
    m_renderer->setHighlightedSubareas(highlighted);
}

void TCyclopediaMapDialogController::setMinimapRendererQuickItem(QObject* pItem)
{
    if (m_renderer)
        disconnect(m_renderer, nullptr, this, nullptr);
    m_renderer = qobject_cast<qmlcomponents::TMinimapRendererQuickItem*>(pItem);
    if (!m_renderer)
        return;
    onSelectedItemChanged();
    connect(m_renderer, &qmlcomponents::TMinimapRendererQuickItem::areaClicked, this, &TCyclopediaMapDialogController::onAreaClicked);
    m_renderer->setStorage(m_storage);
    // House pins: g_things.getCyclopediaHouses() (staticdata.dat).
    // Passage pins: 0xDD subtype 6 → TCyclopediaMapStorage::passages().
    if (m_pendingCenter) {
        const auto [position, scale] = *m_pendingCenter;
        m_pendingCenter.reset();
        centerAt(position, scale);
    } else {
        centerToPlayer();
    }
}

void TCyclopediaMapDialogController::hoverAtPixelCoordinate(const int x, const int y)
{
    m_tooltipTimer.stop();
    const QString tooltip = m_renderer ? m_renderer->markerTooltipAt(QPointF(x, y)) : QString();
    if (!tooltip.isEmpty())
        m_tooltipTimer.start();
    setTooltip(tooltip);
}

void TCyclopediaMapDialogController::onClickAtPixelCoordinate(const int x, const int y, const int TriggeringMouseButton, const Qt::KeyboardModifiers Modifiers)
{
    if (!m_renderer)
        return;
    qmlcomponents::TWorldMapCamera* camera = m_renderer->worldCamera();
    const QPoint field = camera->fieldAt(QPointF(x, y));
    const int layer = camera->layer();
    const Position position(field.x(), field.y(), static_cast<uint8_t>(layer));
    const int subareaId = layer <= GroundLayer ? cyclopedia::TCyclopediaMapStaticData::instance().subareaAt(field.x(), field.y(), layer) : 0;

    input::TGameActionList candidates;
    if (subareaId > 0) {
        candidates.push_back(std::make_shared<input::TGameActionCyclopediaMapSelectionItem>(subareaId));
    }
    // Cyclopedia Shift+click: Soft fog (0xfe) is passable — longer-range than minimap Soft.
    candidates.push_back(input::makeMovementIntent(position, Otc::AutoWalkPathCyclopedia));
    // CIP: GOD_TELEPORT is a mapper candidate (Shift+Ctrl); Soft menus omit it.
    candidates.push_back(input::makeObjectAction(A::GOD_TELEPORT, nullptr, position, -1));
    candidates.push_back(input::makeAction(A::CONTEXT_MENU));

    const input::TGameActionPtr action = m_mapper.map(candidates, 9, static_cast<Qt::MouseButton>(TriggeringMouseButton), static_cast<int>(Modifiers));
    if (!action)
        return;
    if (action->type == A::CONTEXT_MENU) {
        if (TContextMenuController* menu = TContextMenuController::instance()) {
            if (const input::TGameActionList entries = input::contextMenuActions(candidates, SoftCyclopediaMapOrder); !entries.empty())
                menu->showGameActions(entries, 9, m_mapper);
        }
        return;
    }
    if (action->type == A::CYCLOPEDIA_MAP_SELECT_ITEM) {
        select(action->instance, false);
        return;
    }
    input::publish(action);
}

void TCyclopediaMapDialogController::centerToPlayer()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (!player || !m_renderer)
        return;
    const Position position = player->getPosition();
    if (!position.isValid())
        return;
    m_renderer->worldCamera()->centerDelayed(position.x * SubfieldsPerField + SubfieldsPerField / 2, position.y * SubfieldsPerField + SubfieldsPerField / 2, position.z);
}

void TCyclopediaMapDialogController::centerAt(const Position& position, const float scale)
{
    if (!m_renderer) {
        m_pendingCenter.emplace(position, scale);
        return;
    }
    qmlcomponents::TWorldMapCamera* camera = m_renderer->worldCamera();
    if (!std::isnan(scale)) {
        Q_EMIT requestSetScaleFactor(scale);
        camera->setScaleFactor(scale);
    }
    camera->centerDelayed(position.x * SubfieldsPerField + SubfieldsPerField / 2, position.y * SubfieldsPerField + SubfieldsPerField / 2, position.z);
}

void TCyclopediaMapDialogController::donateMoneyForArea(const int AreaID, const int Value)
{
    const auto* area = cyclopedia::TCyclopediaMapStaticData::instance().area(AreaID);
    if (!area || Value <= 0)
        return;
    m_resetSubAreaId = 0;
    m_donationAreaId = AreaID;
    m_donationAmount = Value;
    m_confirmDialog->showMessage(qtTrId("cyclopediamap_confirm_donate_caption"),
                                 qtTrId("cyclopediamap_confirm_donate_area").arg(area->name).arg(englishNumber(static_cast<qulonglong>(Value))));
}

void TCyclopediaMapDialogController::confirmResetViewpoints(const int subAreaId)
{
    const auto* area = cyclopedia::TCyclopediaMapStaticData::instance().area(subAreaId);
    if (!area)
        return;
    m_donationAreaId = 0;
    m_donationAmount = 0;
    m_resetSubAreaId = subAreaId;
    m_confirmDialog->showMessage(qtTrId("cyclopediamap_confirm_reset_viewpoints_caption"),
                                 qtTrId("cyclopediamap_confirm_reset_viewpoints").arg(area->name));
}

void TCyclopediaMapDialogController::requestResetViewpoints()
{
    if (m_selectedId > 0 && !m_selectedIsArea)
        confirmResetViewpoints(m_selectedId);
}

void TCyclopediaMapDialogController::resetSubAreaViewPoints(const int SubAreaID)
{
    if (SubAreaID > 0)
        confirmResetViewpoints(SubAreaID);
}

void TCyclopediaMapDialogController::onCreatureClicked(const int RaceIDRaw)
{
    if (RaceIDRaw <= 0)
        return;
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    action->parameter = 1;
    action->category = RaceIDRaw;
    input::publish(action);
}

void TCyclopediaMapDialogController::onMouseExited()
{
    m_tooltipTimer.stop();
    setTooltip({});
}

void TCyclopediaMapDialogController::onAreaClicked(const int id, const bool isArea) { select(id, isArea); }

void TCyclopediaMapDialogController::select(const int id, const bool isArea)
{
    m_selectedId = id;
    m_selectedIsArea = isArea;
    m_storage->setSelectedId(id);
    rebuildSelection();
}

void TCyclopediaMapDialogController::rebuildSelection()
{
    auto& data = cyclopedia::TCyclopediaMapStaticData::instance();
    cip::tibia::gamewindow::ICyclopediaMapSelectionInfo* previous = m_selectedItem;
    m_selectedItem = nullptr;
    if (const auto* selected = m_selectedId > 0 ? data.area(m_selectedId) : nullptr) {
        if (m_selectedIsArea || !selected->isSubarea) {
            m_selectedItem = new TCyclopediaMapSelectionInfoArea(areaValues(selected->id), this);
        } else if (selected->name.contains(QLatin1String("Passage"), Qt::CaseInsensitive)) {
            auto* passage = new TCyclopediaMapSelectionInfoPassage(this);
            passage->setReachableCreatureOccurrences({});
            m_selectedItem = passage;
        } else {
            TCyclopediaMapSelectionInfoSubArea::Values values;
            values.name = selected->name;
            values.id = selected->id;
            values.unlocked = m_storage->isDiscovered(selected->id);
            values.unlockable = m_storage->isDiscoverable(selected->id);
            if (values.unlocked || m_storage->isAreaCompleted(selected->parentId)) {
                // CIP completed subarea: full set found (poiTarget defaults to 7).
                const int target = m_storage->poiTarget() > 0 ? m_storage->poiTarget() : 7;
                values.viewpointsCount = target;
                values.viewpointsFound = target;
            } else if (selected->id == m_storage->discoverySubarea()) {
                // CIP: denominator is poiTarget; numerator is viewpoints with state ≠ 0.
                const auto& points = m_storage->viewpoints();
                values.viewpointsCount = m_storage->poiTarget() > 0 ? m_storage->poiTarget()
                                                                    : static_cast<int>(points.size());
                values.viewpointsFound = static_cast<int>(std::count_if(points.begin(), points.end(), [](const auto& point) {
                    return point.state != 0;
                }));
            } else {
                values.viewpointInfoDelayed = true;
            }
            m_selectedItem = new TCyclopediaMapSelectionInfoSubArea(areaValues(selected->parentId), values, this);
        }
    }
    onSelectedItemChanged();
    Q_EMIT selectedItemChanged();
    m_currentExploringSubAreaID = m_storage->exploringSubarea();
    Q_EMIT currentExploringSubAreaIDChanged();
    if (previous)
        previous->deleteLater();
}

TCyclopediaMapAreaValues TCyclopediaMapDialogController::areaValues(const int areaId) const
{
    TCyclopediaMapAreaValues values;
    const auto* area = cyclopedia::TCyclopediaMapStaticData::instance().area(areaId);
    if (!area)
        return values;
    values.name = area->name;
    values.id = area->id;
    const auto* mainArea = m_storage->mainArea(areaId);
    values.percentExplored = mainArea ? mainArea->progress : 0;
    const quint64 donated = m_storage->donated(areaId);
    const quint64 goal = m_storage->donationGoal();
    values.donation = englishNumber(donated);
    values.threshold = englishNumber(goal);
    values.donationsTooltip = donationsTooltip();
    values.percentUnlockedImprovedRespawn = goal ? std::min(static_cast<double>(donated) / static_cast<double>(goal), 1.0) : 0.0;
    values.rejectDonations = area->rejectDonations;
    return values;
}

QString TCyclopediaMapDialogController::donationsTooltip() const
{
    auto& data = cyclopedia::TCyclopediaMapStaticData::instance();
    std::vector<std::pair<quint64, QString>> rows;
    for (const auto& area : data.areas()) {
        if (area.isSubarea)
            continue;
        if (const quint64 donated = m_storage->donated(area.id); donated > 0)
            rows.emplace_back(donated, area.name);
    }
    if (rows.empty())
        return qtTrId("area_value_tooltip_no_values");
    std::stable_sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    QString html = QStringLiteral("<table><tr><th colspan=2 align=center>") + qtTrId("area_value_tooltip") + QStringLiteral("</th></tr>");
    for (const auto& [donated, name] : rows)
        html += QStringLiteral("<tr><td>") + name + QStringLiteral("</td><td align=right>") + englishNumber(donated) + QStringLiteral("</td></tr>");
    html += QStringLiteral("</table>");
    return html;
}

void TCyclopediaMapDialogController::updatePlayerArea()
{
    const int areaId = m_storage->currentArea();
    const auto* area = areaId > 0 ? cyclopedia::TCyclopediaMapStaticData::instance().area(areaId) : nullptr;
    const QString name = area ? area->name : QString();
    if (m_currentPlayerArea != areaId || m_currentPlayerAreaName != name) {
        m_currentPlayerArea = areaId;
        m_currentPlayerAreaName = name;
        Q_EMIT currentPlayerAreaChanged();
    }
}

void TCyclopediaMapDialogController::setTooltip(const QString& tooltip)
{
    if (m_currentTooltip == tooltip)
        return;
    m_currentTooltip = tooltip;
    Q_EMIT currentTooltipChanged();
}

}
