#pragma once

#include <cip_gen.h>

#include <QPointer>

class QQuickWindow;

namespace tibia {
namespace input {

/// CIP input::TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter
/// (eventFilter): while installed, every key press is eaten;
/// Escape also emits escapePressed.
class TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter final
    : public cip::tibia::input::TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter
{
    Q_OBJECT
public:
    explicit TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/// CIP input::TDiscardMouseWheelEventFilter (eventFilter): eats
/// wheel events so dialogs / overlays do not scroll the world under them.
class TDiscardMouseWheelEventFilter final : public cip::tibia::input::TDiscardMouseWheelEventFilter
{
    Q_OBJECT
public:
    explicit TDiscardMouseWheelEventFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/// CIP input::TDiscardUnhandledMouseButtonsEventFilter (eventFilter):
/// eats mouse buttons other than left / right / middle.
class TDiscardUnhandledMouseButtonsEventFilter final
    : public cip::tibia::input::TDiscardUnhandledMouseButtonsEventFilter
{
    Q_OBJECT
public:
    explicit TDiscardUnhandledMouseButtonsEventFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/// CIP input::TWindowFocusEventFilter: emits windowFocusLost when the game
/// window deactivates (CIP vtable).
class TWindowFocusEventFilter final : public cip::tibia::input::TWindowFocusEventFilter
{
    Q_OBJECT
public:
    explicit TWindowFocusEventFilter(QQuickWindow* window, QObject* parent = nullptr);
    ~TWindowFocusEventFilter() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPointer<QQuickWindow> m_window;
};

/// CIP input::TEventFilterManager (block / unblock):
/// dialog / UI-focus path that drops hotkeys and moves keyboard focus off or
/// onto the chat input (releaseElementFocus / requestElementFocus).
class TEventFilterManager final : public cip::tibia::input::TEventFilterManager
{
    Q_OBJECT
public:
    explicit TEventFilterManager(QObject* parent = nullptr);

    /// CIP .
    void block();
    /// CIP .
    void unblock();
    bool isBlocked() const { return m_blocked; }

    static TEventFilterManager* instance();

private:
    bool m_blocked = false;
};

}
}
