#include "keyboardstatusstorage.h"

#include <algorithm>

namespace tibia::input {

TKeyboardStatusStorage::TKeyboardStatusStorage(QObject* parent)
    : cip::tibia::input::TKeyboardStatusStorage(parent)
{}

bool TKeyboardStatusStorage::isTracked(const TKey& key) const
{
    return std::find(m_trackedKeys.begin(), m_trackedKeys.end(), key) != m_trackedKeys.end();
}

bool TKeyboardStatusStorage::isPressed(const TKey& key) const
{
    return std::find(m_pressedKeys.begin(), m_pressedKeys.end(), key) != m_pressedKeys.end();
}

void TKeyboardStatusStorage::keyPressed(const TKey& key, const Qt::KeyboardModifiers modifiers)
{
    if (!m_isApplicationActive || !isTracked(key))
        return;
    const std::list<TKey> before = m_pressedKeys;
    m_pressedKeys.clear();
    const bool modifierKey = (key.key >= Qt::Key_Shift && key.key <= Qt::Key_Alt) || key.key == Qt::Key_AltGr;
    if (!modifierKey)
        m_pressedKeys.push_back(key);
    if (modifiers.testFlag(Qt::ControlModifier))
        m_pressedKeys.push_back({ Qt::Key_Control, false });
    if (modifiers.testFlag(Qt::AltModifier))
        m_pressedKeys.push_back({ Qt::Key_Alt, false });
    if (modifiers.testFlag(Qt::ShiftModifier))
        m_pressedKeys.push_back({ Qt::Key_Shift, false });
    if (modifiers.testFlag(Qt::MetaModifier))
        m_pressedKeys.push_back({ Qt::Key_Meta, false });
    if (m_pressedKeys != before)
        Q_EMIT changed();
}

void TKeyboardStatusStorage::keyReleased(const TKey& key)
{
    if (!isTracked(key))
        return;
    const std::list<TKey> before = m_pressedKeys;
    if (isPressed(key))
        m_pressedKeys.clear();
    if (m_pressedKeys != before)
        Q_EMIT changed();
}

std::optional<TKeyboardStatusStorage::TKey> TKeyboardStatusStorage::mostRecentlyPressed(const std::vector<TKey>& keys) const
{
    if (keys.empty())
        return std::nullopt;
    for (auto it = m_pressedKeys.rbegin(); it != m_pressedKeys.rend(); ++it) {
        if (std::find(keys.begin(), keys.end(), *it) != keys.end())
            return *it;
    }
    return std::nullopt;
}

void TKeyboardStatusStorage::releaseKeys(const bool onlyKeypadKeys)
{
    if (m_pressedKeys.empty())
        return;
    if (!onlyKeypadKeys) {
        m_pressedKeys.clear();
        Q_EMIT changed();
        return;
    }
    const auto removed = std::erase_if(m_pressedKeys, [](const TKey& key) { return key.keypad; });
    if (removed != 0)
        Q_EMIT changed();
}

void TKeyboardStatusStorage::setTrackedKeys(const std::vector<TKey>& keys)
{
    if (m_trackedKeys == keys)
        return;
    m_trackedKeys = keys;
    releaseKeys(false);
}

void TKeyboardStatusStorage::releaseTrackedKeys()
{
    if (m_pressedKeys.empty())
        return;
    const auto removed = std::erase_if(m_pressedKeys, [this](const TKey& key) { return isTracked(key); });
    if (removed != 0)
        Q_EMIT changed();
}

void TKeyboardStatusStorage::onApplicationStateChanged(const Qt::ApplicationState NewState)
{
    m_isApplicationActive = NewState == Qt::ApplicationActive;
    if (!m_isApplicationActive)
        releaseKeys(false);
}

}
