#include "recordkeyboardshortcuthandler.h"

#include <QKeyEvent>

namespace tibia::input {

bool TRecordKeyboardShortcutHandler::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress)
        return QObject::eventFilter(watched, event);
    const auto* keyEvent = static_cast<QKeyEvent*>(event);
    const int key = keyEvent->key();
    if (key == Qt::Key_unknown)
        return QObject::eventFilter(watched, event);
    if (!keyEvent->isAutoRepeat()) {
        const bool modifierKey = key >= Qt::Key_Shift && key <= Qt::Key_Alt;
        const QKeySequence keys = modifierKey ? QKeySequence(keyEvent->modifiers().toInt())
                                              : QKeySequence(keyEvent->modifiers().toInt() | key);
        Q_EMIT shortcutPressed(keys);
    }
    return true;
}

}
