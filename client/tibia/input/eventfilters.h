#pragma once

#include <cip_gen.h>

#include <QPointer>

class QQuickWindow;

namespace tibia::input {

/// CIP input::TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter
/// (eventFilter 0x100e87530): while installed, every key press is eaten;
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

/// CIP input::TDiscardMouseWheelEventFilter (eventFilter 0x100e94d30): eats
/// wheel events so dialogs / overlays do not scroll the world under them.
class TDiscardMouseWheelEventFilter final : public cip::tibia::input::TDiscardMouseWheelEventFilter
{
    Q_OBJECT
public:
    explicit TDiscardMouseWheelEventFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/// CIP input::TDiscardUnhandledMouseButtonsEventFilter (eventFilter 0x100e94b50):
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
/// window deactivates (CIP vtable 0x100e68eb0).
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

/// CIP input::TEventFilterManager (block 0x100e87ae0 / unblock 0x100e87b50):
/// dialog / UI-focus path that drops hotkeys and moves keyboard focus off or
/// onto the chat input (releaseElementFocus / requestElementFocus).
class TEventFilterManager final : public cip::tibia::input::TEventFilterManager
{
    Q_OBJECT
public:
    explicit TEventFilterManager(QObject* parent = nullptr);

    /// CIP 0x100e87ae0.
    void block();
    /// CIP 0x100e87b50.
    void unblock();
    bool isBlocked() const { return m_blocked; }

    static TEventFilterManager* instance();

private:
    bool m_blocked = false;
};

}
