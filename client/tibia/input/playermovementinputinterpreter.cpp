#include "playermovementinputinterpreter.h"

#include "chat/chatmodemanager.h"
#include "config/clientoptions.h"
#include "config/hotkeyoptions.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "input/inputciptypes.h"


#include <algorithm>
#include <optional>
#include <unordered_map>
#include <client/staticdata.h>
#include "container/containerciptypes.h"
#include <tibia/game/gamenotify.h>

namespace tibia::input {

namespace {

const std::unordered_map<QString, tibia::client::Direction>& goActions()
{
    static const std::unordered_map<QString, tibia::client::Direction> actions{
        { QStringLiteral("GoNorth"), tibia::client::North },         { QStringLiteral("GoEast"), tibia::client::East },
        { QStringLiteral("GoSouth"), tibia::client::South },         { QStringLiteral("GoWest"), tibia::client::West },
        { QStringLiteral("GoNorthEast"), tibia::client::NorthEast }, { QStringLiteral("GoSouthEast"), tibia::client::SouthEast },
        { QStringLiteral("GoSouthWest"), tibia::client::SouthWest }, { QStringLiteral("GoNorthWest"), tibia::client::NorthWest },
    };
    return actions;
}

}

TPlayerMovementInputInterpreter::TPlayerMovementInputInterpreter(std::shared_ptr<TKeyboardStatusStorage> pKeyboardStatusStorage,
                                                                 config::TClientOptions* pClientOptions,
                                                                 chat::TChatModeManager* pChatModeManager, QObject* parent)
    : cip::tibia::input::TPlayerMovementInputInterpreter(parent)
    , m_pKeyboardStatusStorage(std::move(pKeyboardStatusStorage))
    , m_pClientOptions(pClientOptions)
    , m_pChatModeManager(pChatModeManager)
{
    if (!m_pClientOptions || !m_pChatModeManager)
        return;
    connect(m_pClientOptions, &config::TClientOptions::hotkeyOptionsChanged, this,
            &TPlayerMovementInputInterpreter::onHotkeyOptionsChanged);
    connect(m_pClientOptions, &config::TClientOptions::rotateKeyOptionsChanged, this,
            &TPlayerMovementInputInterpreter::onHotkeyOptionsChanged);
    connect(m_pChatModeManager, &chat::TChatModeManager::chatModeChanged, this,
            &TPlayerMovementInputInterpreter::onHotkeyOptionsChanged);
    onHotkeyOptionsChanged();
}

TPlayerMovementInputInterpreter::~TPlayerMovementInputInterpreter() = default;

void TPlayerMovementInputInterpreter::onHotkeyOptionsChanged()
{
    // : the fixed keys, then the preset's Go* bindings.
    const std::array<std::pair<tibia::client::Direction, TKey>, 12> fixed{ {
        { tibia::client::West, { Qt::Key_Left, false } },
        { tibia::client::West, { Qt::Key_Left, true } },
        { tibia::client::East, { Qt::Key_Right, false } },
        { tibia::client::East, { Qt::Key_Right, true } },
        { tibia::client::North, { Qt::Key_Up, false } },
        { tibia::client::North, { Qt::Key_Up, true } },
        { tibia::client::South, { Qt::Key_Down, false } },
        { tibia::client::South, { Qt::Key_Down, true } },
        { tibia::client::NorthWest, { Qt::Key_Home, true } },
        { tibia::client::NorthEast, { Qt::Key_PageUp, true } },
        { tibia::client::SouthWest, { Qt::Key_End, true } },
        { tibia::client::SouthEast, { Qt::Key_PageDown, true } },
    } };
    const config::THotkeySet set = m_pClientOptions->currentHotkeySet();
    for (size_t mode = 0; mode < m_directions.size(); ++mode) {
        TDirectionKeys& directions = m_directions[mode];
        for (auto& keys : directions)
            keys.clear();
        for (const auto& [direction, key] : fixed)
            directions[direction].push_back(key);
        for (const config::THotkeyBinding& binding : mode == 0 ? set.chatOff : set.chatOn) {
            if (binding.setting.kind != config::TActionSetting::ACTION || binding.keys.count() != 1)
                continue;
            const auto it = goActions().find(binding.setting.action);
            if (it == goActions().end())
                continue;
            const int combined = binding.keys[0].toCombined();
            directions[it->second].push_back({ combined & 0x1ffffff, (combined & Qt::KeypadModifier) != 0 });
        }
    }
    m_rotateWithCtrl = m_pClientOptions->boolOption(config::EClientOption::rotateWithCtrl);
    m_rotateWithShift = m_pClientOptions->boolOption(config::EClientOption::rotateWithShift);
    m_rotateWithAlt = m_pClientOptions->boolOption(config::EClientOption::rotateWithAlt);
    // The status tracks every direction key of the chat mode, then the enabled
    // rotate modifiers.
    std::vector<TKey> tracked;
    for (const std::vector<TKey>& keys : currentDirections())
        tracked.insert(tracked.end(), keys.begin(), keys.end());
    if (m_rotateWithCtrl)
        tracked.push_back({ Qt::Key_Control, false });
    if (m_rotateWithShift)
        tracked.push_back({ Qt::Key_Shift, false });
    if (m_rotateWithAlt)
        tracked.push_back({ Qt::Key_Alt, false });
    m_pKeyboardStatusStorage->setTrackedKeys(tracked);
}

bool TPlayerMovementInputInterpreter::rotates() const
{
    const std::array<std::pair<int, bool>, 3> rotation{ { { Qt::Key_Control, m_rotateWithCtrl },
                                                          { Qt::Key_Shift, m_rotateWithShift },
                                                          { Qt::Key_Alt, m_rotateWithAlt } } };
    for (const auto& [key, enabled] : rotation) {
        if (!enabled && statusHolds(key))
            return false;
    }
    for (const auto& [key, enabled] : rotation) {
        if (enabled && statusHolds(key))
            return true;
    }
    return false;
}

const TPlayerMovementInputInterpreter::TDirectionKeys& TPlayerMovementInputInterpreter::currentDirections() const
{
    return m_directions[m_pChatModeManager && m_pChatModeManager->chatMode() != 0 ? 1 : 0];
}

bool TPlayerMovementInputInterpreter::statusHolds(const int key) const
{
    return m_pKeyboardStatusStorage->isPressed({ key, false });
}

std::optional<TPlayerMovementInputInterpreter::TKey> TPlayerMovementInputInterpreter::newestWalkKey() const
{
    std::vector<TKey> walkKeys;
    for (const std::vector<TKey>& keys : currentDirections())
        walkKeys.insert(walkKeys.end(), keys.begin(), keys.end());
    return m_pKeyboardStatusStorage->mostRecentlyPressed(walkKeys);
}

bool TPlayerMovementInputInterpreter::holdsBlockingModifier() const
{
    static constexpr std::array<int, 5> Blocking{ Qt::Key_Shift, Qt::Key_Control, Qt::Key_Alt, Qt::Key_Meta, Qt::Key_AltGr };
    return std::any_of(Blocking.begin(), Blocking.end(), [this](const int key) { return statusHolds(key); });
}

bool TPlayerMovementInputInterpreter::isNewestWalkKey(const tibia::client::Direction direction) const
{
    const std::optional<TKey> recent = newestWalkKey();
    if (!recent)
        return false;
    const std::vector<TKey>& keys = currentDirections()[direction];
    return std::find(keys.begin(), keys.end(), *recent) != keys.end();
}

bool TPlayerMovementInputInterpreter::walks(const tibia::client::Direction direction) const
{
    return isNewestWalkKey(direction) && !holdsBlockingModifier();
}

bool TPlayerMovementInputInterpreter::turns(const tibia::client::Direction direction) const
{
    return isNewestWalkKey(direction) && rotates();
}


}
