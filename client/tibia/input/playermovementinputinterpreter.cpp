#include "playermovementinputinterpreter.h"

#include "app/clientoptions.h"
#include "config/clientoptiondefaults.h"
#include "config/hotkeyoptions.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "input/inputciptypes.h"
#include "input/playermovementgameactionhandler.h"

#include <tibia/creatures/movementtweens.h>
#include <QKeyEvent>
#include <QTimer>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>
#include <unordered_map>
#include <tibia/game/gamenotify.h>
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

/// CIP DAT_1022bdb0c, TControlsQMLOptionsPage::defaultKeyboardDelayMillis.
constexpr int DefaultKeyboardDelayMs = 250;
/// CIP : the delay between repeats after the first.
constexpr int RepeatDelayMs = 50;
constexpr int TickMs = 10;

/// CIP : the way a step makes the character face.
tibia::client::Direction facing(const tibia::client::Direction direction)
{
    switch (direction) {
        case tibia::client::NorthEast:
        case tibia::client::SouthEast: return tibia::client::East;
        case tibia::client::SouthWest:
        case tibia::client::NorthWest: return tibia::client::West;
        default: return direction;
    }
}

void showSorryNotPossible()
{
    const std::string text = qtTrId("sorry_not_possible").toStdString();
    tibia::game::notify("onTextMessage", tibia::client::MessageFailure, std::string_view(text));
}

}

TPlayerMovementInputInterpreter::TPlayerMovementInputInterpreter(QQuickWindow* window, QQuickItem* chatItem, std::function<int()> chatMode,
                                                                 QObject* parent)
    : cip::tibia::input::TPlayerMovementInputInterpreter(parent)
    , m_window(window)
    , m_chatItem(chatItem)
    , m_chatMode(std::move(chatMode))
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(TickMs);
    connect(m_timer, &QTimer::timeout, this, &TPlayerMovementInputInterpreter::onTick);
    config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    connect(hotkeys, &config::THotkeyOptions::mappingsChanged, this, &TPlayerMovementInputInterpreter::onHotkeyOptionsChanged);
    connect(hotkeys, &config::THotkeyOptions::keysChanged, this, &TPlayerMovementInputInterpreter::onHotkeyOptionsChanged);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name.startsWith(QLatin1String("rotateWith")) || name.startsWith(QLatin1String("keyboardDelay"))
            || name == QLatin1String("alwaysTurnTowardsMoveDirection"))
            onOptionsChanged();
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onServerStop", [](GameObject*, const std::vector<std::any>&) {
        tibia::game::localPlayerStopPendingWalk();
        return std::any{};
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
    m_turnTowardsMoveDirection = boolOption(options, "alwaysTurnTowardsMoveDirection");
    const QJsonValue delay = options.value(QLatin1String("keyboardDelayMs"));
    m_keyboardDelayMs = boolOption(options, "keyboardDelayUseDefault")
                            ? DefaultKeyboardDelayMs
                            : delay.toInt(config::defaultClientOptions().value(QLatin1String("keyboardDelayMs")).toInt(DefaultKeyboardDelayMs));
}

bool TPlayerMovementInputInterpreter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window) {
        switch (event->type()) {
            case QEvent::KeyPress: onKeyPressed(static_cast<QKeyEvent*>(event)); break;
            case QEvent::KeyRelease: onKeyReleased(static_cast<QKeyEvent*>(event)); break;
            case QEvent::FocusOut:
            case QEvent::WindowDeactivate: stopRepeat(); break;
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

bool TPlayerMovementInputInterpreter::rotates(const Qt::KeyboardModifiers modifiers) const
{
    const std::array<std::pair<Qt::KeyboardModifier, bool>, 3> rotation{ { { Qt::ControlModifier, m_rotateWithCtrl },
                                                                           { Qt::ShiftModifier, m_rotateWithShift },
                                                                           { Qt::AltModifier, m_rotateWithAlt } } };
    bool enabledHeld = false;
    for (const auto& [modifier, enabled] : rotation) {
        if (!modifiers.testFlag(modifier))
            continue;
        if (!enabled)
            return false;
        enabledHeld = true;
    }
    return enabledHeld;
}

tibia::client::Direction TPlayerMovementInputInterpreter::directionOf(const QKeyEvent* event) const
{
    TKey key{ event->key(), event->modifiers().testFlag(Qt::KeypadModifier) };
    if (isArrow(key.key))
        key.keypad = false;
    const TDirectionKeys& directions = m_directions[m_chatMode && m_chatMode() != 0 ? 1 : 0];
    const auto it = std::find_if(directions.begin(), directions.end(), [&key](const std::vector<TKey>& keys) {
        return std::find(keys.begin(), keys.end(), key) != keys.end();
    });
    return it == directions.end() ? tibia::client::InvalidDirection : tibia::client::Direction(it - directions.begin());
}

void TPlayerMovementInputInterpreter::onKeyPressed(const QKeyEvent* event)
{
    if (event->isAutoRepeat())
        return;
    const tibia::client::Direction direction = directionOf(event);
    if (direction == tibia::client::InvalidDirection || !inputAllowsWalk())
        return;
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if (direction <= tibia::client::West && rotates(modifiers)) {
        // CIP TMovementIntentRotate / TPlayerMovementIntent::rotate.
        const TMovementIntentRotate rotateIntent(static_cast<int>(direction));
        const TPlayerMovementIntent intent = TPlayerMovementIntent::rotate(rotateIntent.direction());
        tibia::game::turn(static_cast<tibia::client::Direction>(intent.direction()));
        return;
    }
    if (modifiers != Qt::NoModifier)
        return;
    const TMovementIntentDirection walkIntent(static_cast<int>(direction));
    const TPlayerMovementIntent intent = TPlayerMovementIntent::direction(walkIntent.direction());
    m_held = static_cast<tibia::client::Direction>(intent.direction());
    m_heldKey = event->key();
    m_repeats = 0;
    m_elapsed.restart();
    step(m_held);
    m_timer->start();
}

void TPlayerMovementInputInterpreter::onKeyReleased(const QKeyEvent* event)
{
    if (event->isAutoRepeat() || event->key() != m_heldKey)
        return;
    m_held = tibia::client::InvalidDirection;
    m_heldKey = 0;
    if (m_pending == tibia::client::InvalidDirection)
        m_timer->stop();
}

void TPlayerMovementInputInterpreter::stopRepeat()
{
    (void)TMovementIntentStop{};
    (void)TPlayerMovementIntent::stop();
    // CipSoft TMovementIntentStop: clear Target. Do not stopAutoWalk here —
    // key-up mid-hold must not cancel an unrelated click-to-walk path.
    if (TPlayerMovementGameActionHandler* movement = TPlayerMovementGameActionHandler::instance())
        movement->clearTargetIntent();
    m_held = m_pending = tibia::client::InvalidDirection;
    m_heldKey = 0;
    m_timer->stop();
}

// b1/14a6290419ebe2cc
void TPlayerMovementInputInterpreter::onTick()
{
    if (!inputAllowsWalk()) {
        stopRepeat();
        return;
    }
    if (m_held != tibia::client::InvalidDirection && m_elapsed.elapsed() > (m_repeats > 0 ? RepeatDelayMs : m_keyboardDelayMs)) {
        m_elapsed.restart();
        ++m_repeats;
        step(m_held);
    } else if (m_pending != tibia::client::InvalidDirection) {
        step(m_pending);
    }
    if (m_held == tibia::client::InvalidDirection && m_pending == tibia::client::InvalidDirection)
        m_timer->stop();
}

void TPlayerMovementInputInterpreter::step(const tibia::client::Direction direction)
{
    if (!tibia::game::isOnline())
        return;

    // CipSoft: publish stop-auto-walk (0x94) as soon as the player takes over —
    // even while the current tween is still live (canBeginStep false). Waiting until
    // canBeginStep left GoPath chaining the next prediction and ignored cancel.
    if (tibia::game::localPlayerIsAutoWalking() || tibia::game::localPlayerHasAutoWalkDirs()
        || tibia::game::isFollowing()) {
        tibia::game::interruptAutoWalk();
    } else if (TPlayerMovementGameActionHandler* movement = TPlayerMovementGameActionHandler::instance()) {
        movement->clearTargetIntent();
    }

    if (!tibia::creatures::canBeginStep()) {
        m_pending = direction;
        return;
    }
    m_pending = tibia::client::InvalidDirection;
    if (tibia::creatures::blocksStep(direction)) {
        if (m_turnTowardsMoveDirection && facing(direction) != tibia::game::localPlayerDirection())
            tibia::game::turn(facing(direction));
        else {
            const TPathPlanningFailedException error("blocked step");
            (void)error.message();
            showSorryNotPossible();
        }
        return;
    }
    publish(makeAction(A::APPLY_AUTOCHASE_SETTING));
    const TMovementIntentDirection walkIntent(static_cast<int>(direction));
    tibia::game::walk(static_cast<tibia::client::Direction>(walkIntent.direction()));
}

}
