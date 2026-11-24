#pragma once

#include <cip_gen.h>

#include <client/const.h>

#include "input/gameaction.h"
#include "input/keyboardstatusstorage.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace tibia::chat {
class TChatModeManager;
}
namespace tibia::config {
class TClientOptions;
}

namespace tibia {
namespace input {

/// Keyboard walking. The key status (TKeyboardStatusStorage, fed by
/// TKeyEventHandler) tracks the keys this interpreter names: the keys of a
/// direction in the active chat mode — the arrows (keypad flag ignored),
/// keypad Home / PageUp / End / PageDown for the diagonals, and the current
/// preset's single-key Go* bindings (key code only, modifiers dropped, keypad
/// flag kept) — and the enabled rotateWith* modifiers. A direction walks when
/// its key is the newest walk key of the status and no Shift/Ctrl/Alt/Meta/AltGr
/// is held; a cardinal one turns instead when an enabled rotateWith* modifier
/// is held and no disabled one is. TKeyboardInputToGameActionConverter asks
/// these predicates for the movement intent.
class TPlayerMovementInputInterpreter final : public cip::tibia::input::TPlayerMovementInputInterpreter
{
    Q_OBJECT

public:
    /// Keeps the key status, the client options (the hotkey preset, the
    /// rotateWith* options) and the chat mode manager, and takes the walk keys
    /// again when the hotkeys, the rotate options or the chat mode change.
    TPlayerMovementInputInterpreter(std::shared_ptr<TKeyboardStatusStorage> pKeyboardStatusStorage,
                                    config::TClientOptions* pClientOptions, chat::TChatModeManager* pChatModeManager,
                                    QObject* parent = nullptr);
    ~TPlayerMovementInputInterpreter() override;

    /// CIP v12: a walk key is held and no blocking modifier.
    bool walksAnyDirection() const { return newestWalkKey().has_value() && !holdsBlockingModifier(); }
    /// CIP v13..v20: the newest walk key of the status is a key of the
    /// direction and no blocking modifier is held.
    bool walksNorth() const { return walks(tibia::client::North); }
    bool walksEast() const { return walks(tibia::client::East); }
    bool walksSouth() const { return walks(tibia::client::South); }
    bool walksWest() const { return walks(tibia::client::West); }
    bool walksNorthEast() const { return walks(tibia::client::NorthEast); }
    bool walksSouthEast() const { return walks(tibia::client::SouthEast); }
    bool walksSouthWest() const { return walks(tibia::client::SouthWest); }
    bool walksNorthWest() const { return walks(tibia::client::NorthWest); }
    /// CIP v21..v24: the newest walk key of the status is a key of the
    /// direction and the rotate modifiers turn.
    bool rotatesNorth() const { return turns(tibia::client::North); }
    bool rotatesEast() const { return turns(tibia::client::East); }
    bool rotatesSouth() const { return turns(tibia::client::South); }
    bool rotatesWest() const { return turns(tibia::client::West); }

public Q_SLOTS:
    /// The walk keys of the preset and chat mode and the enabled rotate
    /// modifiers; the status tracks them.
    void onHotkeyOptionsChanged();

private:
    using TKey = TKeyboardStatusStorage::TKey;
    /// tibia::client::North .. tibia::client::NorthWest.
    using TDirectionKeys = std::array<std::vector<TKey>, 8>;

    /// The newest held walk key of the status.
    std::optional<TKey> newestWalkKey() const;
    /// The status holds Shift, Ctrl, Alt, Meta or AltGr.
    bool holdsBlockingModifier() const;
    /// The status' newest walk key is one of `direction`'s keys.
    bool isNewestWalkKey(tibia::client::Direction direction) const;
    bool walks(tibia::client::Direction direction) const;
    bool turns(tibia::client::Direction direction) const;
    const TDirectionKeys& currentDirections() const;
    /// The status holds `key` (no keypad flag).
    bool statusHolds(int key) const;
    /// CIP: no disabled rotateWith* modifier in the status, and an enabled one.
    bool rotates() const;

    std::shared_ptr<TKeyboardStatusStorage> m_pKeyboardStatusStorage;
    config::TClientOptions* m_pClientOptions = nullptr;
    chat::TChatModeManager* m_pChatModeManager = nullptr;
    /// [chat off, chat on].
    std::array<TDirectionKeys, 2> m_directions;
    bool m_rotateWithCtrl = true;
    bool m_rotateWithShift = false;
    bool m_rotateWithAlt = false;
};

}
}
