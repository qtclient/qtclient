#pragma once

#include <cip_gen.h>

#include <QAbstractNativeEventFilter>

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

/// CIP input::TDiscardMouseWheelEventFilter (eventFilter): eats wheel events
/// on the object it is installed on (dialogs / overlays). Do not install on
/// the QQuickWindow — that blocks container and list scrolling.
class TDiscardMouseWheelEventFilter final : public cip::tibia::input::TDiscardMouseWheelEventFilter
{
    Q_OBJECT
public:
    explicit TDiscardMouseWheelEventFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/// CIP input::TDiscardUnhandledMouseButtonsEventFilter (eventFilter):
/// strips every button but left / right from mouse events that carry one.
class TDiscardUnhandledMouseButtonsEventFilter final
    : public cip::tibia::input::TDiscardUnhandledMouseButtonsEventFilter
{
    Q_OBJECT
public:
    explicit TDiscardUnhandledMouseButtonsEventFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/// CIP input::TWindowFocusEventFilter: a native event filter owned by the
/// key status. It never claims an event and never reports a focus loss here.
class TWindowFocusEventFilter final : public cip::tibia::input::TWindowFocusEventFilter,
                                      public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit TWindowFocusEventFilter(QObject* parent = nullptr);

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;
};

}
}
