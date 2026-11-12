#pragma once

#include <cip_gen.h>

#include <QMetaMethod>
#include <QPointer>
#include <QTimer>

#include <functional>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace gamewindow {

/// Resolve a moc signal on the controller's dynamic (tip) metaobject so tip
/// NOTIFY deepen redeclares stay live when TSidebarWidget emits chrome.
inline QMetaMethod sidebarControllerSignal(QObject* controller, const char* signature)
{
    if (!controller || !signature)
        return {};
    const QByteArray normalized = QMetaObject::normalizedSignature(signature);
    const int index = controller->metaObject()->indexOfSignal(normalized.constData());
    return index >= 0 ? controller->metaObject()->method(index) : QMetaMethod();
}

/// Emit a cip_gen FINAL Q_PROPERTY NOTIFY on the *base* metaobject.
/// Tip must not shadow FINAL properties (Qt ignores the override); QML listens
/// to the cip_gen NOTIFY. Prefer inheriting the base signal (no tip redeclare)
/// and Q_EMIT — this helper is for dual-emit when SoftHook still needs tip.
inline void emitCipBaseNotify(QObject* object, const QMetaObject* cipMeta, const char* signalSignature)
{
    if (!object || !cipMeta || !signalSignature)
        return;
    const QByteArray normalized = QMetaObject::normalizedSignature(signalSignature);
    const int index = cipMeta->indexOfSignal(normalized.constData());
    if (index < 0)
        return;
    const QMetaMethod method = cipMeta->method(index);
    if (method.isValid())
        method.invoke(object, Qt::DirectConnection);
}

inline void emitSidebarControllerSignal(QObject* controller, const char* signature)
{
    const QMetaMethod method = sidebarControllerSignal(controller, signature);
    if (method.isValid())
        method.invoke(controller, Qt::DirectConnection);
}

/// Same as above with up to two Q_ARG payloads (drag / move chrome).
inline void emitSidebarControllerSignal(QObject* controller, const char* signature, QMetaMethodArgument a0,
                                        QMetaMethodArgument a1 = {})
{
    const QMetaMethod method = sidebarControllerSignal(controller, signature);
    if (method.isValid())
        method.invoke(controller, Qt::DirectConnection, a0, a1);
}

/// What CIP's TSidebarWidgetControllerBase (ctor) adds to every
/// sidebar widget: its TibiaSidebarWidget item, which knows the controller as
/// widgetController, the sidebar it is in and its size. The sidebar manager
/// places widgets through it (TSidebarManager::addWidget). A child of the
/// controller, which reaches it through TSidebarWidgetController.
class TSidebarWidget final : public QObject
{
    Q_OBJECT
public:
    using Controller = cip::tibia::gamewindow::TSidebarWidgetControllerBase;
    static constexpr uint NoSidebar = 0xff;

    /// `assignParentSidebarId` keeps CipSoft's FINAL `parentSidebarId` member
    /// (`TSidebarWidgetControllerBase::m_parentSidebarId`) in sync — QML reads
    /// that FINAL property and ignores tip overrides.
    TSidebarWidget(Controller* controller, QQuickItem* gameWindowItem, QQmlEngine* engine,
                   std::function<void(uint)> assignParentSidebarId = {});

    /// The item (hidden, with the controller as its widgetController), made
    /// once the controller is fully constructed: QML resolves the
    /// controller's properties and slots through its most derived class.
    void createItem(const QString& qmlFile);
    /// Same as createItem, from an arbitrary QML URL (pack FS module, file:).
    void createItemFromUrl(const QUrl& url);

    Controller* controller() const { return m_controller; }
    QQmlEngine* engine() const { return m_engine; }
    QQuickItem* gameWindowItem() const { return m_gameWindowItem; }
    /// v16: null once the item is gone.
    QQuickItem* quickItem() const { return m_item; }

    /// v13 / v14.
    void show();
    void hide();
    /// v17 / v18 / v19 (0xff: in no sidebar).
    uint parentSidebarId() const { return m_parentSidebarId; }
    void setParentSidebarId(uint sidebarId);
    void resetParentSidebarId() { setParentSidebarId(NoSidebar); }
    /// v20: the item's row in its sidebar, -1 without item.
    int identifier() const;
    /// v21 / v22.
    void setMinContentHeight(int height);
    void setMaxContentHeight(int height);
    /// v23 / v24 / v25: the content height (and state) the next add applies,
    /// ignored while a size change is pending.
    void setInitialContentHeight(int height);
    void setMaximized(bool maximized);
    void setInitialContentHeightAndMaximized(int height, bool maximized);
    /// v26 / v27.
    void minimize();
    void maximize();
    /// v28: the height the widget takes when added (initialHeight, else
    /// minHeight, else 57).
    int currentHeight() const;
    /// v29 / v30.
    int maximizedContentHeight() const;
    bool isMaximized() const;

public Q_SLOTS:
    /// v33: dropped onto sidebar `targetSidebar`.
    void requestMoveToSidebar(int targetSidebar);
    /// v34.
    void setDragActive(bool active);
    /// v35 (CIP): the widget closes with its sidebar.
    void onSidebarDestroyed();
    /// CIP .
    void startContentSizeChangedTimer() { m_contentSizeTimer.start(); }
    void setCursorToResizeVertical(bool enabled);

Q_SIGNALS:
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    void widgetDragChanged(uint sidebarId, bool active);

private:
    Controller* m_controller;
    QQmlEngine* m_engine;
    QQuickItem* m_gameWindowItem;
    QPointer<QQuickItem> m_item;
    QTimer m_contentSizeTimer;
    std::function<void(uint)> m_assignParentSidebarId;
    uint m_parentSidebarId = NoSidebar;
};

/// A CIP widget controller class with TSidebarWidgetControllerBase's slots
/// (CIP vtable , slots 31 to 35) on its TSidebarWidget.
template <class Base>
class TSidebarWidgetController : public Base
{
public:
    TSidebarWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr)
        : Base(parent)
        , m_widget(new TSidebarWidget(this, gameWindowItem, engine, [this](uint id) {
              // CipSoft stores the sidebar id on the FINAL base property member.
              this->m_parentSidebarId = id;
          }))
    {}

    TSidebarWidget* widget() const { return m_widget; }
    QQuickItem* quickItem() const { return m_widget->quickItem(); }

    QQuickItem* gameWindowQuickItem() const override { return m_widget->gameWindowItem(); }
    uint parentSidebarId() const override { return this->m_parentSidebarId; }

    void onCloseClicked() override { this->onRequestClose(); }
    /// v32 (CIP).
    void onRequestClose() override { Q_EMIT this->widgetClosed(); }
    void onRequestMoveToSidebar(int targetSidebar) override { m_widget->requestMoveToSidebar(targetSidebar); }
    void onSetDragActive(bool active) override { m_widget->setDragActive(active); }
    void onSidebarDestroyed() override { m_widget->onSidebarDestroyed(); }
    void setCursorToResizeVertical(bool enabled) override { m_widget->setCursorToResizeVertical(enabled); }

private:
    TSidebarWidget* m_widget;
};

}
}
