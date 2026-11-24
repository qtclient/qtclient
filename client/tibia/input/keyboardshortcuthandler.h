#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include "config/hotkeyoptions.h"

#include <QKeySequence>
#include <QList>
#include <QStringList>

#include <functional>
#include <map>
#include <memory>

namespace tibia::shared {
class IClockService;
}

namespace tibia {
namespace input {

class TKeyboardShortcutHandlerEventFilter;

/// The game's keyboard hotkeys. The current hotkey preset's bindings of the
/// active chat mode (clientoptions.json hotkeyOptions) are kept as a key
/// sequence map; their sequences go to the application key filter
/// (TKeyboardShortcutHandlerEventFilter, installed by TEventFilterManager),
/// which hands a bound press to keyPressed. A bound press publishes the
/// setting's action: a static action of the table, TriggerActionButton_<bar>.<button>
/// as ACTION_BUTTON_TRIGGERED, or a chat text. Unbound keys and Go* pass
/// through; walking is TPlayerMovementInputInterpreter.
class TKeyboardShortcutHandler final : public cip::tibia::input::TKeyboardShortcutHandler
{
    Q_OBJECT

Q_SIGNALS:
    /// CIP publishGameAction; connected to the game action router in createGameWindow.
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);

public:
    /// `chatMode`: tibia::chat::EChatMode (0 off).
    TKeyboardShortcutHandler(std::function<int()> chatMode, std::shared_ptr<shared::IClockService> pClockService,
                             QObject* parent = nullptr);
    ~TKeyboardShortcutHandler() override;

    /// The filter TEventFilterManager installs on the application.
    TKeyboardShortcutHandlerEventFilter* keyboardShortcutEventFilter() const { return m_pEventFilter; }
    /// While shortcuts are restricted (a UI element has the keyboard) only
    /// actions allowed then run (TakeManualScreenshot).
    void setShortcutsRestricted(bool restricted) { m_areShortcutsRestricted = restricted; }

    /// A press of a bound key sequence: true when it was eaten. A repeated
    /// press runs again only for the repeating actions, at most once per
    /// interval: USE and TRY_TO_EQUIP 100 ms, USE_WITH..USE_AT_MOUSECURSOR and
    /// ACTION_BUTTON_TRIGGERED 500 ms, SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT 1 s,
    /// MINIMAP_SCROLL_* 1 ms.
    bool keyPressed(const QKeySequence& keys, bool isAutoRepeat);

public Q_SLOTS:
    /// Rebuilds the binding map from the current preset and chat mode.
    void onHotkeyOptionsChanged();

public:
    /// CIP : a static hotkey action by name, or null.
    static IGameActionPtr staticAction(const QString& name);

    /// Host extension hotkey (a pack's): binding `name` in a hotkey set runs `run`; `defaultKeys` (may be
    /// empty) runs it while unbound. `description` is "Category: text" for the hotkey options
    /// page (CIP label_gameactionidentifier_*). Key repeats are ignored.
    static void addExtensionAction(const QString& name, const QKeySequence& defaultKeys, std::function<void()> run,
                                 const QString& description = {});

    struct TExtensionActionInfo
    {
        QString name;
        QKeySequence defaultKeys;
        QString description;
    };
    /// The actions registered via addExtensionAction (for the hotkey options page).
    static QList<TExtensionActionInfo> extensionActions();
    /// CIP names plus Go* (movement pass-through) for DefaultHotkeys mode.
    static QStringList staticActionNames();

    /// TRY_TO_SWITCH_HOTKEYSET, SWITCH_TO_NEXT_HOTKEYSET and
    /// SWITCH_TO_PREVIOUS_HOTKEYSET; other actions are ignored. Connected to
    /// the router's publishGameActionInternal.
    void handleGameAction(const IGameAction& action);
    void handleGameAction(const IGameActionPtr& pGameAction)
    {
        if (pGameAction)
            handleGameAction(*pGameAction);
    }
    /// With auto-switch on, selects the set named action.text when it exists
    /// and is not current, and announces it (on the status line only when
    /// action.enable).
    void tryToSwitchHotkeySet(const TGameActionTryToSwitchHotkeySet& action);
    /// The set after (before) the current one in name order, wrapping, is
    /// selected and announced on the status line and in the console.
    void switchToNextHotkeySet();
    void switchToPreviousHotkeySet();

    /// The action a setting runs: a chat text, a static action, or an object
    /// used in the setting's way (CIP: also the action button handler's); null
    /// for any other setting.
    static IGameActionPtr createGameAction(const config::TActionSetting& setting);

private:

    TKeyboardShortcutHandlerEventFilter* m_pEventFilter;
    std::function<int()> m_chatMode;
    std::shared_ptr<shared::IClockService> m_pClockService;
    /// When a repeated press of a sequence last ran.
    std::map<QKeySequence, qint64> m_lastTriggered;
    std::map<QKeySequence, config::TActionSetting> m_actionSettings;
    bool m_areShortcutsRestricted = false;
};

}
}
