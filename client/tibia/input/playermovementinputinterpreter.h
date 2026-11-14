#pragma once

#include <cip_gen.h>

#include <client/const.h>

#include <QPointer>

#include <array>
#include <functional>
#include <vector>

class QKeyEvent;
class QQuickItem;
class QQuickWindow;

namespace tibia {
namespace input {

/// Keyboard walking (CIP: TKeyEventHandler feeding the key status storage,
/// read by TPlayerMovementInputInterpreter). The keys of a direction: the
/// arrows (keypad flag ignored), keypad Home / PageUp / End / PageDown for the
/// diagonals, and the current preset's single-key Go* bindings of the active
/// chat mode (key code only, modifiers dropped, keypad flag kept). A direction
/// walks with no Shift/Ctrl/Alt/Meta/AltGr held; a cardinal one turns instead
/// when an enabled rotateWith* modifier is held and no disabled one is. Key
/// events are never consumed.
///
/// The key status (CIP TKeyboardStatusStorage) only remembers the last event of
/// a tracked key (the direction keys and the enabled rotateWith* modifiers):
/// a press replaces it with that key plus the modifiers held, a release of any
/// tracked key empties it. So the last pressed key wins, and releasing any
/// movement key stops, even while another is still held. The newest status key
/// that is a direction key is matched in the order N, S, W, E, NW, NE, SW, SE
/// to walk, else N, E, W, S to turn (TKeyboardInputToGameActionConverter),
/// else stop. A changed intent goes to the movement intent handler, which
/// repeats a held direction.
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
    /// tibia::client::North .. tibia::client::NorthWest.
    using TDirectionKeys = std::array<std::vector<TKey>, 8>;

    void onHotkeyOptionsChanged();
    void onOptionsChanged();
    enum class EIntentKind { Stop, Walk, Rotate };
    struct TIntent
    {
        EIntentKind kind = EIntentKind::Stop;
        tibia::client::Direction direction = tibia::client::InvalidDirection;
        bool operator==(const TIntent&) const = default;
    };

    void onKeyEvent(const QKeyEvent* event, bool pressed);
    void update();
    /// The direction `key` walks in the active chat mode, or InvalidDirection.
    tibia::client::Direction directionOf(TKey key) const;
    /// A key the status remembers: a direction key, or an enabled rotate modifier.
    bool isTracked(const TKey& key) const;
    /// The status holds `key` (CIP TKeyboardStatusStorage::v12).
    bool statusHolds(int key) const;
    bool inputAllowsWalk() const;
    /// CIP: no disabled rotateWith* modifier in the status, and an enabled one.
    bool rotates() const;
    void release();

    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_chatItem;
    std::function<int()> m_chatMode;
    /// [chat off, chat on].
    std::array<TDirectionKeys, 2> m_directions;
    bool m_rotateWithCtrl = true;
    bool m_rotateWithShift = false;
    bool m_rotateWithAlt = false;

    /// The key status, oldest first.
    std::vector<TKey> m_status;
    TIntent m_intent;
};

}
}
