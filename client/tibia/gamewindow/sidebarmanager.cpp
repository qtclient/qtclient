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

// b1/148e92aaa4db9daa
void TSidebarController::addPanel(QQuickItem* panel)
{
    panel->setProperty("identifier", m_nextIdentifier++);
    panel->setParentItem(m_upperPane);
    panel->setParent(m_upperPane);
}

// b1/9a4bd4a09d6c797c
void TSidebarController::movePanelToEnd(QQuickItem* panel)
{
    panel->setProperty("identifier", m_nextIdentifier++);
}

// b1/792965b49348607c
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

// b1/b45617d0d16d3b31
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

// b1/3e74d459c1de32a0
int TSidebarController::remainingHeight() const
{
    return m_item ? m_item->property("remainingHeight").toInt() : 0;
}

// b1/6b768282e83b1e89
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

// b1/3948f15292eb5456
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

// b1/779ecc8bfeafd36a
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

// b1/61908bfd1ec6462b
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

// b1/3329f5b9c390df7c
void TSidebarManager::movePanelToEnd(QQuickItem* panel)
{
    if (TSidebarController* main = sidebar(0); main && panel)
        main->movePanelToEnd(panel);
}

// b1/0a7b90212a3636af
void TSidebarManager::addWidget(TSidebarWidget* widget)
{
    if (!widget || m_sidebars.empty())
        return;
    const int height = widget->currentHeight();
    uint target = m_leftCount == 0 ? static_cast<uint>(m_rightCount - 1) : LeftSidebarIdBase - 1 + static_cast<uint>(m_leftCount);
    for (const auto& [id, sidebar] : m_sidebars) {
        if (sidebar->remainingHeight() >= height) {
            target = id;
            break;
        }
    }
    addWidget(target, widget);
}

// b1/0a7b90212a3636af
void TSidebarManager::addWidget(const uint sidebarId, TSidebarWidget* widget, const uint identifier)
{
    TSidebarController* target = sidebar(sidebarId);
    if (!target || !widget)
        return;
    connect(widget, &TSidebarWidget::widgetWantsToMoveToSidebar, this, &TSidebarManager::moveWidgetToSidebar,
            Qt::UniqueConnection);
    connect(widget, &TSidebarWidget::widgetDragChanged, this, &TSidebarManager::onWidgetDragChanged, Qt::UniqueConnection);
    if (QObject* controller = widget->controller()) {
        // Tip NOTIFY deepen: bind positionInSidebarChanged on the dynamic moc.
        connect(controller, SIGNAL(positionInSidebarChanged()), this,
                SLOT(onControllerPositionInSidebarChanged()), Qt::UniqueConnection);
    }
    target->addWidget(widget, identifier);
    enqueueWidgetConfigurationChangedToBeEmitted();
}

// b1/a70a0b5b07eccf6e
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

// b1/191094d035b742e4
void TSidebarManager::moveWidgetToSidebar(const int targetSidebar, TSidebarWidget* widget)
{
    if (targetSidebar < 0 || !hasSidebar(static_cast<uint>(targetSidebar)) || !widget)
        return;
    for (const auto& [id, sidebar] : m_sidebars) {
        if (sidebar->removeWidget(widget, true)) {
            addWidget(static_cast<uint>(targetSidebar), widget);
            return;
        }
    }
    // Not in a vertical sidebar (e.g. pack horizontal dock): still place.
    addWidget(static_cast<uint>(targetSidebar), widget);
}

// b1/918d19ccf0ebb117
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

void TSidebarManager::onControllerPositionInSidebarChanged()
{
    enqueueWidgetConfigurationChangedToBeEmitted();
}

}
