#include "eventfilters.h"

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QWheelEvent>

namespace tibia::input {

namespace {

TEventFilterManager* g_eventFilterManager = nullptr;

}

TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter::
    TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter(QObject* parent)
    : cip::tibia::input::TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter(parent)
{}

bool TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
            Q_EMIT escapePressed();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

TDiscardMouseWheelEventFilter::TDiscardMouseWheelEventFilter(QObject* parent)
    : cip::tibia::input::TDiscardMouseWheelEventFilter(parent)
{}

bool TDiscardMouseWheelEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel)
        return true;
    return QObject::eventFilter(watched, event);
}

TDiscardUnhandledMouseButtonsEventFilter::TDiscardUnhandledMouseButtonsEventFilter(QObject* parent)
    : cip::tibia::input::TDiscardUnhandledMouseButtonsEventFilter(parent)
{}

bool TDiscardUnhandledMouseButtonsEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    // A mouse event that involves any button besides left and right is sent
    // again with only its left / right bits. When no left / right bit is
    // left, the event passes through unchanged.
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        const auto button = static_cast<uint>(mouse->button());
        const auto buttons = static_cast<uint>(mouse->buttons().toInt());
        if (button <= 3 && buttons <= 3)
            break;
        const auto handledButton = static_cast<Qt::MouseButton>(button & 3);
        const auto handledButtons = Qt::MouseButtons::fromInt(static_cast<int>(buttons & 3));
        if (handledButton == Qt::NoButton && handledButtons == Qt::NoButton)
            break;
        QMouseEvent handled(event->type(), mouse->position(), mouse->scenePosition(), mouse->globalPosition(),
                            handledButton, handledButtons, mouse->modifiers(),
                            QPointingDevice::primaryPointingDevice());
        QCoreApplication::sendEvent(watched, &handled);
        return true;
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

TWindowFocusEventFilter::TWindowFocusEventFilter(QObject* parent)
    : cip::tibia::input::TWindowFocusEventFilter(parent)
{}

bool TWindowFocusEventFilter::nativeEventFilter(const QByteArray&, void*, qintptr*)
{
    return false;
}

TEventFilterManager::TEventFilterManager(QObject* parent)
    : cip::tibia::input::TEventFilterManager(parent)
{
    g_eventFilterManager = this;
}

TEventFilterManager* TEventFilterManager::instance()
{
    return g_eventFilterManager;
}

void TEventFilterManager::block()
{
    if (m_blocked)
        return;
    m_blocked = true;
    Q_EMIT releaseElementFocus();
}

void TEventFilterManager::unblock()
{
    if (!m_blocked)
        return;
    m_blocked = false;
    Q_EMIT requestElementFocus();
}

}
