#include "sidebarmanager.h"

#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <QPointF>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>

namespace tibia::gamewindow {

QQuickItem* createQuickItem(QQmlEngine* engine, const QString& file)
{
    if (!engine)
        return nullptr;
    QQmlComponent component(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/") + file));
    QObject* object = component.beginCreate(engine->rootContext());
    auto* item = qobject_cast<QQuickItem*>(object);
    if (!item) {
        qWarning().noquote() << "Component creation error for" << file << component.errorString();
        delete object;
        return nullptr;
    }
    component.completeCreate();
    return item;
}

void bindPanelItem(cip::tibia::gamewindow::TSidebarPanelControllerBase* controller, QQuickItem* item)
{
    item->setProperty("panelController", QVariant::fromValue<QObject*>(controller));
    QObject::connect(item, SIGNAL(positionInSidebarChanged()), controller, SIGNAL(positionInSidebarChanged()));
}

TSidebarController::TSidebarController(const uint sidebarId, QQuickItem* placeholder, QObject* parent)
    : cip::tibia::gamewindow::TSidebarController(parent)
{
    m_sidebarId = sidebarId;
    m_acceptsWidgetDrops = false;

    QQmlEngine* engine = qmlEngine(placeholder);
    QQmlComponent component(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/sidebar.qml")));
    m_item = qobject_cast<QQuickItem*>(component.beginCreate(engine->rootContext()));
    if (!m_item) {
        qWarning().noquote() << "sidebar.qml:" << component.errorString();
        return;
    }
    m_item->setParent(placeholder);
    m_item->setParentItem(placeholder);
    m_item->setImplicitHeight(placeholder->height());
    component.completeCreate();
    m_item->setProperty("controller", QVariant::fromValue<QObject*>(this));
    m_upperPane = m_item->findChild<QQuickItem*>(QStringLiteral("sidebarUpperPane"));
    m_lowerPane = m_item->findChild<QQuickItem*>(QStringLiteral("sidebarLowerPane"));
    connect(this, SIGNAL(checkBounds()), m_item, SLOT(onApplyWindowBounds()));
}

TSidebarController::~TSidebarController()
{
    delete m_item;
}

void TSidebarController::addPanel(QQuickItem* panel)
{
    panel->setProperty("identifier", m_nextIdentifier++);
    panel->setParentItem(m_upperPane);
    panel->setParent(m_upperPane);
}

void TSidebarController::movePanelToEnd(QQuickItem* panel)
{
    panel->setProperty("identifier", m_nextIdentifier++);
}

void TSidebarController::addWidget(TSidebarWidget* widget, const uint identifier)
{
    QQuickItem* item = widget->quickItem();
    if (!item || !m_lowerPane)
        return;
    connect(this, &QObject::destroyed, widget, &TSidebarWidget::onSidebarDestroyed, Qt::UniqueConnection);
    item->setProperty("identifier", identifier ? identifier : m_nextIdentifier++);
    widget->setParentSidebarId(m_sidebarId);
    item->setParentItem(m_lowerPane);
}

bool TSidebarController::removeWidget(TSidebarWidget* widget, const bool keepParentItem)
{
    QQuickItem* item = widget->quickItem();
    if (!item || !m_lowerPane || !m_lowerPane->childItems().contains(item))
        return false;
    disconnect(this, &QObject::destroyed, widget, &TSidebarWidget::onSidebarDestroyed);
    widget->setInitialContentHeightAndMaximized(widget->maximizedContentHeight(), widget->isMaximized());
    widget->resetParentSidebarId();
    if (!keepParentItem)
        item->setParentItem(nullptr);
    return true;
}

int TSidebarController::remainingHeight() const
{
    return m_item ? m_item->property("remainingHeight").toInt() : 0;
}

void TSidebarController::setAcceptsWidgetDrops(const bool accepts)
{
    if (accepts == m_acceptsWidgetDrops)
        return;
    m_acceptsWidgetDrops = accepts;
    Q_EMIT acceptsWidgetDropsChanged();
}

TSidebarManager::TSidebarManager(QQuickItem* rightPlaceholder, QObject* rightManagerItem, QQuickItem* leftPlaceholder, QObject* leftManagerItem,
                                 QObject* parent)
    : cip::tibia::gamewindow::TSidebarManager(parent)
    , m_rightPlaceholder(rightPlaceholder)
    , m_leftPlaceholder(leftPlaceholder)
{
    m_panelConfigurationTimer.setInterval(100);
    m_panelConfigurationTimer.setSingleShot(true);
    connect(&m_panelConfigurationTimer, &QTimer::timeout, this, &TSidebarManager::panelConfigurationChanged);
    m_widgetConfigurationTimer.setInterval(100);
    m_widgetConfigurationTimer.setSingleShot(true);
    connect(&m_widgetConfigurationTimer, &QTimer::timeout, this, &TSidebarManager::widgetConfigurationChanged);

    setSidebarCount(Right, 1);
    rightManagerItem->setProperty("controller", QVariant::fromValue<QObject*>(this));
    leftManagerItem->setProperty("controller", QVariant::fromValue<QObject*>(this));
}

TSidebarManager::~TSidebarManager() = default;

TSidebarController* TSidebarManager::sidebar(const uint sidebarId) const
{
    const auto it = m_sidebars.find(sidebarId);
    return it != m_sidebars.end() ? it->second.get() : nullptr;
}

void TSidebarManager::setSidebarCount(const Side side, const int count)
{
    int& current = side == Right ? m_rightCount : m_leftCount;
    const int target = std::max(side == Right ? 1 : 0, count);
    if (target == current)
        return;
    while (current < target)
        addSidebar(side);
    while (current > target)
        removeSidebar(side);
    Q_EMIT numberOfSidebarsChanged(m_sidebars.size());
    Q_EMIT sidebarMangerOptionsChanged();
}

void TSidebarManager::addSidebar(const Side side)
{
    int& count = side == Right ? m_rightCount : m_leftCount;
    const uint id = (side == Right ? 0 : LeftSidebarIdBase) + static_cast<uint>(count);
    m_sidebars[id] = std::make_unique<TSidebarController>(id, placeholder(side));
    ++count;
    enqueueWidgetConfigurationChangedToBeEmitted();
}

void TSidebarManager::removeSidebar(const Side side)
{
    int& count = side == Right ? m_rightCount : m_leftCount;
    if (count == 0)
        return;
    --count;
    m_sidebars.erase((side == Right ? 0 : LeftSidebarIdBase) + static_cast<uint>(count));
    enqueueWidgetConfigurationChangedToBeEmitted();
}

void TSidebarManager::addPanel(cip::tibia::gamewindow::TSidebarPanelControllerBase* controller, QQuickItem* panel)
{
    TSidebarController* main = sidebar(0);
    if (!main || !panel)
        return;
    connect(controller, &cip::tibia::gamewindow::TSidebarPanelControllerBase::positionInSidebarChanged, this,
            &TSidebarManager::enqueuePanelConfigurationChangedToBeEmitted);
    main->addPanel(panel);
    m_panelConfigurationTimer.start();
}

void TSidebarManager::movePanelToEnd(QQuickItem* panel)
{
    if (TSidebarController* main = sidebar(0); main && panel)
        main->movePanelToEnd(panel);
}

bool TSidebarManager::isHudSidebarId(const uint sidebarId)
{
    return sidebarId == HudRightId || sidebarId == HudLeftId || sidebarId == HudContainersId;
}

void TSidebarManager::ensureHudSidebar(const uint sidebarId, QQuickItem* hostPlaceholder)
{
    if (!isHudSidebarId(sidebarId) || !hostPlaceholder)
        return;

    // Host is stock CIP sidebarWidth (176). Fill it at 1:1 — no Item.scale.
    auto applyHudZoom = [](QQuickItem* item, QQuickItem* host) {
        if (!item || !host)
            return;
        item->setScale(1.0);
        item->setTransformOriginPoint(QPointF(0, 0));
        item->setWidth(host->width());
        item->setHeight(host->height());
        item->setPosition(QPointF(0, 0));
    };

    if (auto it = m_sidebars.find(sidebarId); it != m_sidebars.end()) {
        if (QQuickItem* item = it->second->item()) {
            item->setParentItem(hostPlaceholder);
            item->setParent(hostPlaceholder);
            applyHudZoom(item, hostPlaceholder);
        }
        return;
    }
    m_sidebars[sidebarId] = std::make_unique<TSidebarController>(sidebarId, hostPlaceholder);
    if (QQuickItem* item = m_sidebars[sidebarId]->item()) {
        applyHudZoom(item, hostPlaceholder);
        QObject::connect(hostPlaceholder, &QQuickItem::widthChanged, item, [item, hostPlaceholder, applyHudZoom] {
            applyHudZoom(item, hostPlaceholder);
        });
        QObject::connect(hostPlaceholder, &QQuickItem::heightChanged, item, [item, hostPlaceholder, applyHudZoom] {
            applyHudZoom(item, hostPlaceholder);
        });
    }
    enqueueWidgetConfigurationChangedToBeEmitted();
}

void TSidebarManager::teardownHudSidebars()
{
    for (const uint id : { HudRightId, HudLeftId, HudContainersId })
        m_sidebars.erase(id);
    enqueueWidgetConfigurationChangedToBeEmitted();
}

void TSidebarManager::addWidget(TSidebarWidget* widget)
{
    if (!widget || m_sidebars.empty())
        return;
    const int height = widget->currentHeight();
    uint target = m_leftCount == 0 ? static_cast<uint>(m_rightCount - 1) : LeftSidebarIdBase - 1 + static_cast<uint>(m_leftCount);
    for (const auto& [id, sidebar] : m_sidebars) {
        if (isHudSidebarId(id))
            continue;
        if (sidebar->remainingHeight() >= height) {
            target = id;
            break;
        }
    }
    addWidget(target, widget);
}

void TSidebarManager::addWidget(const uint sidebarId, TSidebarWidget* widget, const uint identifier)
{
    TSidebarController* target = sidebar(sidebarId);
    if (!target || !widget)
        return;
    connect(widget, &TSidebarWidget::widgetWantsToMoveToSidebar, this, &TSidebarManager::moveWidgetToSidebar,
            Qt::UniqueConnection);
    connect(widget, &TSidebarWidget::widgetDragChanged, this, &TSidebarManager::onWidgetDragChanged, Qt::UniqueConnection);
    connect(widget->controller(), &TSidebarWidget::Controller::positionInSidebarChanged, this,
            &TSidebarManager::enqueueWidgetConfigurationChangedToBeEmitted, Qt::UniqueConnection);
    target->addWidget(widget, identifier);
    enqueueWidgetConfigurationChangedToBeEmitted();
}

bool TSidebarManager::removeWidget(TSidebarWidget* widget)
{
    if (!widget)
        return false;
    for (const auto& [id, sidebar] : m_sidebars) {
        if (sidebar->removeWidget(widget, false)) {
            enqueueWidgetConfigurationChangedToBeEmitted();
            return true;
        }
    }
    return false;
}

std::vector<uint> TSidebarManager::sidebarIds() const
{
    std::vector<uint> ids;
    ids.reserve(m_sidebars.size());
    for (const auto& [id, sidebar] : m_sidebars)
        ids.push_back(id);
    return ids;
}

std::vector<uint> TSidebarManager::classicSidebarIds() const
{
    std::vector<uint> ids;
    ids.reserve(m_sidebars.size());
    for (const auto& [id, sidebar] : m_sidebars) {
        if (!isHudSidebarId(id))
            ids.push_back(id);
    }
    return ids;
}

void TSidebarManager::moveWidgetToSidebar(const int targetSidebar, TSidebarWidget* widget)
{
    if (targetSidebar < 0 || !hasSidebar(static_cast<uint>(targetSidebar)))
        return;
    for (const auto& [id, sidebar] : m_sidebars) {
        if (sidebar->removeWidget(widget, true)) {
            addWidget(static_cast<uint>(targetSidebar), widget);
            return;
        }
    }
}

void TSidebarManager::onWidgetDragChanged(const uint sourceSidebar, const bool active)
{
    for (const auto& [id, sidebar] : m_sidebars)
        sidebar->setAcceptsWidgetDrops(active && id != sourceSidebar);
}

void TSidebarManager::openAdditionalSidebar(const QString& position)
{
    addSidebar(position.compare(QLatin1String("left"), Qt::CaseInsensitive) == 0 ? Left : Right);
    Q_EMIT numberOfSidebarsChanged(m_sidebars.size());
    Q_EMIT sidebarMangerOptionsChanged();
}

void TSidebarManager::closeSidebarClosestToGamewindow(const QString& position)
{
    const Side side = position.compare(QLatin1String("left"), Qt::CaseInsensitive) == 0 ? Left : Right;
    if (side == Right ? !canCloseRightSidebar() : !canCloseLeftSidebar())
        return;
    removeSidebar(side);
    Q_EMIT numberOfSidebarsChanged(m_sidebars.size());
    Q_EMIT sidebarMangerOptionsChanged();
}

void TSidebarManager::onUnusedWidthChanged(const int unusedWidth)
{
    m_unusedWidth = unusedWidth;
    Q_EMIT sidebarMangerOptionsChanged();
}

void TSidebarManager::enqueuePanelConfigurationChangedToBeEmitted()
{
    m_panelConfigurationTimer.start();
}

void TSidebarManager::enqueueWidgetConfigurationChangedToBeEmitted()
{
    m_widgetConfigurationTimer.start();
}

}
