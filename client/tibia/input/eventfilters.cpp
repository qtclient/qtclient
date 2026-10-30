#include "eventfilters.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickWindow>
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
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick: {
        const Qt::MouseButton button = static_cast<QMouseEvent*>(event)->button();
        if (button != Qt::LeftButton && button != Qt::RightButton && button != Qt::MiddleButton)
            return true;
        break;
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

TWindowFocusEventFilter::TWindowFocusEventFilter(QQuickWindow* window, QObject* parent)
    : cip::tibia::input::TWindowFocusEventFilter(parent)
    , m_window(window)
{
    if (window)
        window->installEventFilter(this);
}

TWindowFocusEventFilter::~TWindowFocusEventFilter()
{
    if (m_window)
        m_window->removeEventFilter(this);
}

bool TWindowFocusEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window
        && (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::FocusOut))
        Q_EMIT windowFocusLost();
    return QObject::eventFilter(watched, event);
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
