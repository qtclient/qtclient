#pragma once

#include "input/eventfilters.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QPointer>
#include <QString>
#include <QVariantMap>

#include <typeinfo>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace gamewindow {

class TDialog;

/// CIP's TDialogStackController (ctor , the one instance made at
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

    /// CIP : `dialog` goes on top.
    void show(TDialog* dialog);
    /// CIP .
    void hide(TDialog* dialog);
    /// Drops a dialog that is going away, without hiding it.
    void remove(TDialog* dialog);
    /// CIP .
    bool contains(const TDialog* dialog) const;
    /// The first stacked dialog whose controller is a `T` (not a subclass) goes on top.
    template <class T>
    void showDialogOfType();
    bool isEmpty() const { return m_dialogs.empty(); }
    /// Hides every dialog (leaving the world).
    void hideAll();
    /// Escape: closes the topmost shown dialog with a personal mouse shield;
    /// false when there is none.
    bool closeTopShieldedDialog();
    /// The top dialog's element asks for the focus or lets go
    /// (TEventFilterManager::requestFocus / releaseFocus); only for the top dialog.
    void dialogTookFocus(const TDialog* dialog);
    void dialogReleasedFocus(const TDialog* dialog);
    /// Character selection is not a stack dialog. While it is open, the keyboard is blocked.
    void setCharacterListOpen(bool open);


private:
    /// CIP : only the top dialog shows; emits dialogVisible.
    void update();

    QQmlEngine* m_engine;
    QPointer<QQuickItem> m_placeholder;
    std::vector<TDialog*> m_dialogs;
    bool m_characterListOpen = false;
};

/// The parts of CIP's TDialogControllerBase a dialog controller overrides
/// (its vtable slots 18 and 21 to 25).
class TDialogHooks : public ::tibia::CipTyped
{
public:
    virtual ~TDialogHooks() = default;
    /// v18 (CIP): the dialog blocks clicks itself (TibiaDialog's
    /// personalMouseShield) instead of a TibiaMouseShield over the game window.
    virtual bool hasPersonalMouseShield() const { return false; }
    /// v21 (CIP): the QML item's initial properties.
    virtual QVariantMap initialDialogProperties() const = 0;
    /// v22 to v25 (CIP ..): around every visibility change.
    virtual void beforeShow() {}
    virtual void beforeHide() {}
    virtual void afterShow() {}
    virtual void afterHide() {}
    /// v19 / v20 (CIP): after the item is made, before it is destroyed.
    virtual void afterCreate() {}
    virtual void beforeDestroy() {}
};

/// What CIP's TDialogControllerBase (ctor) does for a dialog
/// controller: its QML item in the game window's dialogPlaceholder, inside a
/// TibiaMouseShield unless the dialog has its own, shown through the dialog
/// stack. A child of the controller, reached through TDialogController.
class TDialog final : public QObject
{
    Q_OBJECT
public:
    using Controller = cip::tibia::gamewindow::TDialogControllerBase;

    /// `qmlFile`: a file of the qmlcomponents module ("BlessingsDialog.qml")
    /// or a full URL ("qrc:/qt/qml/mod...").
    TDialog(Controller* controller, TDialogHooks* hooks, const QString& qmlFile);
    ~TDialog() override;

    Controller* controller() const { return m_controller; }
    TDialogHooks* hooks() const { return m_hooks; }
    QQuickItem* item() const { return m_item; }

    /// v19 (CIP): makes the item, hidden, once.
    void create();
    /// v12 (CIP): closes the context menu and shows the dialog on
    /// top of the stack. Creates the item first if needed.
    void show();
    /// v13 (CIP).
    void hide();
    /// v14 (CIP): destroys the item and emits dialogClosed.
    void close();
    /// v20 (CIP): hides and deletes the item and its shield.
    void destroy();
    /// v15 (CIP): darkens the game window behind the dialog.
    void setMouseShieldVisible(bool visible);
    /// v17 (CIP).
    bool isShown() const;
    /// v16 (CIP).
    bool isInStack() const;

    /// CIP : the stack showing or hiding this dialog.
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

template <class T>
void TDialogStackController::showDialogOfType()
{
    for (TDialog* dialog : m_dialogs) {
        if (!dialog)
            continue;
        // Cache before typeid: polymorphic typeid evaluates its operand.
        TDialog::Controller* const controller = dialog->controller();
        if (controller && typeid(*controller) == typeid(T)) {
            show(dialog);
            return;
        }
    }
}

}
}
