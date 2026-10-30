#pragma once

#include "input/eventfilters.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QPointer>
#include <QString>
#include <QVariantMap>

#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia::gamewindow {

class TDialog;

/// CIP's TDialogStackController (ctor 0x100c4bf70, the one instance made at
/// start-up, global DAT_1077b25a0): the dialogs on screen, of which only the
/// top one shows. Also where dialogs are made: the QML engine and the game
/// window's dialogPlaceholder.
class TDialogStackController final : public cip::tibia::gamewindow::TDialogStackController
{
    Q_OBJECT
public:
    TDialogStackController(QQmlEngine* engine, QQuickItem* dialogPlaceholder, QObject* parent = nullptr);
    ~TDialogStackController() override;

    static TDialogStackController* instance();

    QQmlEngine* engine() const { return m_engine; }
    QQuickItem* dialogPlaceholder() const { return m_placeholder; }

    /// CIP 0x100c4c010: `dialog` goes on top.
    void show(TDialog* dialog);
    /// CIP 0x100c4c340.
    void hide(TDialog* dialog);
    /// Drops a dialog that is going away, without hiding it.
    void remove(TDialog* dialog);
    /// CIP 0x100c4c4c0.
    bool contains(const TDialog* dialog) const;
    bool isEmpty() const { return m_dialogs.empty(); }
    /// Hides every dialog (leaving the world).
    void hideAll();
    /// The keyboard hotkeys are off: a dialog without a personal mouse shield
    /// is on top (CIP 0x100c4c270 calls TEventFilterManager::block
    /// 0x100e87ae0, whose flag +0x13 keeps the TKeyEventHandler filter out in
    /// 0x100e87890; an empty stack or a personal-shield dialog unblocks,
    /// 0x100e87b50).
    bool hotkeysBlocked() const { return m_hotkeysBlocked; }
    /// CIP 0x100c4c400 / 0x100c4c420: the top dialog took or gave back the
    /// keyboard; TEventFilterManager 0x100e87b90 / 0x100e87bb0 then release or
    /// rebind the console's chat input (chatInputFocusChanged).
    void dialogTookFocus(const TDialog* dialog);
    void dialogReleasedFocus(const TDialog* dialog);
    /// CIP TEventFilterManager::block (0x100e87ae0) for a TUiFocusManager:
    /// the hotkeys are off and the chat input lets go of the keyboard.
    void blockKeyboard();
    /// Character selection is not a stack dialog. While it is open, walk hotkeys stay off.
    void setCharacterListOpen(bool open);
    /// CIP TEventFilterManager::unblock (0x100e87b50): the hotkeys follow
    /// the stack again and the chat input takes the keyboard back
    /// (chatInputFocusChanged(true)), once per block.
    void unblockKeyboard();

Q_SIGNALS:
    void chatInputFocusChanged(bool bound);

private:
    /// CIP 0x100c4c270: only the top dialog shows; emits dialogVisible.
    void update();

    QQmlEngine* m_engine;
    QPointer<QQuickItem> m_placeholder;
    std::vector<TDialog*> m_dialogs;
    /// CIP TEventFilterManager (0x100e87ae0 / 0x100e87b50).
    input::TEventFilterManager m_eventFilters;
    bool m_hotkeysBlocked = false;
    bool m_keyboardBlocked = false;
    bool m_characterListOpen = false;
};

/// The parts of CIP's TDialogControllerBase a dialog controller overrides
/// (its vtable slots 18 and 21 to 25).
class TDialogHooks : public ::tibia::CipTyped
{
public:
    virtual ~TDialogHooks() = default;
    /// v18 (CIP 0x100c153d0): the dialog blocks clicks itself (TibiaDialog's
    /// personalMouseShield) instead of a TibiaMouseShield over the game window.
    virtual bool hasPersonalMouseShield() const { return false; }
    /// v21 (CIP 0x100c15750): the QML item's initial properties.
    virtual QVariantMap initialDialogProperties() const = 0;
    /// v22 to v25 (CIP 0x100c15970..0x100c159a0): around every visibility change.
    virtual void beforeShow() {}
    virtual void beforeHide() {}
    virtual void afterShow() {}
    virtual void afterHide() {}
};

/// What CIP's TDialogControllerBase (ctor 0x100c14f00) does for a dialog
/// controller: its QML item in the game window's dialogPlaceholder, inside a
/// TibiaMouseShield unless the dialog has its own, shown through the dialog
/// stack. A child of the controller, reached through TDialogController.
class TDialog final : public QObject
{
    Q_OBJECT
public:
    using Controller = cip::tibia::gamewindow::TDialogControllerBase;

    /// `qmlFile`: a file of the qmlcomponents module ("BlessingsDialog.qml")
    /// or a full URL ("qrc:/qt/qml/mod/...").
    TDialog(Controller* controller, TDialogHooks* hooks, const QString& qmlFile);
    ~TDialog() override;

    Controller* controller() const { return m_controller; }
    TDialogHooks* hooks() const { return m_hooks; }
    QQuickItem* item() const { return m_item; }

    /// v19 (CIP 0x100c15490): makes the item, hidden, once.
    void create();
    /// v12 (CIP 0x100c150b0): closes the context menu and shows the dialog on
    /// top of the stack. Creates the item first if needed.
    void show();
    /// v13 (CIP 0x100c15260).
    void hide();
    /// v14 (CIP 0x100c152d0): destroys the item and emits dialogClosed.
    void close();
    /// v20 (CIP 0x100c158f0): hides and deletes the item and its shield.
    void destroy();
    /// v15 (CIP 0x100c152f0): darkens the game window behind the dialog.
    void setMouseShieldVisible(bool visible);
    /// v17 (CIP 0x100c153b0).
    bool isShown() const;
    /// v16 (CIP 0x100c15390).
    bool isInStack() const;

    /// CIP 0x100c15200: the stack showing or hiding this dialog.
    void setVisibleByStack(bool visible);

private:
    void setItemsVisible(bool visible);

    Controller* m_controller;
    TDialogHooks* m_hooks;
    QString m_qmlFile;
    QPointer<QQuickItem> m_item;
    QPointer<QQuickItem> m_shield;
    bool m_shieldDarkened = false;
};

/// A CIP dialog controller class (deriving from TDialogControllerBase) with
/// TDialogControllerBase's behaviour on its TDialog. Override the
/// TDialogHooks to change the QML item's properties or react to showing.
template <class Base>
class TDialogController : public Base, public TDialogHooks
{
public:
    explicit TDialogController(const QString& qmlFile, QObject* parent = nullptr)
        : Base(parent)
        , m_dialog(new TDialog(this, this, qmlFile))
    {}
    ~TDialogController() override { m_dialog->destroy(); }

    TDialog* dialog() const { return m_dialog; }

    QVariantMap initialDialogProperties() const override
    {
        return { { QStringLiteral("controller"), QVariant::fromValue<QObject*>(const_cast<TDialogController*>(this)) } };
    }

private:
    TDialog* m_dialog;
};

}
