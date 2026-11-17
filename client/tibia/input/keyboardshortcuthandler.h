#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QElapsedTimer>
#include <QKeySequence>
#include <QList>
#include <QPointer>
#include <QStringList>

#include <functional>
#include <map>

class QKeyEvent;
class QQuickItem;
class QQuickWindow;

namespace tibia {
namespace input {

/// The game's keyboard hotkeys (CIP: the application key filter 
/// with). A key press looks up the current hotkey preset's
/// chatOn or chatOff bindings (clientoptions.json hotkeyOptions) and
/// publishes the bound action: the static actions of ,
/// TriggerActionButton_<bar>.<button> as ACTION_BUTTON_TRIGGERED. Unbound
/// keys and Go* pass through; walking is TPlayerMovementInputInterpreter.
class TKeyboardShortcutHandler final : public cip::tibia::input::TKeyboardShortcutHandler
{
    Q_OBJECT

Q_SIGNALS:
    /// CIP publishGameAction; connected to the game action router in createGameWindow.
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);

public:
    /// `chatMode`: tibia::chat::EChatMode (0 off). `chatItem`: the chat
    /// whose input still gets the hotkeys while focused.
    TKeyboardShortcutHandler(QQuickWindow* window, QQuickItem* chatItem, std::function<int()> chatMode, QObject* parent = nullptr);
    ~TKeyboardShortcutHandler() override;

    /// Classic docked Chat or floated HudChat (whichever is the active input host).
    void setChatItem(QQuickItem* chatItem) { m_chatItem = chatItem; }

    /// CIP : a static hotkey action by name, or null.
    static IGameActionPtr staticAction(const QString& name);

    /// product product hotkey: binding `name` in a hotkey set runs `run`; `defaultKeys` (may be
    /// empty) runs it while unbound. `description` is "Category: text" for the hotkey options
    /// page (CIP label_gameactionidentifier_*). Key repeats are ignored.
    static void addProductAction(const QString& name, const QKeySequence& defaultKeys, std::function<void()> run,
                                 const QString& description = {});

    struct TProductActionInfo
    {
        QString name;
        QKeySequence defaultKeys;
        QString description;
    };
    /// Product actions registered via addProductAction (for the hotkey options page).
    static QList<TProductActionInfo> productActions();
    /// CIP names plus Go* (movement pass-through) for DefaultHotkeys mode.
    static QStringList staticActionNames();

    /// TRY_TO_SWITCH_HOTKEYSET, SWITCH_TO_NEXT_HOTKEYSET and
    /// SWITCH_TO_PREVIOUS_HOTKEYSET; other actions are ignored.
    static void handleGameAction(const IGameAction& action);
    /// With auto-switch on, selects the set named action.text when it exists
    /// and is not current, and announces it (on the status line only when
    /// action.enable).
    static void tryToSwitchHotkeySet(const TGameActionTryToSwitchHotkeySet& action);
    /// The set after (before) the current one in name order, wrapping, is
    /// selected and announced on the status line and in the console.
    static void switchToNextHotkeySet();
    static void switchToPreviousHotkeySet();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool handleKey(const QKeyEvent* key);
    /// CIP's auto-repeat throttle, timed per key sequence: USE and
    /// TRY_TO_EQUIP 100 ms, the USE_WITH..USE_AT_MOUSECURSOR family and
    /// ACTION_BUTTON_TRIGGERED 500 ms, SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT 1 s,
    /// MINIMAP_SCROLL_* 1 ms; any other action does not repeat.
    bool throttled(EGameAction type, const QKeySequence& keys);

    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_chatItem;
    std::function<int()> m_chatMode;
    std::map<QKeySequence, QElapsedTimer> m_lastRepeat;
};

}
}
