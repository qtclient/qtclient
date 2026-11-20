#include "cyclopediamapdialogcontroller.h"

#include "cyclopedia/cyclopediaciptypes.h"
#include "cyclopedia/cyclopediamapcharacterconfigurationstorage.h"
#include "cyclopedia/cyclopediainterfaces.h"
#include "cyclopedia/cyclopediamapdata.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameaction.h"
#include "minimap/minimapmarkerstorage.h"
#include "mods/instance_backend.h"
#include "qmlcomponents/minimaprendererquickitem.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/coordinates.h"
#include "worldmap/worldmapgameactionprovider.h"

#include <QCursor>
#include <QGuiApplication>
#include <QLocale>

#include <client/const.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/gamesession_forward.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Cursor = TMouseCursorController;
using SelectionType = cip::tibia::qmlcomponents::enums::ECyclopediaMapSelectionType;
using StaticData = cyclopedia::TCyclopediaMapStaticDataStorage;

constexpr int SubfieldsPerField = 32;
constexpr int GroundLayer = 7;
/// The hover delay before the tooltip shows.
constexpr int TooltipDelayMs = 500;
/// How often the cursor follows the action under the mouse.
constexpr int MouseCursorIntervalMs = 50;
/// The rule context of the Cyclopedia map.
constexpr int MapRuleContext = 9;
/// The furthest a walk on the map may go, per axis.
constexpr int MaxWalkDistance = 110;
/// A subarea that is unlocked, or whose area is completed, shows all of its
/// viewpoints found.
constexpr int CompletedViewpoints = 7;
/// The Cyclopedia tab of the houses (TGameActionOpenCyclopedia).
constexpr int HousesTab = 5;
/// The confirmation's kind.
constexpr int ConfirmResetViewpoints = 0;
constexpr int ConfirmDonation = 1;

/// The context menu of the map offers the marker actions only.
constexpr A ContextMenuOrder[] = {
    A::SHOW_DIALOG_CREATE_MINIMAP_MARKER,
    A::SHOW_DIALOG_EDIT_MINIMAP_MARKER,
    A::DELETE_MINIMAP_MARKER,
};

QString englishNumber(const qulonglong value) { return QLocale(QLocale::English).toString(value); }

qulonglong balance(const tibia::client::ResourceTypes_t type)
{
    return tibia::game::hasLocalPlayer() ? tibia::game::localPlayerResourceBalance(type) : 0;
}

/// Donated over the goal, at most 1; 0 without a goal.
double unlockedImprovedRespawn(const TCyclopediaMapAreaValues& area)
{
    if (area.donationGoal == 0)
        return 0.0;
    return std::min(1.0, double(area.donated) / double(area.donationGoal));
}

}

TCyclopediaMapSelectionInfoArea::TCyclopediaMapSelectionInfoArea(const TCyclopediaMapAreaValues& values, QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaMapSelectionInfoArea(parent)
    , m_data(values)
{
    m_selectionType = SelectionType::SelectionTypeArea;
}

void TCyclopediaMapSelectionInfoArea::setAreaData(const TCyclopediaMapAreaValues& values)
{
    m_data = values;
    Q_EMIT infoChanged();
}

QString TCyclopediaMapSelectionInfoArea::areaDonationString() const { return englishNumber(m_data.donated); }

QString TCyclopediaMapSelectionInfoArea::areaDonationThreshold() const { return englishNumber(m_data.donationGoal); }

double TCyclopediaMapSelectionInfoArea::percentUnlockedImprovedRespawn() const { return unlockedImprovedRespawn(m_data); }

TCyclopediaMapSelectionInfoSubArea::TCyclopediaMapSelectionInfoSubArea(const Values& values, QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaMapSelectionInfoSubArea(parent)
{
    m_selectionType = SelectionType::SelectionTypeSubArea;
    setSubAreaData(values);
}

void TCyclopediaMapSelectionInfoSubArea::setSubAreaData(const Values& values)
{
    m_data = values;
    m_subAreaID = values.id;
    m_subAreaName = values.name;
    m_viewpointsCount = values.viewpointsCount;
    m_viewpointsFound = values.viewpointsFound;
    m_unlockable = values.unlockable;
    m_unlocked = values.unlocked;
    m_viewpointInfoDelayed = values.viewpointInfoDelayed;
    m_areaName = values.area.name;
    m_areaID = values.area.id;
    m_percentExplored = values.area.percentExplored;
    m_areaDonationString = englishNumber(values.area.donated);
    m_areaDonationThreshold = englishNumber(values.area.donationGoal);
    m_areaDonationsTooltip = values.area.donationsTooltip;
    m_percentUnlockedImprovedRespawn = unlockedImprovedRespawn(values.area);
    m_rejectDonations = values.area.rejectDonations;
    Q_EMIT cip::tibia::gamewindow::TCyclopediaMapSelectionInfoArea::infoChanged();
    QObject* previous = m_creatureOccurrences;
    m_creatureOccurrences = new TCyclopediaCreatureOccurrencesInfo(values.creatureOccurrences, this);
    Q_EMIT cip::tibia::gamewindow::TCyclopediaMapSelectionInfoSubArea::infoChanged();
    if (previous)
        previous->deleteLater();
}

TCyclopediaMapSelectionInfoPassage::TCyclopediaMapSelectionInfoPassage(const Values& values, QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaMapSelectionInfoPassage(parent)
{
    m_selectionType = SelectionType::SelectionTypePassage;
    setPassageData(values);
}

void TCyclopediaMapSelectionInfoPassage::setPassageData(const Values& values)
{
    m_data = values;
    const QList<QObject*> previous = m_reachableCreatureOccurrences;
    m_reachableCreatureOccurrences.clear();
    for (const auto& [subAreaId, occurrences] : m_data.creatureOccurrences)
        m_reachableCreatureOccurrences.append(new TCyclopediaCreatureOccurrencesInfo(occurrences, this));
    Q_EMIT infoChanged();
    for (QObject* occurrences : previous)
        occurrences->deleteLater();
}

TCyclopediaMapDialogController::TCyclopediaMapDialogController(QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaMapDialogController(parent)
    , m_configuration(new cyclopedia::TCyclopediaMapCharacterConfigurationStorage(this))
    , m_storage(new cyclopedia::TCyclopediaMapStorage(this))
    , m_confirmDialog(new TMessageDialogController(this))
{
    // Tip ICyclopediaMapSelectionInfo twin (selectionType).
    (void)tipICyclopediaMapSelectionInfo();

    m_confirmDialog->setCloseOnButtonClicked(true);
    m_confirmDialog->setYesNoButtons();
    connect(m_confirmDialog, &TMessageDialogController::acceptClicked, this, &TCyclopediaMapDialogController::onConfirmDialogAccepted);

    m_tooltipTimer.setInterval(TooltipDelayMs);
    m_tooltipTimer.setSingleShot(true);
    connect(&m_tooltipTimer, &QTimer::timeout, this, &TCyclopediaMapDialogController::onTooltipTimerTimeout);
    m_mouseCursorTimer.setInterval(MouseCursorIntervalMs);
    connect(&m_mouseCursorTimer, &QTimer::timeout, this, &TCyclopediaMapDialogController::onMouseCursorTimerTimeout);

    // The session's map storage.
    connect(m_storage, &cyclopedia::TCyclopediaMapStorage::viewpointsChanged, this, &TCyclopediaMapDialogController::refreshSelection);
    connect(m_storage, &cyclopedia::TCyclopediaMapStorage::areaAndSubAreaDataChanged, this, &TCyclopediaMapDialogController::refreshSelection);
    connect(m_storage, &cyclopedia::TCyclopediaMapStorage::areaDonationsChanged, this, &TCyclopediaMapDialogController::refreshSelection);
    connect(m_storage, &cyclopedia::TCyclopediaMapStorage::playerAreaChanged, this,
            &cip::tibia::gamewindow::TCyclopediaMapDialogController::currentPlayerAreaChanged);
    connect(m_storage, &cyclopedia::TCyclopediaMapStorage::playerPositionChanged, this,
            &cip::tibia::gamewindow::TCyclopediaMapDialogController::currentPlayerSubAreaIDChanged);
    connect(m_storage, &cyclopedia::TCyclopediaMapStorage::explorationRewardAmountGoStrengthChanged, this,
            &cip::tibia::gamewindow::TCyclopediaMapDialogController::explorationRewardAmountGoStrengthChanged);
    // The player's gold.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this, [this](qint64, qint64, int type) {
        if (type == static_cast<int>(tibia::client::RESOURCE_BANK_BALANCE) || type == static_cast<int>(tibia::client::RESOURCE_GOLD_EQUIPPED))
            Q_EMIT goldBalanceChanged();
    });
    // Host: CIP makes the controller per game session; qtclient's outlives it.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_selection = TNoSelection{};
        refreshSelection();
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
        updateTimeDisplay();
    }
}

void TCyclopediaMapDialogController::setInstanceStorage(mods::InstanceBackend* instance) { m_instance = instance; }

void TCyclopediaMapDialogController::updateTimeDisplay()
{
    if (!m_ambientLight)
        return;
    m_timeOfDayNormalized = m_ambientLight->normalizedTibiaTimeOfDay();
    Q_EMIT timeOfDayChanged();
}

bool TCyclopediaMapDialogController::showMapMarkerPanel() const { return m_configuration->showDisplay(); }

void TCyclopediaMapDialogController::setShowMapMarkerPanel(const bool v)
{
    if (m_configuration->showDisplay() == v)
        return;
    m_configuration->setShowDisplay(v);
    Q_EMIT showMapMarkerPanelChanged();
}

bool TCyclopediaMapDialogController::showNavigationPanel() const { return m_configuration->showNavigation(); }

void TCyclopediaMapDialogController::setShowNavigationPanel(const bool v)
{
    if (m_configuration->showNavigation() == v)
        return;
    m_configuration->setShowNavigation(v);
    Q_EMIT showNavigationPanelChanged();
}

bool TCyclopediaMapDialogController::showAreaAndSubAreaInfoPanel() const { return m_configuration->showAreaAndSubAreaInfo(); }

void TCyclopediaMapDialogController::setShowAreaAndSubAreaInfoPanel(const bool v)
{
    if (m_configuration->showAreaAndSubAreaInfo() == v)
        return;
    m_configuration->setShowAreaAndSubAreaInfo(v);
    Q_EMIT showAreaAndSubAreaInfoPanelChanged();
}

qlonglong TCyclopediaMapDialogController::totalGoldBalance() const
{
    if (!tibia::game::hasLocalPlayer())
        return 0;
    return static_cast<qlonglong>(balance(tibia::client::RESOURCE_BANK_BALANCE) + balance(tibia::client::RESOURCE_GOLD_EQUIPPED));
}

int TCyclopediaMapDialogController::currentPlayerArea() const { return m_storage->currentArea(); }

QString TCyclopediaMapDialogController::currentPlayerAreaName() const
{
    if (const int areaId = m_storage->currentArea(); areaId != 0)
        return StaticData::instance().areaName(static_cast<quint16>(areaId));
    return QString();
}

int TCyclopediaMapDialogController::currentExploringSubAreaID() const { return m_storage->exploringSubarea(); }

int TCyclopediaMapDialogController::currentPlayerSubAreaID() const
{
    return StaticData::instance().subAreaAt(m_storage->playerPosition());
}

int TCyclopediaMapDialogController::explorationRewardAmountGoStrength() const { return m_storage->explorationRewardAmountGoStrength(); }

void TCyclopediaMapDialogController::showDialogTab()
{
    const auto& data = StaticData::instance();
    const cyclopedia::TCyclopediaMapCharacterConfigurationStorage::TLocation& location = m_configuration->location();
    const auto* areaId = std::get_if<uint16_t>(&location);
    if (areaId && *areaId == 0) {
        // mod: an instance's subarea when the character has nothing selected.
        if (m_instance) {
            const int subareaId = m_instance->subareaId();
            if (subareaId > 0 && data.hasSubArea(static_cast<quint16>(subareaId))) {
                selectSubArea(static_cast<quint16>(subareaId));
                Q_EMIT showMapMarkerPanelChanged();
                Q_EMIT showNavigationPanelChanged();
                return;
            }
        }
        if (!std::holds_alternative<TNoSelection>(m_selection))
            m_selection = TNoSelection{};
        refreshSelection();
    } else if (areaId) {
        if (data.hasArea(*areaId))
            selectArea(*areaId);
        else if (data.hasSubArea(*areaId))
            selectSubArea(*areaId);
    } else if (const auto* passage = std::get_if<cyclopedia::TCyclopediaMapCharacterConfigurationStorage::TPassage>(&location)) {
        selectPassage(Position(passage->x, passage->y, static_cast<uint8_t>(passage->z)));
    }
    Q_EMIT showMapMarkerPanelChanged();
    Q_EMIT showNavigationPanelChanged();
}

void TCyclopediaMapDialogController::hideDialogTab()
{
    m_renderer = nullptr;
    if (auto* cursor = Cursor::instance())
        cursor->onCyclopediaMapExited();
}

void TCyclopediaMapDialogController::onSelectedItemChanged()
{
    if (!m_renderer)
        return;
    // An area or subarea highlights its subareas, a passage its field.
    std::set<int> highlighted;
    Position coordinate(0, 0, 0);
    if (const auto* area = std::get_if<TAreaSelection>(&m_selection)) {
        for (const quint16 id : StaticData::instance().subAreaIdsOfArea(area->data.id))
            highlighted.insert(id);
    } else if (const auto* subArea = std::get_if<TSubAreaSelection>(&m_selection)) {
        highlighted.insert(subArea->data.id);
    } else if (const auto* passage = std::get_if<TPassageSelection>(&m_selection)) {
        coordinate = passage->data.coordinate;
    }
    m_renderer->setHighlightedSubareas(highlighted);
    m_renderer->setHighlightedCoordinate(coordinate);
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
    centerToPlayer();
}

void TCyclopediaMapDialogController::requestResetViewpoints()
{
    m_confirmType = ConfirmResetViewpoints;
    m_confirmDialog->showMessage(qtTrId("cyclopediamap_confirm_reset_viewpoints_caption"),
                                 qtTrId("cyclopediamap_confirm_reset_viewpoints").arg(StaticData::instance().subAreaName(m_resetSubAreaId)));
}

void TCyclopediaMapDialogController::hoverAtPixelCoordinate(const int x, const int y)
{
    m_tooltipTimer.stop();
    const QString tooltip = m_renderer ? m_renderer->markerTooltipAt(QPointF(x, y)) : QString();
    if (!tooltip.isEmpty())
        m_tooltipTimer.start();
    if (m_currentTooltip != tooltip) {
        m_currentTooltip = tooltip;
        Q_EMIT currentTooltipChanged();
    }
    if (!m_mouseCursorTimer.isActive())
        m_mouseCursorTimer.start();
}

void TCyclopediaMapDialogController::onClickAtPixelCoordinate(const int x, const int y, const int TriggeringMouseButton, const Qt::KeyboardModifiers Modifiers)
{
    const input::TGameActionList actions = gameActionsAt(x, y);
    const input::IGameActionPtr action =
        m_mapper.map(actions, MapRuleContext, static_cast<Qt::MouseButton>(TriggeringMouseButton), static_cast<int>(Modifiers));
    if (!action)
        return;
    if (action->gameActionType() != A::CONTEXT_MENU) {
        Q_EMIT publishGameAction(action);
        return;
    }
    if (const input::TGameActionList entries = input::contextMenuActions(actions, ContextMenuOrder); !entries.empty()) {
        if (TContextMenuController* menu = TContextMenuController::instance())
            menu->showGameActions(entries, MapRuleContext, m_mapper);
    }
}

void TCyclopediaMapDialogController::centerToPlayer()
{
    if (!tibia::game::hasLocalPlayer() || !m_renderer)
        return;
    const Position position = tibia::game::localPlayerPosition();
    if (!position.isValid())
        return;
    m_renderer->worldCamera()->centerDelayed(position.x * SubfieldsPerField + SubfieldsPerField / 2, position.y * SubfieldsPerField + SubfieldsPerField / 2, position.z);
}

void TCyclopediaMapDialogController::centerAt(const Position& position, const float scale)
{
    if (!m_renderer)
        return;
    qmlcomponents::TWorldMapCamera* camera = m_renderer->worldCamera();
    if (!std::isnan(scale)) {
        Q_EMIT requestSetScaleFactor(scale);
        camera->setScaleFactor(scale);
    }
    camera->centerDelayed(position.x * SubfieldsPerField + SubfieldsPerField / 2, position.y * SubfieldsPerField + SubfieldsPerField / 2, position.z);
}

TCyclopediaDialogContext TCyclopediaMapDialogController::captureContext() const
{
    TMapInfoCyclopediaContext context;
    if (m_renderer) {
        const qmlcomponents::TWorldMapCamera* camera = m_renderer->worldCamera();
        context.topLeft = worldmap::TWorldMapSubfieldCoordinate(camera->topLeft(), camera->layer());
        context.zoom = camera->scaleFactor();
    }
    return context;
}

void TCyclopediaMapDialogController::restoreContext(const TCyclopediaDialogContext& CyclopediaContext)
{
    const auto& context = cyclopediaContextAs<TMapInfoCyclopediaContext>(CyclopediaContext);
    if (!m_renderer)
        return;
    qmlcomponents::TWorldMapCamera* camera = m_renderer->worldCamera();
    if (!std::isnan(context.zoom)) {
        Q_EMIT requestSetScaleFactor(context.zoom);
        camera->setScaleFactor(context.zoom);
    }
    if (context.topLeft == worldmap::TWorldMapSubfieldCoordinate())
        centerToPlayer();
    else
        camera->setTopLeftDelayed(context.topLeft.subfieldX, context.topLeft.subfieldY, context.topLeft.z);
}

void TCyclopediaMapDialogController::onCreatureClicked(const int RaceIDRaw)
{
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(1);
    action->setRaceId(static_cast<uint16_t>(RaceIDRaw));
    Q_EMIT publishGameAction(action);
}

void TCyclopediaMapDialogController::resetSubAreaViewPoints(const int SubAreaID)
{
    m_resetSubAreaId = static_cast<quint16>(SubAreaID);
    m_confirmType = ConfirmResetViewpoints;
    // An unlocked subarea keeps its viewpoints.
    if (m_storage->isDiscovered(m_resetSubAreaId))
        return;
    m_confirmDialog->showMessage(qtTrId("cyclopediamap_confirm_reset_viewpoints_caption"),
                                 qtTrId("cyclopediamap_confirm_reset_viewpoints").arg(StaticData::instance().subAreaName(m_resetSubAreaId)));
}

void TCyclopediaMapDialogController::donateMoneyForArea(const int AreaID, const int Value)
{
    m_donationAreaId = static_cast<quint16>(AreaID);
    m_donationAmount = Value;
    m_confirmType = ConfirmDonation;
    m_confirmDialog->showMessage(qtTrId("cyclopediamap_confirm_donate_caption"),
                                 qtTrId("cyclopediamap_confirm_donate_area")
                                     .arg(StaticData::instance().areaName(m_donationAreaId))
                                     .arg(QLocale(QLocale::English).toString(static_cast<qlonglong>(Value))));
}

void TCyclopediaMapDialogController::onMouseExited()
{
    m_tooltipTimer.stop();
    if (auto* cursor = Cursor::instance())
        cursor->onCyclopediaMapExited();
    m_mouseCursorTimer.stop();
}

void TCyclopediaMapDialogController::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    if (pGameAction->gameActionType() == A::REMOVE_SPRITEIDENTIFIER_FROM_CACHE) {
        if (m_renderer)
            m_renderer->requestRebuild();
        return;
    }
    if (pGameAction->gameActionType() != A::CYCLOPEDIA_MAP_SELECT_ITEM)
        return;
    const auto* item = input::gameActionAs<input::TGameActionCyclopediaMapSelectionItem>(pGameAction);
    if (!item)
        return;
    const input::TCyclopediaMapSelectionAreaData& areaData = item->areaData();
    switch (areaData.kind) {
    case input::TCyclopediaMapSelectionAreaData::SubArea:
        selectSubArea(areaData.subAreaId);
        m_configuration->setLocation(areaData.subAreaId);
        break;
    case input::TCyclopediaMapSelectionAreaData::HouseMarker: {
        // The houses tab opens with the house once this action is done.
        auto action = std::make_shared<input::TGameActionOpenCyclopedia>(HousesTab);
        action->setHouseId(areaData.houseId);
        QTimer::singleShot(0, this, [this, action]() { Q_EMIT publishGameAction(action); });
        break;
    }
    case input::TCyclopediaMapSelectionAreaData::Passage:
        selectPassage(areaData.coordinate);
        m_configuration->setLocation(cyclopedia::TCyclopediaMapCharacterConfigurationStorage::TPassage{
            areaData.coordinate.x, areaData.coordinate.y, areaData.coordinate.z });
        break;
    }
}

void TCyclopediaMapDialogController::onAreaClicked(const int id, const bool isArea)
{
    if (isArea)
        selectArea(static_cast<quint16>(id));
    else
        selectSubArea(static_cast<quint16>(id));
    m_configuration->setLocation(static_cast<uint16_t>(id));
}

void TCyclopediaMapDialogController::onConfirmDialogAccepted()
{
    if (m_confirmType == ConfirmResetViewpoints)
        tibia::game::sendCyclopediaMapResetViewpoints(m_resetSubAreaId);
    if (m_confirmType == ConfirmDonation)
        tibia::game::sendCyclopediaMapDonate(m_donationAreaId, static_cast<uint32_t>(m_donationAmount));
}

void TCyclopediaMapDialogController::onTooltipTimerTimeout() { Q_EMIT showTooltip(); }

void TCyclopediaMapDialogController::onMouseCursorTimerTimeout()
{
    auto* cursor = Cursor::instance();
    if (!m_renderer || !cursor)
        return;
    const int modifiers = static_cast<int>(QGuiApplication::queryKeyboardModifiers());
    const QPointF point = m_renderer->mapFromGlobal(QPointF(QCursor::pos()));
    if (!m_renderer->boundingRect().contains(point))
        return;
    cursor->onCyclopediaMapEntered();
    const input::TGameActionList actions = gameActionsAt(static_cast<int>(point.x()), static_cast<int>(point.y()));
    const input::IGameActionPtr action = m_mapper.map(actions, MapRuleContext, Qt::LeftButton, modifiers);
    int shape = Cursor::Default;
    if (action && action->gameActionType() == A::SET_PLAYER_MOVEMENT_INTENT) {
        // A walk the player cannot make from here shows the reject cursor.
        shape = Cursor::DefaultReject;
        const auto* intent = input::gameActionAs<input::TGameActionPlayerMovementIntent>(action);
        if (intent && tibia::game::hasLocalPlayer()) {
            const Position player = tibia::game::localPlayerPosition();
            const int dx = std::abs(static_cast<int>(intent->target().x) - static_cast<int>(player.x));
            const int dy = std::abs(static_cast<int>(intent->target().y) - static_cast<int>(player.y));
            if (intent->target().z == player.z && dx <= MaxWalkDistance && dy <= MaxWalkDistance)
                shape = worldmap::TWorldMapGameActionProvider::cursorShapeOf(action);
        }
    }
    cursor->setLayerShape(Cursor::CyclopediaMap, shape);
}

void TCyclopediaMapDialogController::refreshSelection()
{
    m_selectedItem = buildSelectionInfo();
    onSelectedItemChanged();
    Q_EMIT selectedItemChanged();
    Q_EMIT currentExploringSubAreaIDChanged();
}

void TCyclopediaMapDialogController::selectArea(const quint16 areaId)
{
    TAreaSelection selection;
    selection.data.id = areaId;
    m_selection = selection;
    refreshSelection();
}

void TCyclopediaMapDialogController::selectSubArea(const quint16 subAreaId)
{
    TSubAreaSelection selection;
    selection.data.id = subAreaId;
    m_selection = selection;
    refreshSelection();
}

void TCyclopediaMapDialogController::selectPassage(const Position& coordinate)
{
    TPassageSelection selection;
    selection.data.coordinate = coordinate;
    m_selection = selection;
    refreshSelection();
}

cip::tibia::gamewindow::ICyclopediaMapSelectionInfo* TCyclopediaMapDialogController::buildSelectionInfo()
{
    cip::tibia::gamewindow::ICyclopediaMapSelectionInfo* current = m_selectedItem;
    cip::tibia::gamewindow::ICyclopediaMapSelectionInfo* next = nullptr;
    if (auto* area = std::get_if<TAreaSelection>(&m_selection)) {
        updateAreaData(area->data);
        m_storage->setSelectedId(area->data.id);
        auto* info = qobject_cast<TCyclopediaMapSelectionInfoArea*>(current);
        if (info)
            info->setAreaData(area->data);
        else
            info = new TCyclopediaMapSelectionInfoArea(area->data, this);
        next = info;
    } else if (auto* subArea = std::get_if<TSubAreaSelection>(&m_selection)) {
        updateSubAreaData(subArea->data);
        m_storage->setSelectedId(subArea->data.id);
        auto* info = qobject_cast<TCyclopediaMapSelectionInfoSubArea*>(current);
        if (info)
            info->setSubAreaData(subArea->data);
        else
            info = new TCyclopediaMapSelectionInfoSubArea(subArea->data, this);
        next = info;
    } else if (auto* passage = std::get_if<TPassageSelection>(&m_selection)) {
        updatePassageData(passage->data);
        auto* info = qobject_cast<TCyclopediaMapSelectionInfoPassage*>(current);
        if (info)
            info->setPassageData(passage->data);
        else
            info = new TCyclopediaMapSelectionInfoPassage(passage->data, this);
        next = info;
    }
    // Host: CIP leaves a replaced info object to QML; qtclient deletes it.
    if (current && current != next)
        current->deleteLater();
    return next;
}

void TCyclopediaMapDialogController::updateAreaData(TCyclopediaMapAreaValues& area) const
{
    const auto& data = StaticData::instance();
    area.name = data.areaName(area.id);
    const auto* mainArea = m_storage->mainArea(area.id);
    area.percentExplored = mainArea ? mainArea->progress : 0;
    area.donated = m_storage->donated(area.id);
    area.donationsTooltip = donationsTooltip();
    area.donationGoal = m_storage->donationGoal();
    area.rejectDonations = data.doesAreaRejectDonations(area.id);
}

void TCyclopediaMapDialogController::updateSubAreaData(TCyclopediaMapSelectionInfoSubArea::Values& subArea) const
{
    const auto& data = StaticData::instance();
    const auto* staticSubArea = data.subArea(subArea.id);
    subArea.area.id = staticSubArea ? staticSubArea->areaId : 0;
    updateAreaData(subArea.area);
    subArea.name = data.subAreaName(subArea.id);
    subArea.unlockable = m_storage->isDiscoverable(subArea.id);
    subArea.unlocked = m_storage->isDiscovered(subArea.id);
    subArea.viewpointInfoDelayed = false;
    if (subArea.unlocked || m_storage->isAreaCompleted(subArea.area.id)) {
        subArea.viewpointsCount = CompletedViewpoints;
        subArea.viewpointsFound = CompletedViewpoints;
    } else if (!m_storage->hasSubAreaViewpointInfo(subArea.id)) {
        // Not known yet: the panel waits for the server's answer.
        subArea.viewpointInfoDelayed = true;
        tibia::game::sendCyclopediaMapRequestArea(subArea.id);
    } else {
        const auto info = m_storage->subAreaViewpointInfo(subArea.id);
        subArea.viewpointsCount = info.poiTarget;
        subArea.viewpointsFound = info.discoveredViewpoints;
    }
    subArea.creatureOccurrences = creatureOccurrences(subArea.id);
}

void TCyclopediaMapDialogController::updatePassageData(TCyclopediaMapSelectionInfoPassage::Values& passage) const
{
    passage.subAreaIds.clear();
    for (const auto& known : m_storage->passages()) {
        if (known.coordinate.x == passage.coordinate.x && known.coordinate.y == passage.coordinate.y && known.coordinate.z == passage.coordinate.z) {
            // The passage message lists the subareas the passage leads to.
            for (const int subAreaId : known.subAreaIds)
                passage.subAreaIds.push_back(static_cast<quint16>(subAreaId));
            break;
        }
    }
    passage.creatureOccurrences.clear();
    for (const quint16 subAreaId : passage.subAreaIds)
        passage.creatureOccurrences[subAreaId] = creatureOccurrences(subAreaId);
}

TCyclopediaCreatureOccurrencesData TCyclopediaMapDialogController::creatureOccurrences(const quint16 subAreaId) const
{
    TCyclopediaCreatureOccurrencesData data;
    data.subAreaId = subAreaId;
    data.subAreaName = StaticData::instance().subAreaName(subAreaId);
    for (const cyclopedia::TCyclopediaMapStorage::TSubAreaMonster& monster : m_storage->subAreaMonsters(subAreaId)) {
        TCyclopediaCreatureData creature;
        creature.raceId = monster.raceId;
        creature.isUnknown = !m_storage->isMonsterKnown(monster.raceId);
        creature.raceName = creature.isUnknown
            ? qtTrId("monsters_unknown_name")
            : cyclopedia::capitalizeWords(QString::fromStdString(tibia::game::raceData(monster.raceId).name));
        data.creatures[monster.state].push_back(std::move(creature));
    }
    return data;
}

QString TCyclopediaMapDialogController::donationsTooltip() const
{
    std::vector<std::pair<quint64, QString>> rows;
    for (const auto& [areaId, area] : StaticData::instance().areas()) {
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

int TCyclopediaMapDialogController::displayedFloor() const
{
    const int layer = m_renderer ? m_renderer->worldCamera()->layer() : GroundLayer;
    if (!tibia::game::hasLocalPlayer())
        return layer;
    const int playerFloor = tibia::game::localPlayerPosition().z;
    if (playerFloor > GroundLayer || layer > GroundLayer)
        return layer;
    return playerFloor;
}

input::TGameActionList TCyclopediaMapDialogController::gameActionsAt(const int x, const int y) const
{
    if (!m_renderer)
        return {};
    using Hit = qmlcomponents::TMinimapRendererQuickItem::TMarkerHit;
    const int floor = displayedFloor();
    qmlcomponents::TWorldMapCamera* camera = m_renderer->worldCamera();
    const QPoint field = camera->fieldAt(QPointF(x, y));
    const int layer = camera->layer();
    const Position coordinate(field.x(), field.y(), static_cast<uint8_t>(layer));

    input::TGameActionList selections;
    input::TGameActionList actions;
    // The markers under the point: a minimap marker gives its actions, the first
    // passage or house marker is what a click selects.
    for (const Hit& hit : m_renderer->markersAt(QPointF(x, y))) {
        if (hit.kind == Hit::MinimapMarker) {
            minimap::TMinimapMarkerStorage* markers = minimap::TMinimapMarkerStorage::active();
            if (markers && markers->markerAt(worldmap::TWorldMapCoordinate(hit.coordinate)))
                actions = m_provider.markerActions(hit.coordinate, floor);
            continue;
        }
        if ((hit.kind == Hit::Passage || hit.kind == Hit::HouseMarker) && selections.empty()) {
            input::TCyclopediaMapSelectionAreaData areaData;
            areaData.kind = hit.kind == Hit::Passage ? input::TCyclopediaMapSelectionAreaData::Passage
                                                     : input::TCyclopediaMapSelectionAreaData::HouseMarker;
            areaData.coordinate = hit.coordinate;
            areaData.houseId = hit.houseId;
            selections.push_back(std::make_shared<input::TGameActionCyclopediaMapSelectionItem>(areaData));
            break;
        }
    }
    if (actions.empty())
        actions = m_provider.fieldActions(coordinate, floor);
    // On and above the ground a click selects the subarea drawn under the field.
    if (selections.empty() && layer <= GroundLayer) {
        const worldmap::TWorldMapCoordinate ground = worldmap::TWorldMapCoordinate(coordinate).shearedToFloor(GroundLayer);
        const quint16 subAreaId = StaticData::instance().subAreaAt(ground);
        if (subAreaId != 0 && StaticData::instance().isCoordinateInSubArea(subAreaId, ground)) {
            input::TCyclopediaMapSelectionAreaData areaData;
            areaData.kind = input::TCyclopediaMapSelectionAreaData::SubArea;
            areaData.subAreaId = subAreaId;
            selections.push_back(std::make_shared<input::TGameActionCyclopediaMapSelectionItem>(areaData));
        }
    }
    input::TGameActionList result = selections;
    result.insert(result.end(), actions.begin(), actions.end());
    return result;
}

}
