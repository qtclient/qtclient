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

namespace tibia::input {

/// The game's keyboard hotkeys (CIP: the application key filter 0x100e85b80
/// with FUN_100e839f0). A key press looks up the current hotkey preset's
/// chatOn or chatOff bindings (clientoptions.json hotkeyOptions) and
/// publishes the bound action: the static actions of FUN_100e893b0,
/// TriggerActionButton_<bar>.<button> as ACTION_BUTTON_TRIGGERED. Unbound
/// keys and Go* pass through; walking is TPlayerMovementInputInterpreter.
class TKeyboardShortcutHandler final : public cip::tibia::input::TKeyboardShortcutHandler
{
    Q_OBJECT
public:
    /// `chatMode`: tibia::chat::EChatMode (0 off). `chatItem`: the chat
    /// whose input still gets the hotkeys while focused.
    TKeyboardShortcutHandler(QQuickWindow* window, QQuickItem* chatItem, std::function<int()> chatMode, QObject* parent = nullptr);
    ~TKeyboardShortcutHandler() override;

    /// Classic docked Chat or floated HudChat (whichever is the active input host).
    void setChatItem(QQuickItem* chatItem) { m_chatItem = chatItem; }

    /// CIP FUN_100e893b0: a static hotkey action by name, or null.
    static TGameActionPtr staticAction(const QString& name);

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
    /// CIP FUN_100e893b0 names plus Go* (movement pass-through) for DefaultHotkeys mode.
    static QStringList staticActionNames();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool handleKey(const QKeyEvent* key);
    /// CIP's auto-repeat throttle per action type (0x100e85b80): 3/9 100 ms,
    /// 4-8 and 235 500 ms, 72 1 s, 97-100 1 ms; anything else does not repeat.
    bool throttled(const TGameAction& action);

    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_chatItem;
    std::function<int()> m_chatMode;
    std::map<int, QElapsedTimer> m_lastRepeat;
};

}
