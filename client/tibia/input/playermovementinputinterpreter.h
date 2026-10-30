#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <client/const.h>

#include <QElapsedTimer>
#include <QPointer>

#include <array>
#include <functional>
#include <vector>

class QKeyEvent;
class QQuickItem;
class QQuickWindow;
class QTimer;

namespace tibia::input {

/// Keyboard walking (CIP: TKeyEventHandler 0x100e6f2a0 feeding the key
/// status storage, read by TPlayerMovementInputInterpreter). The keys of a
/// direction (FUN_100e6cc40): the arrows (keypad flag ignored, list
/// DAT_1077b2638), keypad Home / PageUp / End / PageDown for the diagonals,
/// and the current preset's single-key Go* bindings of the active chat mode
/// (key code only, modifiers dropped, keypad flag kept). A direction walks
/// with no Shift/Ctrl/Alt/Meta/AltGr held (DAT_1077b2608); a cardinal one
/// turns instead when an enabled rotateWith* modifier is held and no disabled
/// one is (FUN_100e6e340). Key events are never consumed.
///
/// Holding a direction repeats it like CIP's TPlayerMovementIntentHandler tick
/// (0x100e69730): one step on the press, the next after the keyboard delay
/// (keyboardDelayUseDefault ? 250 ms (FUN_100a22280) : keyboardDelayMs), then
/// every 50 ms; the OS auto-repeat is ignored. A step waits for the walk
/// calculator (the last request is kept until then). A step onto a
/// known blocked field is not sent (0x100e6b150): with
/// alwaysTurnTowardsMoveDirection the character turns to face it
/// (FUN_100a41430: diagonals face east / west) if it does not already,
/// otherwise "sorry_not_possible" shows.
///
/// Absorbs CIP TKeyEventHandler + TPlayerMovementIntentHandler roles until
/// those are split into separate runtime objects.
class TPlayerMovementInputInterpreter final : public cip::tibia::input::TPlayerMovementInputInterpreter
{
    Q_OBJECT
public:
    /// `chatMode`: tibia::chat::EChatMode (0 off). `chatItem`: the chat whose
    /// text input does not block walking.
    TPlayerMovementInputInterpreter(QQuickWindow* window, QQuickItem* chatItem, std::function<int()> chatMode, QObject* parent = nullptr);
    ~TPlayerMovementInputInterpreter() override;

    /// Classic docked Chat or floated HudChat (whichever is the active input host).
    void setChatItem(QQuickItem* chatItem) { m_chatItem = chatItem; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct TKey
    {
        int key = 0;
        bool keypad = false;
        bool operator==(const TKey&) const = default;
    };
    /// Otc::North .. Otc::NorthWest.
    using TDirectionKeys = std::array<std::vector<TKey>, 8>;

    void onHotkeyOptionsChanged();
    void onOptionsChanged();
    void onKeyPressed(const QKeyEvent* key);
    void onKeyReleased(const QKeyEvent* key);
    /// The direction `event`'s key walks in the active chat mode, or InvalidDirection.
    Otc::Direction directionOf(const QKeyEvent* event) const;
    bool canWalk() const;
    bool rotates(Qt::KeyboardModifiers modifiers) const;
    void stopRepeat();
    /// CIP 0x100e69730.
    void onTick();
    /// CIP vtable slot 0xd0: one step, deferred while the character cannot walk.
    void step(Otc::Direction direction);

    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_chatItem;
    std::function<int()> m_chatMode;
    /// [chat off, chat on].
    std::array<TDirectionKeys, 2> m_directions;
    bool m_rotateWithCtrl = true;
    bool m_rotateWithShift = false;
    bool m_rotateWithAlt = false;
    bool m_turnTowardsMoveDirection = false;
    int m_keyboardDelayMs = 250;

    QTimer* m_timer = nullptr;
    std::vector<client::gamecore::Hook> m_hooks;
    QElapsedTimer m_elapsed;
    Otc::Direction m_held = Otc::InvalidDirection;
    int m_heldKey = 0;
    int m_repeats = 0;
    Otc::Direction m_pending = Otc::InvalidDirection;
};

}
