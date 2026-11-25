// Host API v9: the dialog table (abi/dialog_api.h). A pack dialog is a host
// TDialogController over CipSoft's TDialogControllerBase whose item gets the pack's
// controller as `controller` (TDialogHooks forwarded to the pack); message boxes are
// CipSoft's TMessageDialogController; the stack is the client's TDialogStackController; host
// dialogs are the game window's CipSoft dialog controllers.
#include "mods/abi_internal.h"

#include "app/sessionbootstrap.h"
#include "client/gameclient.h"
#include "gamewindow/createcharacterdialogcontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/messagedialogcontroller.h"

#include <QJsonObject>
#include <QKeySequence>
#include <QMetaMethod>
#include <QPointer>
#include <QQuickItem>
#include <QString>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <map>
#include <memory>

/// The ABI's opaque dialog handle (abi/dialog_api.h): a pack dialog or a message box.
struct QtClientDialog
{
    QPointer<QObject> controller;
    tibia::gamewindow::TDialog* dialog = nullptr;
    bool messageBox = false;
};

namespace tibia::mods {
namespace {

using gamewindow::TDialog;
using gamewindow::TDialogStackController;

/// Emits `signal` (no arguments) on `object` when its class declares it.
void emitByName(QObject* object, const char* signal)
{
    if (!object)
        return;
    const QMetaObject* meta = object->metaObject();
    const int index = meta->indexOfSignal(QMetaObject::normalizedSignature(QByteArray(signal) + "()").constData());
    if (index >= 0)
        meta->method(index).invoke(object, Qt::DirectConnection);
}

/// A pack dialog: CipSoft's TDialogControllerBase behaviour, the pack's controller in QML.
class TPackDialogController final : public gamewindow::TDialogController<cip::tibia::gamewindow::TDialogControllerBase>
{
public:
    TPackDialogController(const QtClientDialogDescV1& desc, QVariantMap properties)
        : TDialogController(QString::fromUtf8(desc.qml_url ? desc.qml_url : ""))
        , m_pack(static_cast<QObject*>(desc.controller))
        , m_properties(std::move(properties))
        , m_flags(desc.flags)
    {
        if (desc.hooks.struct_size != 0)
            std::memcpy(&m_hooks, &desc.hooks, std::min(desc.hooks.struct_size, sizeof(m_hooks)));
        // TDialog::close emits dialogClosed on this controller: the pack's controller and
        // its `closed` hook hear it.
        QObject::connect(this, &cip::tibia::gamewindow::TDialogControllerBase::dialogClosed, this, [this] {
            emitByName(m_pack, "dialogClosed");
            if (m_hooks.closed)
                m_hooks.closed(m_hooks.user);
        });
    }

    bool hasPersonalMouseShield() const override { return (m_flags & QTCLIENT_DIALOG_FLAG_PERSONAL_MOUSE_SHIELD) != 0; }

    QVariantMap initialDialogProperties() const override
    {
        QVariantMap properties = m_properties;
        properties.insert(QStringLiteral("controller"), QVariant::fromValue<QObject*>(m_pack.data()));
        return properties;
    }

    void afterCreate() override
    {
        if (m_hooks.after_create)
            m_hooks.after_create(m_hooks.user, dialog()->item());
    }
    void beforeDestroy() override
    {
        if (m_hooks.before_destroy)
            m_hooks.before_destroy(m_hooks.user);
    }
    void beforeShow() override
    {
        if (m_hooks.before_show)
            m_hooks.before_show(m_hooks.user);
    }
    void afterShow() override
    {
        if (m_hooks.after_show)
            m_hooks.after_show(m_hooks.user);
    }
    void beforeHide() override
    {
        if (m_hooks.before_hide)
            m_hooks.before_hide(m_hooks.user);
    }
    void afterHide() override
    {
        if (m_hooks.after_hide)
            m_hooks.after_hide(m_hooks.user);
    }

    /// Destroy: no hook runs any more.
    void dropHooks() { m_hooks = QtClientDialogHooksV1{}; }

private:
    QPointer<QObject> m_pack;
    QVariantMap m_properties;
    int m_flags = 0;
    QtClientDialogHooksV1 m_hooks{};
};

TDialog* dialogOf(QtClientDialog* handle)
{
    return handle && handle->controller ? handle->dialog : nullptr;
}

QtClientDialog* dlgCreate(const QtClientDialogDescV1* desc)
{
    if (!desc || !desc->qml_url || !*desc->qml_url || !desc->controller || !hostObjects())
        return nullptr;
    QJsonValue extra;
    if (!parseJsonValue(desc->initial_properties_json, extra) || !extra.isObject())
        return nullptr;
    auto* controller = new TPackDialogController(*desc, extra.toObject().toVariantMap());
    if (QObject* parent = hostObjects()->lifetimeParent)
        controller->setParent(parent);
    return new QtClientDialog{ controller, controller->dialog(), false };
}

void dlgDestroy(QtClientDialog* handle)
{
    if (!handle)
        return;
    if (handle->controller) {
        if (auto* pack = dynamic_cast<TPackDialogController*>(handle->controller.data())) {
            // before_destroy runs (TDialog::destroy in the controller's destructor), the rest not.
            pack->dialog()->destroy();
            pack->dropHooks();
            delete pack;
        } else if (handle->messageBox) {
            // A message box: its dialogClosed handler runs `closed` and deletes it.
            if (TDialog* dialog = dialogOf(handle))
                dialog->close();
            return;
        }
    }
    delete handle;
}

void dlgShow(QtClientDialog* handle)
{
    if (TDialog* dialog = dialogOf(handle))
        dialog->show();
}
void dlgHide(QtClientDialog* handle)
{
    if (TDialog* dialog = dialogOf(handle))
        dialog->hide();
}
void dlgClose(QtClientDialog* handle)
{
    if (TDialog* dialog = dialogOf(handle))
        dialog->close();
}
int dlgIsShown(QtClientDialog* handle)
{
    const TDialog* dialog = dialogOf(handle);
    return dialog && dialog->isShown() ? 1 : 0;
}
int dlgIsInStack(QtClientDialog* handle)
{
    const TDialog* dialog = dialogOf(handle);
    return dialog && dialog->isInStack() ? 1 : 0;
}
QtClientQQuickItemPtr dlgItem(QtClientDialog* handle)
{
    const TDialog* dialog = dialogOf(handle);
    return dialog ? dialog->item() : nullptr;
}
void dlgSetMouseShieldVisible(QtClientDialog* handle, int visible)
{
    if (TDialog* dialog = dialogOf(handle))
        dialog->setMouseShieldVisible(visible != 0);
}

// --- message boxes ---

QtClientDialog* dlgMessageBox(const QtClientMessageBoxDescV1* desc)
{
    if (!desc || !hostObjects())
        return nullptr;
    using Button = gamewindow::TMessageDialogController::Button;
    using gamewindow::TMessageDialogButton;
    auto* box = new gamewindow::TMessageDialogController(hostObjects()->lifetimeParent);
    auto* handle = new QtClientDialog{ box, box->dialog(), true };
    box->setCloseOnButtonClicked(true);
    box->setMessageDialogTextFormat(
        static_cast<cip::tibia::gamewindow::TMessageDialogController::EMessageDialogTextFormat>(desc->text_format));
    QList<TMessageDialogButton*> buttons;
    for (size_t i = 0; desc->buttons && i < desc->button_count; ++i) {
        const QtClientMessageButtonV1& b = desc->buttons[i];
        const QKeySequence key = b.shortcut_utf8 && *b.shortcut_utf8
            ? QKeySequence(QString::fromUtf8(b.shortcut_utf8), QKeySequence::PortableText)
            : QKeySequence();
        buttons.append(new TMessageDialogButton(static_cast<Button>(b.id), QString::fromUtf8(b.label_utf8 ? b.label_utf8 : ""),
                                                key, box));
    }
    if (buttons.isEmpty())
        buttons.append(new TMessageDialogButton(Button::OK, qtTrId("ok"), TMessageDialogButton::RETURN_KEYS, box));
    box->setButtons(buttons);

    void* user = desc->user;
    auto* clicked = desc->clicked;
    auto* closed = desc->closed;
    auto done = std::make_shared<bool>(false);
    QObject::connect(box, &cip::tibia::gamewindow::TMessageDialogController::buttonClicked, box,
                     [user, clicked](const cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier id,
                                     const bool checked) {
                         if (clicked)
                             clicked(user, static_cast<int>(id), checked ? 1 : 0);
                     });
    const auto finish = [user, closed, done, handle] {
        if (*done)
            return;
        *done = true;
        if (closed)
            closed(user);
        delete handle;
    };
    QObject::connect(box, &cip::tibia::gamewindow::TDialogControllerBase::dialogClosed, box, [box, finish] {
        finish();
        box->deleteLater();
    });
    // The game window going away deletes the box without closing it.
    QObject::connect(box, &QObject::destroyed, [finish] { finish(); });

    const QString title = QString::fromUtf8(desc->title_utf8 ? desc->title_utf8 : "");
    const QString text = QString::fromUtf8(desc->text_utf8 ? desc->text_utf8 : "");
    if (desc->checkbox_utf8 && *desc->checkbox_utf8)
        box->showMessageWithCheckBox(title, text, QString::fromUtf8(desc->checkbox_utf8), false);
    else
        box->showMessage(title, text);
    return handle;
}

// --- the dialog stack ---

int dlgAnyStacked()
{
    return TDialogStackController::isAnyDialogStacked() ? 1 : 0;
}
int dlgAnyShieldingStacked()
{
    return TDialogStackController::isAnyShieldingDialogStacked() ? 1 : 0;
}
int dlgCloseTopShielded()
{
    TDialogStackController* stack = TDialogStackController::instance();
    return stack && stack->closeTopShieldedDialog() ? 1 : 0;
}

struct TStackWatch
{
    Subscribers<std::function<void(bool)>> visible;
    Subscribers<std::function<void(int, bool)>> hostShown;
    QPointer<TDialogStackController> bound;
    QMetaObject::Connection connection;
    std::array<bool, 7> shown{};
};

TStackWatch& stackWatch()
{
    static TStackWatch w;
    return w;
}

TDialog* hostDialog(const int kind)
{
    const HostObjects* host = hostObjects();
    return host && host->hostDialog ? host->hostDialog(kind) : nullptr;
}

/// The stack showed or hid its top dialog: the host dialogs whose shown state changed.
void onStackChanged(const bool visible)
{
    TStackWatch& w = stackWatch();
    w.visible.call(visible);
    for (int kind = QTCLIENT_HOST_DIALOG_BLESSINGS; kind <= QTCLIENT_HOST_DIALOG_BOSS_DIFFICULTY_SELECTION; ++kind) {
        const TDialog* dialog = hostDialog(kind);
        const bool shown = dialog && dialog->isShown();
        if (shown == w.shown[static_cast<size_t>(kind)])
            continue;
        w.shown[static_cast<size_t>(kind)] = shown;
        w.hostShown.call(kind, shown);
    }
}

void bindStack()
{
    TStackWatch& w = stackWatch();
    TDialogStackController* stack = TDialogStackController::instance();
    if (w.bound == stack)
        return;
    QObject::disconnect(w.connection);
    w.bound = stack;
    if (stack)
        w.connection = QObject::connect(stack, &cip::tibia::gamewindow::TDialogStackController::dialogVisible, stack,
                                        [](const bool visible) { onStackChanged(visible); });
}

int dlgSubscribeStackVisible(void (*fn)(void* user, int visible), void* user)
{
    if (!fn)
        return 0;
    bindStack();
    return stackWatch().visible.add([fn, user](const bool visible) { fn(user, visible ? 1 : 0); });
}

void dlgUnsubscribeStackVisible(int id)
{
    stackWatch().visible.remove(id);
}

// --- host dialogs ---

QtClientQQuickItemPtr dlgHostDialogItem(int kind)
{
    const TDialog* dialog = hostDialog(kind);
    return dialog ? dialog->item() : nullptr;
}

int dlgHostDialogShown(int kind)
{
    const TDialog* dialog = hostDialog(kind);
    return dialog && dialog->isShown() ? 1 : 0;
}

int dlgSubscribeHostDialogShown(int kind, void (*fn)(void* user, int kind, int shown), void* user)
{
    if (!fn)
        return 0;
    bindStack();
    return stackWatch().hostShown.add([kind, fn, user](const int k, const bool shown) {
        if (kind == 0 || kind == k)
            fn(user, k, shown ? 1 : 0);
    });
}

void dlgUnsubscribeHostDialogShown(int id)
{
    stackWatch().hostShown.remove(id);
}

/// The create character dialog's QML: a CipSoft TCreateCharacterDialogController made with
/// the file (CipSoft's when none), installed like the client's own.
QString& createCharacterQml()
{
    static QString url;
    return url;
}

void installCreateCharacterQml()
{
    const HostObjects* host = hostObjects();
    if (!host || !host->sessionBootstrap)
        return;
    const QString& url = createCharacterQml();
    // CipSoft's dialog file (qmlcomponents) when the pack gave its file back.
    host->sessionBootstrap->installCreateCharacter(new gamewindow::TCreateCharacterDialogController(
        url.isEmpty() ? QStringLiteral("CreateCharacterDialog.qml") : url));
}

int dlgSetHostDialogQml(int kind, const char* qml_url)
{
    if (kind != QTCLIENT_HOST_DIALOG_CREATE_CHARACTER)
        return 0;
    const QString url = qml_url ? QString::fromUtf8(qml_url) : QString();
    if (url == createCharacterQml())
        return 1;
    createCharacterQml() = url;
    installCreateCharacterQml();
    return 1;
}

const QtClientDialogApiV1 g_dialogApi = {
    .struct_size = sizeof(QtClientDialogApiV1),
    .create = &dlgCreate,
    .destroy = &dlgDestroy,
    .show = &dlgShow,
    .hide = &dlgHide,
    .close = &dlgClose,
    .is_shown = &dlgIsShown,
    .is_in_stack = &dlgIsInStack,
    .item = &dlgItem,
    .set_mouse_shield_visible = &dlgSetMouseShieldVisible,
    .message_box = &dlgMessageBox,
    .any_stacked = &dlgAnyStacked,
    .any_shielding_stacked = &dlgAnyShieldingStacked,
    .close_top_shielded = &dlgCloseTopShielded,
    .subscribe_stack_visible = &dlgSubscribeStackVisible,
    .unsubscribe_stack_visible = &dlgUnsubscribeStackVisible,
    .host_dialog_item = &dlgHostDialogItem,
    .host_dialog_shown = &dlgHostDialogShown,
    .subscribe_host_dialog_shown = &dlgSubscribeHostDialogShown,
    .unsubscribe_host_dialog_shown = &dlgUnsubscribeHostDialogShown,
    .set_host_dialog_qml = &dlgSetHostDialogQml,
};

} // namespace

void bindDialogWatchersToHost()
{
    bindStack();
    stackWatch().shown = {};
    // A new game window keeps a pack's create character dialog.
    if (hostObjects() && !createCharacterQml().isEmpty())
        installCreateCharacterQml();
}

const QtClientDialogApiV1* dialogApiV1()
{
    return &g_dialogApi;
}

} // namespace tibia::mods
