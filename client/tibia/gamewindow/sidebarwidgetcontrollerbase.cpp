#include "sidebarwidgetcontrollerbase.h"

#include "gamewindow/mousecursorcontroller.h"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>

namespace tibia::gamewindow {

namespace {

constexpr int ContentSizeChangedDelayMs = 100;
constexpr int DefaultWidgetHeight = 57;

}

TSidebarWidget::TSidebarWidget(Controller* controller, QQuickItem* gameWindowItem, QQmlEngine* engine)
    : QObject(controller)
    , m_controller(controller)
    , m_engine(engine)
    , m_gameWindowItem(gameWindowItem)
{
    m_contentSizeTimer.setInterval(ContentSizeChangedDelayMs);
    m_contentSizeTimer.setSingleShot(true);
    connect(&m_contentSizeTimer, &QTimer::timeout, controller, &Controller::contentSizeChanged);
    connect(controller, &Controller::contentSizeChanged, controller, &Controller::saveWidgetOptions, Qt::QueuedConnection);
}

void TSidebarWidget::createItem(const QString& qmlFile)
{
    if (m_item || !m_engine)
        return;
    QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/") + qmlFile));
    QObject* object = component.beginCreate(m_engine->rootContext());
    auto* item = qobject_cast<QQuickItem*>(object);
    if (!item) {
        qWarning().noquote() << "Component creation error for" << qmlFile << component.errorString();
        delete object;
        return;
    }
    // CIP sets widgetController on the finished item (e.g. 0x100ba4b03):
    // battlelist.qml fills _FILTER_BUTTONS_INFO in Component.onCompleted.
    component.completeCreate();
    item->setProperty("widgetController", QVariant::fromValue<QObject*>(m_controller));
    m_item = item;
    m_item->setParent(m_controller);
    m_item->setVisible(false);

    connect(m_item, &QObject::destroyed, this, [this] {
        m_item = nullptr;
        Q_EMIT m_controller->widgetClosed();
    });
    connect(m_item, SIGNAL(maximizedChanged()), m_controller, SIGNAL(contentSizeChanged()));
    connect(m_item, SIGNAL(manuallyResizing()), this, SLOT(startContentSizeChangedTimer()));
    connect(m_item, SIGNAL(maximizedContentHeightHasChanged()), this, SLOT(startContentSizeChangedTimer()));
    connect(m_item, SIGNAL(positionInSidebarChanged()), m_controller, SIGNAL(positionInSidebarChanged()));
}

void TSidebarWidget::show()
{
    if (m_item)
        m_item->setVisible(true);
}

void TSidebarWidget::hide()
{
    if (m_item)
        m_item->setVisible(false);
}

void TSidebarWidget::setParentSidebarId(const uint sidebarId)
{
    if (sidebarId == m_parentSidebarId)
        return;
    m_parentSidebarId = sidebarId;
    Q_EMIT m_controller->sidebarIdChanged();
}

int TSidebarWidget::identifier() const
{
    return m_item ? m_item->property("identifier").toInt() : -1;
}

void TSidebarWidget::setMinContentHeight(const int height)
{
    if (m_item)
        m_item->setProperty("minContentHeight", height);
}

void TSidebarWidget::setMaxContentHeight(const int height)
{
    if (m_item)
        m_item->setProperty("maxContentHeight", height);
}

void TSidebarWidget::setInitialContentHeight(const int height)
{
    if (height <= 0 || !m_item || m_contentSizeTimer.isActive())
        return;
    m_item->setProperty("initialContentHeight", height);
    QMetaObject::invokeMethod(m_item, "applyInitialContentHeight");
}

void TSidebarWidget::setMaximized(const bool maximized)
{
    if (!m_item)
        return;
    m_item->setProperty("maximized", maximized);
    QMetaObject::invokeMethod(m_item, "applyInitialContentHeight");
}

void TSidebarWidget::setInitialContentHeightAndMaximized(const int height, const bool maximized)
{
    if (height <= 0 || !m_item || m_contentSizeTimer.isActive())
        return;
    m_item->setProperty("maximized", maximized);
    m_item->setProperty("initialContentHeight", height);
    QMetaObject::invokeMethod(m_item, "applyInitialContentHeight");
}

void TSidebarWidget::minimize()
{
    if (m_item)
        m_item->setProperty("maximized", false);
}

void TSidebarWidget::maximize()
{
    if (m_item)
        m_item->setProperty("maximized", true);
}

int TSidebarWidget::currentHeight() const
{
    if (!m_item)
        return DefaultWidgetHeight;
    if (const int initial = m_item->property("initialHeight").toInt(); initial > 0)
        return initial;
    const int minimum = m_item->property("minHeight").toInt();
    return minimum > 0 ? minimum : DefaultWidgetHeight;
}

int TSidebarWidget::maximizedContentHeight() const
{
    return m_item ? m_item->property("maximizedContentHeight").toInt() : 0;
}

bool TSidebarWidget::isMaximized() const
{
    return !m_item || m_item->property("maximized").toBool();
}

void TSidebarWidget::requestMoveToSidebar(const int targetSidebar)
{
    if (targetSidebar < 0)
        return;
    setDragActive(false);
    Q_EMIT widgetWantsToMoveToSidebar(targetSidebar, this);
}

void TSidebarWidget::setDragActive(const bool active)
{
    Q_EMIT widgetDragChanged(m_parentSidebarId, active);
}

void TSidebarWidget::onSidebarDestroyed()
{
    if (!m_item)
        return;
    m_controller->onRequestClose();
    m_item->setParentItem(nullptr);
    Q_EMIT m_controller->widgetClosed();
}

void TSidebarWidget::setCursorToResizeVertical(const bool enabled)
{
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setResizeVertical(enabled);
}

}
