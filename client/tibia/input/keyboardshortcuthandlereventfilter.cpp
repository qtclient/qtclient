#include "keyboardshortcuthandlereventfilter.h"

#include "input/keyboardshortcuthandler.h"

#include <QKeyEvent>

namespace tibia::input {

TKeyboardShortcutHandlerEventFilter::TKeyboardShortcutHandlerEventFilter(TKeyboardShortcutHandler* keyboardShortcutHandler,
                                                                         QObject* parent)
    : cip::tibia::input::TKeyboardShortcutHandlerEventFilter(parent)
    , m_keyboardShortcutHandler(keyboardShortcutHandler)
{}

bool TKeyboardShortcutHandlerEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress || m_keySequences.empty())
        return QObject::eventFilter(watched, event);
    const auto* keyEvent = static_cast<QKeyEvent*>(event);
    const int key = keyEvent->key();
    if (key >= Qt::Key_Shift && key <= Qt::Key_Alt)
        return QObject::eventFilter(watched, event);
    const QKeySequence keys(keyEvent->modifiers().toInt() | key);
    // CipSoft first lets Escape close the top dialog of the focus stack; the
    // dialog focus stack is not ported, so Escape reaches its binding.
    if (m_keySequences.count(keys) == 0 || !m_keyboardShortcutHandler)
        return QObject::eventFilter(watched, event);
    return m_keyboardShortcutHandler->keyPressed(keys, keyEvent->isAutoRepeat());
}

}
