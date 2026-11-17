#include "keyboardshortcuthandlereventfilter.h"

#include "gamewindow/dialogmanager.h"
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
    // Escape first closes the topmost shielded dialog of the dialog stack.
    if (key == Qt::Key_Escape) {
        if (gamewindow::TDialogStackController* stack = gamewindow::TDialogStackController::instance();
            stack && stack->closeTopShieldedDialog())
            return true;
    }
    if (m_keySequences.count(keys) == 0 || !m_keyboardShortcutHandler)
        return QObject::eventFilter(watched, event);
    return m_keyboardShortcutHandler->keyPressed(keys, keyEvent->isAutoRepeat());
}

}
