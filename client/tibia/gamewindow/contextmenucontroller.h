#pragma once

#include "input/eventfilters.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <functional>
#include <utility>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia::input {
class TGameActionMapper;
}

namespace tibia::gamewindow {

/// One line of a context menu (CIP: TContextMenuEntryItem, ctor 0x100b7df70).
class TContextMenuEntryItem final : public cip::tibia::gamewindow::TContextMenuEntryItem
{
    Q_OBJECT
public:
    TContextMenuEntryItem(const QString& text, const QString& shortcut, bool separator, QObject* parent = nullptr);

    /// CIP 0x100b7e060 / 0x100b7e070.
    void setCheckable(bool checkable) { m_checkable = checkable; }
    void setChecked(bool checked) { m_checked = checked; }
};

/// The context menu (CIP: TContextMenuController, ctor 0x100b7e580, the root
/// context's tibiaContextMenuController): ContextMenuTemplate.qml in a
/// TibiaMouseShield over the window. A click outside, Escape or an entry
/// closes it; while it is open every key press is swallowed. It opens on the
/// next event loop pass (onAsyncShow), at the mouse, flipped left or up where
/// it would leave the window.
class TContextMenuController final : public cip::tibia::gamewindow::TContextMenuController
{
    Q_OBJECT
public:
    TContextMenuController(QQmlEngine* engine, QQuickItem* root, QObject* parent = nullptr);
    ~TContextMenuController() override;

    static TContextMenuController* instance();

    /// CIP 0x100b80dc0: one entry per action (CONTEXT_MENU_SEPARATOR is a
    /// separator); the first action a Shift, Ctrl or Alt left click would run
    /// in `context` shows that modifier. An entry publishes its action.
    void showGameActions(const input::TGameActionList& actions, int context, const input::TGameActionMapper& mapper);

    /// product product entries (the Lua client's menu hooks): `extension` runs after
    /// showGameActions' own entries, before the menu opens, while `owner` lives, and adds
    /// entries with addCustomEntry / addCustomSeparator.
    using GameActionsExtension = std::function<void(const input::TGameActionList& actions, int context, TContextMenuController& menu)>;
    void addGameActionsExtension(QObject* owner, GameActionsExtension extension);

    /// The entry text for an action (CIP: FUN_100b7f4d0); some actions also
    /// set `shortcut`.
    static QString labelOf(const input::TGameAction& action, QString& shortcut);

    /// A menu its owner builds entry by entry (CIP: 0x100b7ea40 clear,
    /// 0x100b7eb20 add entry, 0x100b7ec80 separator, 0x100b7eea0 show).
    void beginCustomMenu() { clear(); }
    TContextMenuEntryItem* addCustomEntry(const QString& text, const QString& shortcut = {}) { return addEntry(text, shortcut); }
    void addCustomSeparator() { addSeparator(); }
    void showCustomMenu() { requestShow(); }

    /// CHANGE_CONTEXT_MENU_ENABLED (CIP handleGameAction 0x100b81d40): a
    /// disabled menu never opens (the tutorial turns it off).
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

public Q_SLOTS:
    void onRequestClose() override;
    void onAsyncShow() override;
    void onCalculatePosition() override;

public:
    /// CIP 0x100b80f00: cut, copy, paste, delete and select all for a text field.
    Q_INVOKABLE void showContextMenuForTextInputQmlItem(QObject* textInput) override;

private:
    /// CIP 0x100b7ea40.
    void clear();
    /// CIP 0x100b7ec80: never first and never twice in a row.
    void addSeparator();
    /// CIP 0x100b7eb20: the entry closes the menu when triggered.
    TContextMenuEntryItem* addEntry(const QString& text, const QString& shortcut = {});
    void requestShow();
    void setKeyboardBlocked(bool blocked);

    QPointer<QQuickItem> m_root;
    QPointer<QQuickItem> m_shield;
    QPointer<QQuickItem> m_menu;
    QTimer m_showTimer;
    /// CIP TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter (0x100e87530).
    input::TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter m_discardKeys;
    bool m_keyboardBlocked = false;
    bool m_enabled = true;
    std::vector<std::pair<QPointer<QObject>, GameActionsExtension>> m_extensions;
};

}
