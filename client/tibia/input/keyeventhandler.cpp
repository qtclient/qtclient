#include "keyeventhandler.h"

#include "input/keyboardstatusstorage.h"

#include <QKeyEvent>

#include <algorithm>
#include <array>

namespace tibia::input {

namespace {

using TKey = TKeyboardStatusStorage::TKey;

/// The arrow keys with the keypad flag; they count as the plain arrows.
const std::array<TKey, 4>& keypadArrows()
{
    static const std::array<TKey, 4> keys{ { { Qt::Key_Left, true }, { Qt::Key_Right, true }, { Qt::Key_Up, true },
                                             { Qt::Key_Down, true } } };
    return keys;
}

}

TKeyEventHandler::TKeyEventHandler(std::shared_ptr<TKeyboardStatusStorage> pKeyboardStatusStorage)
    : cip::tibia::input::TKeyEventHandler(nullptr)
    , m_pKeyboardStatusStorage(std::move(pKeyboardStatusStorage))
{}

bool TKeyEventHandler::eventFilter(QObject*, QEvent* event)
{
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)
        return false;
    const auto* keyEvent = static_cast<QKeyEvent*>(event);
    TKey key{ keyEvent->key(), keyEvent->modifiers().testFlag(Qt::KeypadModifier) };
    if (std::find(keypadArrows().begin(), keypadArrows().end(), key) != keypadArrows().end())
        key = { key.key, false };
    if (event->type() == QEvent::KeyRelease) {
        if (!keyEvent->isAutoRepeat())
            m_pKeyboardStatusStorage->keyReleased(key);
    } else {
        if (key.key == Qt::Key_NumLock)
            m_pKeyboardStatusStorage->releaseKeys(true);
        m_pKeyboardStatusStorage->keyPressed(key, keyEvent->modifiers());
    }
    return false;
}

}
