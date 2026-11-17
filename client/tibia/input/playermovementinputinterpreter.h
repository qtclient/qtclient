#pragma once

#include <cip_gen.h>

#include <client/const.h>

#include "input/gameaction.h"
#include "input/keyboardstatusstorage.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace tibia {
namespace input {

/// Keyboard walking. The key status (TKeyboardStatusStorage, fed by
/// TKeyEventHandler) tracks the keys this interpreter names: the keys of a
/// direction in the active chat mode — the arrows (keypad flag ignored),
/// keypad Home / PageUp / End / PageDown for the diagonals, and the current
/// preset's single-key Go* bindings (key code only, modifiers dropped, keypad
/// flag kept) — and the enabled rotateWith* modifiers. A direction walks with
/// no Shift/Ctrl/Alt/Meta/AltGr held; a cardinal one turns instead when an
/// enabled rotateWith* modifier is held and no disabled one is.
///
/// The newest status key that is a direction key is matched in the order N, S,
/// W, E, NW, NE, SW, SE to walk, else N, E, W, S to turn
/// (TKeyboardInputToGameActionConverter), else stop. A changed intent goes to
/// the movement intent handler, which repeats a held direction.
class TPlayerMovementInputInterpreter final : public cip::tibia::input::TPlayerMovementInputInterpreter
{
    Q_OBJECT

Q_SIGNALS:
    /// CIP publishGameAction; connected to the game action router in createGameWindow.
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);

public:
    /// `chatMode`: tibia::chat::EChatMode (0 off).
    TPlayerMovementInputInterpreter(std::shared_ptr<TKeyboardStatusStorage> pKeyboardStatusStorage, std::function<int()> chatMode,
                                    QObject* parent = nullptr);
    ~TPlayerMovementInputInterpreter() override;

public Q_SLOTS:
    /// The walk keys of the preset and chat mode; the status tracks them.
    void onHotkeyOptionsChanged();

private:
    using TKey = TKeyboardStatusStorage::TKey;
    /// tibia::client::North .. tibia::client::NorthWest.
    using TDirectionKeys = std::array<std::vector<TKey>, 8>;

    void onOptionsChanged();
    enum class EIntentKind { Stop, Walk, Rotate };
    struct TIntent
    {
        EIntentKind kind = EIntentKind::Stop;
        tibia::client::Direction direction = tibia::client::InvalidDirection;
        bool operator==(const TIntent&) const = default;
    };

    void update();
    const TDirectionKeys& currentDirections() const;
    /// The status holds `key` (no keypad flag).
    bool statusHolds(int key) const;
    /// CIP: no disabled rotateWith* modifier in the status, and an enabled one.
    bool rotates() const;

    std::shared_ptr<TKeyboardStatusStorage> m_pKeyboardStatusStorage;
    std::function<int()> m_chatMode;
    /// [chat off, chat on].
    std::array<TDirectionKeys, 2> m_directions;
    bool m_rotateWithCtrl = true;
    bool m_rotateWithShift = false;
    bool m_rotateWithAlt = false;
    TIntent m_intent;
};

}
}
