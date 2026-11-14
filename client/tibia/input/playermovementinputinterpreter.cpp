#include "playermovementinputinterpreter.h"

#include "app/clientoptions.h"
#include "config/clientoptiondefaults.h"
#include "config/hotkeyoptions.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "input/inputciptypes.h"

#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>

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

bool isArrow(const int key)
{
    return key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Left || key == Qt::Key_Right;
}

bool boolOption(const QJsonObject& options, const char* name)
{
    const QLatin1String key(name);
    return options.value(key).toBool(config::defaultClientOptions().value(key).toBool());
}

}

TPlayerMovementInputInterpreter::TPlayerMovementInputInterpreter(QQuickWindow* window, QQuickItem* chatItem, std::function<int()> chatMode,
                                                                 QObject* parent)
    : cip::tibia::input::TPlayerMovementInputInterpreter(parent)
    , m_window(window)
    , m_chatItem(chatItem)
    , m_chatMode(std::move(chatMode))
{
    config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    connect(hotkeys, &config::THotkeyOptions::mappingsChanged, this, &TPlayerMovementInputInterpreter::onHotkeyOptionsChanged);
    connect(hotkeys, &config::THotkeyOptions::keysChanged, this, &TPlayerMovementInputInterpreter::onHotkeyOptionsChanged);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name.startsWith(QLatin1String("rotateWith")))
            onOptionsChanged();
    });
    onHotkeyOptionsChanged();
    onOptionsChanged();
    if (window)
        window->installEventFilter(this);
}

TPlayerMovementInputInterpreter::~TPlayerMovementInputInterpreter()
{
    if (m_window)
        m_window->removeEventFilter(this);
}

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
}

void TPlayerMovementInputInterpreter::onOptionsChanged()
{
    const QJsonObject options = app::ClientOptions::load().options();
    m_rotateWithCtrl = boolOption(options, "rotateWithCtrl");
    m_rotateWithShift = boolOption(options, "rotateWithShift");
    m_rotateWithAlt = boolOption(options, "rotateWithAlt");
}

bool TPlayerMovementInputInterpreter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window) {
        switch (event->type()) {
            case QEvent::KeyPress: onKeyEvent(static_cast<QKeyEvent*>(event), true); break;
            case QEvent::KeyRelease: onKeyEvent(static_cast<QKeyEvent*>(event), false); break;
            case QEvent::FocusOut:
            case QEvent::WindowDeactivate: release(); break;
            default: break;
        }
    }
    return QObject::eventFilter(watched, event);
}

bool TPlayerMovementInputInterpreter::inputAllowsWalk() const
{
    if (!tibia::game::isOnline())
        return false;
    // CIP TEventFilterManager: personal-mouse-shield dialogs (NPC, etc.) leave the
    // filter up so walk/hotkeys still run; only modal / focused-UI blocks match
    // hotkeysBlocked (same gate as TKeyboardShortcutHandler).
    if (gamewindow::TDialogStackController* dialogs = gamewindow::TDialogStackController::instance(); dialogs && dialogs->hotkeysBlocked())
        return false;
    QQuickItem* focus = m_window ? m_window->activeFocusItem() : nullptr;
    return !focus || !(focus->inherits("QQuickTextInput") || focus->inherits("QQuickTextEdit")) || (m_chatItem && m_chatItem->isAncestorOf(focus));
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

tibia::client::Direction TPlayerMovementInputInterpreter::directionOf(TKey key) const
{
    if (isArrow(key.key))
        key.keypad = false;
    const TDirectionKeys& directions = m_directions[m_chatMode && m_chatMode() != 0 ? 1 : 0];
    const auto it = std::find_if(directions.begin(), directions.end(), [&key](const std::vector<TKey>& keys) {
        return std::find(keys.begin(), keys.end(), key) != keys.end();
    });
    return it == directions.end() ? tibia::client::InvalidDirection : tibia::client::Direction(it - directions.begin());
}

bool TPlayerMovementInputInterpreter::isTracked(const TKey& key) const
{
    if (directionOf(key) != tibia::client::InvalidDirection)
        return true;
    return (key.key == Qt::Key_Control && m_rotateWithCtrl) || (key.key == Qt::Key_Shift && m_rotateWithShift)
        || (key.key == Qt::Key_Alt && m_rotateWithAlt);
}

bool TPlayerMovementInputInterpreter::statusHolds(const int key) const
{
    return std::any_of(m_status.begin(), m_status.end(), [key](const TKey& held) { return held.key == key; });
}

/// CIP TKeyEventHandler::eventFilter + TKeyboardStatusStorage::v13 / v14.
void TPlayerMovementInputInterpreter::onKeyEvent(const QKeyEvent* event, const bool pressed)
{
    if (!pressed && event->isAutoRepeat())
        return;
    TKey key{ event->key(), event->modifiers().testFlag(Qt::KeypadModifier) };
    if (isArrow(key.key))
        key.keypad = false;
    if (!isTracked(key))
        return;
    const std::vector<TKey> before = m_status;
    if (pressed) {
        m_status.clear();
        static constexpr std::array<int, 5> Modifiers{ Qt::Key_Shift, Qt::Key_Control, Qt::Key_Alt, Qt::Key_Meta,
                                                       Qt::Key_AltGr };
        if (std::find(Modifiers.begin(), Modifiers.end(), key.key) == Modifiers.end())
            m_status.push_back(key);
        const Qt::KeyboardModifiers modifiers = event->modifiers();
        if (modifiers.testFlag(Qt::ControlModifier))
            m_status.push_back({ Qt::Key_Control, false });
        if (modifiers.testFlag(Qt::AltModifier))
            m_status.push_back({ Qt::Key_Alt, false });
        if (modifiers.testFlag(Qt::ShiftModifier))
            m_status.push_back({ Qt::Key_Shift, false });
        if (modifiers.testFlag(Qt::MetaModifier))
            m_status.push_back({ Qt::Key_Meta, false });
    } else if (std::find(m_status.begin(), m_status.end(), key) != m_status.end()) {
        m_status.clear();
    }
    if (m_status != before)
        update();
}

void TPlayerMovementInputInterpreter::update()
{
    // CIP TKeyboardInputToGameActionConverter::v12: the newest direction key of
    // the status walks (no modifier in the status) or turns; only a change
    // becomes a new intent (v14).
    TIntent intent;
    if (inputAllowsWalk()) {
        std::optional<TKey> recent;
        for (auto it = m_status.rbegin(); it != m_status.rend() && !recent; ++it) {
            if (directionOf(*it) != tibia::client::InvalidDirection)
                recent = *it;
        }
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
        const TDirectionKeys& directions = m_directions[m_chatMode && m_chatMode() != 0 ? 1 : 0];
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
        publish(makeAction(A::APPLY_AUTOCHASE_SETTING));
        publish(makeAction(A::STOP_FOLLOWING_CREATURE));
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

void TPlayerMovementInputInterpreter::release()
{
    m_status.clear();
    update();
}

}
