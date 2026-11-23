#include "gamewindow/tutorialoverlaycontroller.h"

#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "gamewindow/sidebarmanager.h"
#include "gamewindow/tutorialoverlaymousetrailanimationcontroller.h"
#include "input/eventfiltermanager.h"
#include "qmlcomponents/tutorialhelper.h"

#include <QQuickItem>
#include <QVariant>

#include <cmath>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

/// An overlay item shows its marker's hint under the common caption.
void showHint(const TTutorialOverlayItemController& overlayItem, const TTutorialOverlayController::THint& hint)
{
    QQuickItem* item = overlayItem.item();
    if (!item)
        return;
    item->setProperty("htmlText", hint.text);
    item->setProperty("caption", qtTrId("tutorial_hint_caption"));
    item->setProperty("showBorder", hint.border);
    item->setProperty("textAnchor", hint.textAnchor);
}

}

TTutorialOverlayController::TTutorialOverlayController(QQmlEngine* engine, QQuickItem* dialogPlaceholder,
                                                       QQuickItem* gameWindowItem,
                                                       input::TEventFilterManager* eventFilterManager, QObject* parent)
    : cip::tibia::gamewindow::TTutorialOverlayController(parent)
    , m_engine(engine)
    , m_dialogPlaceholder(dialogPlaceholder)
    , m_gameWindowItem(gameWindowItem)
    , m_eventFilterManager(eventFilterManager)
{
    // The mouse over the overlay's mouse filters shows the rejecting arrow.
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setLayerShape(TMouseCursorController::TutorialOverlay, TMouseCursorController::DefaultReject);
}

TTutorialOverlayController::~TTutorialOverlayController() = default;

QQuickItem* TTutorialOverlayController::createOverlayItem(const QString& qmlFile)
{
    QQuickItem* item = createQuickItem(m_engine, qmlFile);
    if (!item)
        return nullptr;
    item->setParent(this);
    if (m_dialogPlaceholder)
        item->setParentItem(m_dialogPlaceholder);
    return item;
}

void TTutorialOverlayController::createMouseTrail()
{
    if (!m_dialogPlaceholder)
        return;
    QQuickItem* item = createOverlayItem(QStringLiteral("tutorial/TutorialOverlayMouseTrailAnimation.qml"));
    m_mouseTrail = std::make_shared<TTutorialOverlayMouseTrailAnimationController>(item);
}

void TTutorialOverlayController::showOverlay()
{
    if (m_isShown)
        return;
    m_isShown = true;
    createMouseTrail();
    if (m_eventFilterManager)
        m_eventFilterManager->showTutorialOverlay();
}

void TTutorialOverlayController::hideOverlay()
{
    if (!m_isShown)
        return;
    m_isShown = false;
    m_mouseTrail.reset();
    m_mouseFilters.clear();
    m_overlayItems.clear();
    if (m_eventFilterManager)
        m_eventFilterManager->hideTutorialOverlay();
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->onTutorialOverlayExited();
}

void TTutorialOverlayController::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
    case A::REMOVE_ALL_TUTORIAL_MARKERS:
        m_hints.clear();
        m_mouseTrailSourceMarkerID = 0;
        m_mouseTrailDestinationMarkerID = 0;
        m_mouseTrailAnimationType = 0;
        update();
        return;
    case A::ADD_HINT_AT_TUTORIAL_MARKER:
        if (const auto* action = input::gameActionAs<input::TGameActionAddHintAtTutorialMarker>(pGameAction)) {
            m_hints[action->hint().markerId] = action->hint();
            update();
        }
        return;
    case A::ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER:
        if (const auto* action = input::gameActionAs<input::TGameActionAddMouseTrailHintAtTutorialMarker>(pGameAction)) {
            m_mouseTrailSourceMarkerID = action->sourceMarkerId();
            m_mouseTrailDestinationMarkerID = action->destinationMarkerId();
            m_mouseTrailAnimationType = action->animationType();
            update();
        }
        return;
    case A::CHANGE_TUTORIAL_MODE_ENABLED:
        if (const auto* action = input::gameActionAs<input::TGameActionChangeTutorialModeEnabled>(pGameAction)) {
            if (action->enabled())
                showOverlay();
            else
                hideOverlay();
        }
        return;
    default:
        return;
    }
}

void TTutorialOverlayController::onMouseFilterAreaEntered()
{
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->onTutorialOverlayEntered();
}

void TTutorialOverlayController::onMouseFilterAreaExited()
{
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->onTutorialOverlayExited();
}

void TTutorialOverlayController::update()
{
    if (!m_isShown)
        return;
    std::map<int, std::shared_ptr<TTutorialOverlayItemController>> markersGone = m_overlayItems;
    const std::vector<cip::tibia::qmlcomponents::TTutorialMarkerQuickItem*> markers
        = qmlcomponents::TTutorialMarkerRegistry::instance().markers();
    // The free areas start as the whole game window; each marker is cut out.
    std::vector<QRectF> freeAreas;
    if (m_gameWindowItem)
        freeAreas.emplace_back(m_gameWindowItem->position(), m_gameWindowItem->boundingRect().size());
    for (cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* marker : markers) {
        const int markerID = marker->markerID();
        m_markerItems[markerID] = marker;
        std::shared_ptr<TTutorialOverlayItemController> overlayItem;
        if (const auto known = m_overlayItems.find(markerID); known != m_overlayItems.end()) {
            overlayItem = known->second;
            markersGone.erase(markerID);
        } else {
            overlayItem = std::make_shared<TTutorialOverlayItemController>(
                createOverlayItem(QStringLiteral("tutorial/TutorialOverlayTemplate.qml")));
            m_overlayItems[markerID] = overlayItem;
        }
        const QPointF position = marker->mapToItem(m_dialogPlaceholder, QPointF(0, 0));
        const QSizeF size = marker->boundingRect().size();
        const QRectF rect(std::floor(position.x()), std::floor(position.y()), std::floor(size.width()),
                          std::floor(size.height()));
        if (QQuickItem* item = overlayItem->item()) {
            item->setSize(rect.size());
            item->setPosition(rect.topLeft());
        }
        subtractRect(rect, freeAreas);
    }
    for (const auto& [markerID, overlayItem] : markersGone)
        m_overlayItems.erase(markerID);
    updateMouseFilters(freeAreas);

    std::shared_ptr<TTutorialOverlayItemController> source;
    std::shared_ptr<TTutorialOverlayItemController> destination;
    for (const auto& [markerID, overlayItem] : m_overlayItems) {
        if (const auto hint = m_hints.find(markerID); hint != m_hints.end()) {
            showHint(*overlayItem, hint->second);
        } else {
            THint none;
            none.markerId = markerID;
            showHint(*overlayItem, none);
        }
        if (markerID == m_mouseTrailSourceMarkerID)
            source = overlayItem;
        if (markerID == m_mouseTrailDestinationMarkerID)
            destination = overlayItem;
    }
    if (!m_mouseTrail)
        return;
    m_mouseTrail->setSource(source);
    m_mouseTrail->setDestination(destination);
    m_mouseTrail->setAnimationType(m_mouseTrailAnimationType);
    m_mouseTrail->updateAnimation();
}

void TTutorialOverlayController::updateMouseFilters(const std::vector<QRectF>& areas)
{
    if (areas.size() < m_mouseFilters.size())
        m_mouseFilters.resize(areas.size());
    for (size_t index = 0; index < areas.size(); ++index) {
        std::shared_ptr<TTutorialOverlayMouseFilterController> filter;
        if (m_mouseFilters.size() < index + 1) {
            QQuickItem* item = createOverlayItem(QStringLiteral("tutorial/TutorialOverlayMouseFilterTemplate.qml"));
            filter = std::make_shared<TTutorialOverlayMouseFilterController>(item);
            if (item) {
                connect(item, SIGNAL(entered()), this, SLOT(onMouseFilterAreaEntered()));
                connect(item, SIGNAL(exited()), this, SLOT(onMouseFilterAreaExited()));
            }
            m_mouseFilters.push_back(filter);
        } else {
            filter = m_mouseFilters[index];
        }
        if (QQuickItem* item = filter->item()) {
            item->setSize(areas[index].size());
            item->setPosition(areas[index].topLeft());
        }
    }
}

void TTutorialOverlayController::subtractRect(const QRectF& rect, std::vector<QRectF>& areas)
{
    std::vector<QRectF> remaining;
    for (const QRectF& area : areas) {
        const QRectF overlap = rect & area;
        if (overlap.width() <= 0.0 || overlap.height() <= 0.0) {
            remaining.push_back(area);
            continue;
        }
        const double leftWidth = overlap.x() - area.x();
        if (leftWidth > 0.0 && area.height() > 0.0)
            remaining.emplace_back(area.x(), area.y(), leftWidth, area.height());
        const double overlapRight = overlap.x() + overlap.width();
        const double rightWidth = (area.x() + area.width()) - overlapRight;
        if (rightWidth > 0.0 && area.height() > 0.0)
            remaining.emplace_back(overlapRight, area.y(), rightWidth, area.height());
        const double topHeight = overlap.y() - area.y();
        if (overlap.width() > 0.0 && topHeight > 0.0)
            remaining.emplace_back(overlap.x(), area.y(), overlap.width(), topHeight);
        if (overlap.width() > 0.0) {
            const double overlapBottom = overlap.y() + overlap.height();
            const double bottomHeight = (area.y() + area.height()) - overlapBottom;
            if (bottomHeight > 0.0)
                remaining.emplace_back(overlap.x(), overlapBottom, overlap.width(), bottomHeight);
        }
    }
    areas = std::move(remaining);
}

}
