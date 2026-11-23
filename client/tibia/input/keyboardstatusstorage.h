#pragma once

#include "input/eventfilters.h"

#include <cip_gen.h>

#include <list>
#include <optional>
#include <vector>

namespace tibia {
namespace input {

/// The keys the player holds, as the movement input reads them. Only tracked
/// keys (the walk keys of the chat mode and the enabled rotate modifiers) are
/// remembered, and only the last press counts: a press replaces the status with
/// that key (unless it is a modifier) plus the modifiers held, a release of any
/// held key empties it.
class TKeyboardStatusStorage final : public cip::tibia::input::TKeyboardStatusStorage
{
    Q_OBJECT
public:
    struct TKey
    {
        TKey() = default;
        /// A key of the main keyboard, or of the keypad.
        TKey(int key, bool keypad = false)
            : key(key)
            , keypad(keypad)
        {
        }
        int key = 0;
        bool keypad = false;
        /// The Qt key.
        int keyCode() const { return key; }
        /// The key is on the keypad.
        bool isKeypad() const { return keypad; }
        bool operator==(const TKey&) const = default;
        /// The main keyboard's key `qtKey`; a keypad key never is one.
        bool operator==(int qtKey) const { return !keypad && key == qtKey; }
    };

    explicit TKeyboardStatusStorage(QObject* parent = nullptr);

    /// `key` is held.
    bool isPressed(const TKey& key) const;
    /// A press of a tracked key; ignored while the application is inactive.
    void keyPressed(const TKey& key, Qt::KeyboardModifiers modifiers);
    /// A release of a tracked key: a held key empties the status.
    void keyReleased(const TKey& key);
    /// The newest held key that is one of `keys`.
    std::optional<TKey> mostRecentlyPressed(const std::vector<TKey>& keys) const;
    /// Forgets every held key, or only the keypad ones.
    void releaseKeys(bool onlyKeypadKeys);
    /// The keys to remember; a change forgets every held key.
    void setTrackedKeys(const std::vector<TKey>& keys);
    /// Forgets the held keys that are tracked.
    void releaseTrackedKeys();

public Q_SLOTS:
    /// Leaving the active state forgets every key; presses are ignored until the
    /// application is active again.
    void onApplicationStateChanged(Qt::ApplicationState NewState) override;

private:
    bool isTracked(const TKey& key) const;

    std::list<TKey> m_pressedKeys;
    std::vector<TKey> m_trackedKeys;
    bool m_isApplicationActive = true;
    /// Reports nothing on this platform.
    TWindowFocusEventFilter m_windowFocusEventFilter;
};

}
}
