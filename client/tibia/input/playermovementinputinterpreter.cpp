#include "playermovementinputinterpreter.h"

#include "app/clientoptions.h"
#include "config/clientoptiondefaults.h"
#include "config/hotkeyoptions.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "input/inputciptypes.h"


#include <algorithm>
#include <optional>
#include <unordered_map>
#include <tibia/game/gamesession.h>

namespace tibia::input {

namespace {

using A = EGameAction;

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

bool boolOption(const QJsonObject& options, const char* name)
{
    const QLatin1String key(name);
    return options.value(key).toBool(config::defaultClientOptions().value(key).toBool());
}

}

TPlayerMovementInputInterpreter::TPlayerMovementInputInterpreter(std::shared_ptr<TKeyboardStatusStorage> pKeyboardStatusStorage,
                                                                 std::function<int()> chatMode, QObject* parent)
    : cip::tibia::input::TPlayerMovementInputInterpreter(parent)
    , m_pKeyboardStatusStorage(std::move(pKeyboardStatusStorage))
    , m_chatMode(std::move(chatMode))
{
    config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    connect(hotkeys, &config::THotkeyOptions::mappingsChanged, this, &TPlayerMovementInputInterpreter::onHotkeyOptionsChanged);
    connect(hotkeys, &config::THotkeyOptions::keysChanged, this, &TPlayerMovementInputInterpreter::onHotkeyOptionsChanged);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name.startsWith(QLatin1String("rotateWith")))
            onOptionsChanged();
    });
    // CIP: TKeyboardInputToGameActionConverter reads the status on every change.
    connect(m_pKeyboardStatusStorage.get(), &TKeyboardStatusStorage::changed, this, &TPlayerMovementInputInterpreter::update);
    onOptionsChanged();
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
    const config::THotkeySet& set = config::THotkeyOptions::instance()->currentSet();
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

void TPlayerMovementInputInterpreter::onOptionsChanged()
{
    const QJsonObject options = app::ClientOptions::load().options();
    m_rotateWithCtrl = boolOption(options, "rotateWithCtrl");
    m_rotateWithShift = boolOption(options, "rotateWithShift");
    m_rotateWithAlt = boolOption(options, "rotateWithAlt");
    onHotkeyOptionsChanged();
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
    return m_directions[m_chatMode && m_chatMode() != 0 ? 1 : 0];
}

bool TPlayerMovementInputInterpreter::statusHolds(const int key) const
{
    return m_pKeyboardStatusStorage->isPressed({ key, false });
}

void TPlayerMovementInputInterpreter::update()
{
    // CIP TKeyboardInputToGameActionConverter::v12: the newest direction key of
    // the status walks (no modifier in the status) or turns; only a change
    // becomes a new intent (v14).
    TIntent intent;
    {
        const TDirectionKeys& directions = currentDirections();
        std::vector<TKey> walkKeys;
        for (const std::vector<TKey>& keys : directions)
            walkKeys.insert(walkKeys.end(), keys.begin(), keys.end());
        const std::optional<TKey> recent = m_pKeyboardStatusStorage->mostRecentlyPressed(walkKeys);
        static constexpr std::array<int, 5> Blocking{ Qt::Key_Shift, Qt::Key_Control, Qt::Key_Alt, Qt::Key_Meta,
                                                      Qt::Key_AltGr };
        const bool blocked = std::any_of(Blocking.begin(), Blocking.end(), [this](const int key) { return statusHolds(key); });
        static constexpr std::array<tibia::client::Direction, 8> walking{
            tibia::client::North,     tibia::client::South,     tibia::client::West,      tibia::client::East,
            tibia::client::NorthWest, tibia::client::NorthEast, tibia::client::SouthWest, tibia::client::SouthEast,
        };
        static constexpr std::array<tibia::client::Direction, 4> turning{
            tibia::client::North, tibia::client::East, tibia::client::West, tibia::client::South,
        };
        const auto bound = [&](const tibia::client::Direction direction) {
            const std::vector<TKey>& keys = directions[direction];
            return std::find(keys.begin(), keys.end(), *recent) != keys.end();
        };
        if (recent && !blocked) {
            for (const tibia::client::Direction direction : walking) {
                if (bound(direction)) {
                    intent = { EIntentKind::Walk, direction };
                    break;
                }
            }
        } else if (recent && rotates()) {
            for (const tibia::client::Direction direction : turning) {
                if (bound(direction)) {
                    intent = { EIntentKind::Rotate, direction };
                    break;
                }
            }
        }
    }
    if (intent == m_intent)
        return;
    m_intent = intent;
    switch (intent.kind) {
    case EIntentKind::Walk:
        (void)TPlayerMovementIntent::direction(TMovementIntentDirection(static_cast<int>(intent.direction)).direction());
        Q_EMIT publishGameAction(makeGameAction(A::APPLY_AUTOCHASE_SETTING));
        Q_EMIT publishGameAction(std::make_shared<TGameActionCreature>(A::STOP_FOLLOWING_CREATURE, 0));
        tibia::game::localPlayerHoldDirection(intent.direction);
        break;
    case EIntentKind::Rotate:
        (void)TPlayerMovementIntent::rotate(TMovementIntentRotate(static_cast<int>(intent.direction)).direction());
        tibia::game::localPlayerRotate(intent.direction);
        break;
    case EIntentKind::Stop:
        (void)TPlayerMovementIntent::stop();
        tibia::game::localPlayerReleaseDirection();
        break;
    }
}


}
