#include "dialogmanager.h"

#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/mousecursorcontroller.h"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QThread>
#include <QDebug>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

TDialogStackController* g_instance = nullptr;

constexpr auto ModuleUrl = "qrc:/qt/qml/qmlcomponents/qml/";

QUrl urlOf(const QString& qmlFile)
{
    return QUrl(qmlFile.contains(QLatin1Char(':')) ? qmlFile : QLatin1String(ModuleUrl) + qmlFile);
}

/// CIP FUN_10109b3a0 / FUN_10109b6f0: an item of `qmlFile` under `parentItem`,
/// owned by `owner`.
QQuickItem* createItem(QQmlEngine* engine, const QString& qmlFile, QQuickItem* parentItem, QObject* owner, const QVariantMap& properties)
{
    QQmlComponent component(engine, urlOf(qmlFile));
    QObject* object = component.beginCreate(engine->rootContext());
    auto* item = qobject_cast<QQuickItem*>(object);
    if (!item) {
        qCritical().noquote() << "Component creation error for" << qmlFile << component.errorString();
        delete object;
        return nullptr;
    }
    if (!properties.isEmpty())
        component.setInitialProperties(item, properties);
    item->setParent(owner);
    item->setParentItem(parentItem);
    component.completeCreate();
    return item;
}

void deleteItem(QPointer<QQuickItem>& item)
{
    if (!item)
        return;
    item->setVisible(false);
    item->setParentItem(nullptr);
    item->setParent(nullptr);
    // Once the event loop has exited (controllers destroyed at shutdown) a deferred
    // delete never runs and the item outlives the context objects it binds to.
    if (QThread::currentThread()->loopLevel() == 0)
        delete item.data();
    else
        item->deleteLater();
    item = nullptr;
}

}

TDialogStackController::TDialogStackController(QQmlEngine* engine, QQuickItem* dialogPlaceholder, QObject* parent)
    : cip::tibia::gamewindow::TDialogStackController(parent)
    , m_engine(engine)
    , m_placeholder(dialogPlaceholder)
{
    ownProductSurface(this);
    g_instance = this;
}

TDialogStackController::~TDialogStackController()
{
    if (g_instance == this)
        g_instance = nullptr;
}

TDialogStackController* TDialogStackController::instance()
{
    return g_instance;
}

void TDialogStackController::show(TDialog* dialog)
{
    if (!dialog)
        return;
    std::erase(m_dialogs, dialog);
    m_dialogs.push_back(dialog);
    update();
}

void TDialogStackController::hide(TDialog* dialog)
{
    if (!contains(dialog))
        return;
    dialog->setVisibleByStack(false);
    remove(dialog);
}

void TDialogStackController::remove(TDialog* dialog)
{
    if (std::erase(m_dialogs, dialog) != 0)
        update();
}

bool TDialogStackController::contains(const TDialog* dialog) const
{
    return std::find(m_dialogs.begin(), m_dialogs.end(), dialog) != m_dialogs.end();
}

void TDialogStackController::dialogTookFocus(const TDialog* dialog)
{
    if (!m_dialogs.empty() && m_dialogs.back() == dialog)
        Q_EMIT chatInputFocusChanged(false);
}

void TDialogStackController::dialogReleasedFocus(const TDialog* dialog)
{
    if (!m_dialogs.empty() && m_dialogs.back() == dialog)
        Q_EMIT chatInputFocusChanged(true);
}

void TDialogStackController::setCharacterListOpen(const bool open)
{
    m_characterListOpen = open;
    if (open)
        blockKeyboard();
    else if (m_dialogs.empty() || m_dialogs.back()->hooks()->hasPersonalMouseShield())
        unblockKeyboard();
    else
        m_hotkeysBlocked = true;
}

void TDialogStackController::blockKeyboard()
{
    m_hotkeysBlocked = true;
    if (m_keyboardBlocked)
        return;
    m_keyboardBlocked = true;
    m_eventFilters.block();
    Q_EMIT chatInputFocusChanged(false);
}

void TDialogStackController::unblockKeyboard()
{
    m_hotkeysBlocked = m_characterListOpen || (!m_dialogs.empty() && !m_dialogs.back()->hooks()->hasPersonalMouseShield());
    if (!m_keyboardBlocked)
        return;
    m_keyboardBlocked = false;
    m_eventFilters.unblock();
    Q_EMIT chatInputFocusChanged(true);
}

void TDialogStackController::hideAll()
{
    while (!m_dialogs.empty())
        m_dialogs.back()->hide();
}

void TDialogStackController::update()
{
    m_hotkeysBlocked = m_characterListOpen || (!m_dialogs.empty() && !m_dialogs.back()->hooks()->hasPersonalMouseShield());
    // CIP's stack unblocks the TEventFilterManager when it doesn't block: a focused UI part lets go.
    if (m_keyboardBlocked && !m_hotkeysBlocked)
        unblockKeyboard();
    for (TDialog* dialog : m_dialogs)
        dialog->setVisibleByStack(dialog == m_dialogs.back());
    Q_EMIT dialogVisible(!m_dialogs.empty());
}

TDialog::TDialog(Controller* controller, TDialogHooks* hooks, const QString& qmlFile)
    : QObject(controller)
    , m_controller(controller)
    , m_hooks(hooks)
    , m_qmlFile(qmlFile)
{}

TDialog::~TDialog()
{
    if (TDialogStackController* stack = TDialogStackController::instance())
        stack->remove(this);
}

void TDialog::create()
{
    TDialogStackController* stack = TDialogStackController::instance();
    if (m_item || !stack || !stack->engine() || !stack->dialogPlaceholder())
        return;
    QVariantMap properties = m_hooks->initialDialogProperties();
    QQuickItem* parentItem = stack->dialogPlaceholder();
    if (m_hooks->hasPersonalMouseShield()) {
        properties.insert(QStringLiteral("personalMouseShield"), true);
    } else {
        if (!m_shield) {
            m_shield = createItem(stack->engine(), QStringLiteral("TibiaMouseShield.qml"), parentItem, this, {});
            if (m_shield) {
                m_shield->setVisible(false);
                m_shield->setProperty("mouseShieldVisible", m_shieldDarkened);
            }
        }
        if (m_shield) {
            if (auto* placeholder = m_shield->findChild<QQuickItem*>(QStringLiteral("placeholder")))
                parentItem = placeholder;
        }
    }
    m_item = createItem(stack->engine(), m_qmlFile, parentItem, this, properties);
    if (m_item)
        m_item->setVisible(false);
}

void TDialog::show()
{
    if (TContextMenuController* menu = TContextMenuController::instance())
        menu->onRequestClose();
    create();
    if (TDialogStackController* stack = TDialogStackController::instance())
        stack->show(this);
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setDefaultCursor();
}

void TDialog::hide()
{
    if (TDialogStackController* stack = TDialogStackController::instance(); stack && stack->contains(this)) {
        stack->hide(this);
        return;
    }
    setVisibleByStack(false);
}

void TDialog::close()
{
    destroy();
    Q_EMIT m_controller->dialogClosed();
}

void TDialog::destroy()
{
    if (m_item)
        hide();
    deleteItem(m_item);
    deleteItem(m_shield);
}

void TDialog::setMouseShieldVisible(const bool visible)
{
    m_shieldDarkened = visible;
    if (m_shield)
        m_shield->setProperty("mouseShieldVisible", visible);
}

bool TDialog::isShown() const
{
    return m_item && m_item->isVisible();
}

bool TDialog::isInStack() const
{
    const TDialogStackController* stack = TDialogStackController::instance();
    return stack && stack->contains(this);
}

void TDialog::setVisibleByStack(const bool visible)
{
    if (!m_item || m_item->isVisible() == visible)
        return;
    if (visible)
        m_hooks->beforeShow();
    else
        m_hooks->beforeHide();
    setItemsVisible(visible);
    if (visible) {
        // CIP TPreventAutoCloseEventFilter (0x100b74020) on the dialog item.
        if (!m_item->property("_modPreventAutoClose").isValid()) {
            auto* filter = new TPreventAutoCloseEventFilter(m_item);
            m_item->installEventFilter(filter);
            m_item->setProperty("_modPreventAutoClose", QVariant::fromValue(filter));
        }
        m_hooks->afterShow();
    } else
        m_hooks->afterHide();
}

void TDialog::setItemsVisible(const bool visible)
{
    if (m_shield)
        m_shield->setVisible(visible);
    m_item->setVisible(visible);
}

}
