#include "dialogmanager.h"

#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameaction.h"
#include "input/eventfiltermanager.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/soundhelper.h"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QThread>
#include <QDebug>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

std::shared_ptr<TDialogStackController> g_instance;

constexpr auto ModuleUrl = "qrc:/qt/qml/qmlcomponents/qml/";

QUrl urlOf(const QString& qmlFile)
{
    return QUrl(qmlFile.contains(QLatin1Char(':')) ? qmlFile : QLatin1String(ModuleUrl) + qmlFile);
}

}

/// CIP / : an item of `qmlFile` under `parentItem`,
/// owned by `owner` (also the info banner's wrapper maker).
QQuickItem* createItem(QQmlEngine* engine, const QString& qmlFile, QQuickItem* parentItem, QObject* owner, const QVariantMap& properties)
{
    QQmlComponent component(engine, urlOf(qmlFile));
    QObject* object = component.beginCreate(engine->rootContext());
    auto* item = qobject_cast<QQuickItem*>(object);
    if (!item) {
        qCritical() << "Component creation error for " << urlOf(qmlFile).url() << "\n" << component.errors();
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

namespace {

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

TDialogStackController::TDialogStackController(std::shared_ptr<input::TEventFilterManager> EventFilterManager, QObject* parent)
    : cip::tibia::gamewindow::TDialogStackController(parent)
    , m_pEventFilterManager(std::move(EventFilterManager))
{
    ownProductSurface(this);
}

TDialogStackController::~TDialogStackController() = default;

void TDialogStackController::setInstance(std::shared_ptr<TDialogStackController> DialogStackController)
{
    g_instance = std::move(DialogStackController);
}

TDialogStackController* TDialogStackController::instance()
{
    return g_instance.get();
}

void TDialogStackController::setDialogPlaceholder(QQmlEngine* engine, QQuickItem* dialogPlaceholder)
{
    m_engine = engine;
    m_placeholder = dialogPlaceholder;
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

bool TDialogStackController::hasDialogWithoutPersonalMouseShield() const
{
    return std::any_of(m_dialogs.begin(), m_dialogs.end(),
                       [](const TDialog* dialog) { return !dialog->hooks()->hasPersonalMouseShield(); });
}

void TDialogStackController::dialogTookFocus(const TDialog* dialog)
{
    if (!m_dialogs.empty() && m_dialogs.back() == dialog)
        if (input::TEventFilterManager* eventFilters = m_pEventFilterManager.get())
            eventFilters->requestFocus();
}

void TDialogStackController::dialogReleasedFocus(const TDialog* dialog)
{
    if (!m_dialogs.empty() && m_dialogs.back() == dialog)
        if (input::TEventFilterManager* eventFilters = m_pEventFilterManager.get())
            eventFilters->releaseFocus();
}

bool TDialogStackController::closeTopShieldedDialog()
{
    for (auto it = m_dialogs.rbegin(); it != m_dialogs.rend(); ++it) {
        TDialog* dialog = *it;
        if (dialog && dialog->hooks()->hasPersonalMouseShield() && dialog->isShown()) {
            dialog->close();
            return true;
        }
    }
    return false;
}

void TDialogStackController::hideAll()
{
    while (!m_dialogs.empty())
        m_dialogs.back()->hide();
}

void TDialogStackController::update()
{
    // A top dialog without a personal mouse shield blocks the keyboard; an
    // empty stack or a shielded top dialog unblocks it.
    if (input::TEventFilterManager* eventFilters = m_pEventFilterManager.get()) {
        if (!m_dialogs.empty() && !m_dialogs.back()->hooks()->hasPersonalMouseShield())
            eventFilters->block();
        else
            eventFilters->unblock();
    }
    for (TDialog* dialog : m_dialogs)
        dialog->setVisibleByStack(dialog == m_dialogs.back());
    Q_EMIT dialogVisible(!m_dialogs.empty());
}

TDialog::TDialog(Controller* controller, TDialogHooks* hooks, const QString& qmlFile, QQmlEngine* engine,
                 QQuickItem* dialogParentItem)
    : QObject(controller)
    , m_controller(controller)
    , m_hooks(hooks)
    , m_engine(engine)
    , m_qmlFile(qmlFile)
    , m_dialogParentItem(dialogParentItem)
{}

TDialog::~TDialog()
{
    if (TDialogStackController* stack = TDialogStackController::instance())
        stack->remove(this);
}

void TDialog::create()
{
    TDialogStackController* stack = TDialogStackController::instance();
    if (m_item || !stack)
        return;
    QQmlEngine* const engine = m_engine ? m_engine : stack->engine();
    QQuickItem* parentItem = m_dialogParentItem ? m_dialogParentItem.data() : stack->dialogPlaceholder();
    if (!engine || !parentItem)
        return;
    QVariantMap properties = m_hooks->initialDialogProperties();
    if (m_hooks->hasPersonalMouseShield()) {
        properties.insert(QStringLiteral("personalMouseShield"), true);
    } else {
        if (!m_shield) {
            m_shield = createItem(engine, QStringLiteral("TibiaMouseShield.qml"), parentItem, this, {});
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
    m_item = createItem(engine, m_qmlFile, parentItem, this, properties);
    if (m_item) {
        m_item->setVisible(false);
        m_hooks->afterCreate();
    }
}

void TDialog::show()
{
    if (TContextMenuController* menu = TContextMenuController::instance())
        menu->onRequestClose();
    create();
    if (TDialogStackController* stack = TDialogStackController::instance())
        stack->show(this);
    if (qmlcomponents::TSoundHelperService* sound = qmlcomponents::TSoundHelperService::instance())
        sound->playSound(qmlcomponents::TSoundHelperService::OPEN_DIALOG_OR_WIDGET);
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
    if (m_item) {
        m_hooks->beforeDestroy();
        hide();
    }
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
        // CIP TPreventAutoCloseEventFilter on the dialog item.
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
