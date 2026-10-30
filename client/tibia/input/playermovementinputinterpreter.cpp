#include "playermovementinputinterpreter.h"

#include "app/clientoptions.h"
#include "config/clientoptiondefaults.h"
#include "config/hotkeyoptions.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "input/inputciptypes.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <tibia/world/world.h>
#include <framework/core/gameevents.h>

#include <QKeyEvent>
#include <QTimer>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>
#include <unordered_map>

namespace tibia::input {

namespace {

using A = EGameAction;

const std::unordered_map<QString, Otc::Direction>& goActions()
{
    static const std::unordered_map<QString, Otc::Direction> actions{
        { QStringLiteral("GoNorth"), Otc::North },         { QStringLiteral("GoEast"), Otc::East },
        { QStringLiteral("GoSouth"), Otc::South },         { QStringLiteral("GoWest"), Otc::West },
        { QStringLiteral("GoNorthEast"), Otc::NorthEast }, { QStringLiteral("GoSouthEast"), Otc::SouthEast },
        { QStringLiteral("GoSouthWest"), Otc::SouthWest }, { QStringLiteral("GoNorthWest"), Otc::NorthWest },
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
/// CIP 0x100e69730: the delay between repeats after the first.
constexpr int RepeatDelayMs = 50;
constexpr int TickMs = 10;

/// CIP FUN_100a41430: the way a step makes the character face.
Otc::Direction facing(const Otc::Direction direction)
{
    switch (direction) {
        case Otc::NorthEast:
        case Otc::SouthEast: return Otc::East;
        case Otc::SouthWest:
        case Otc::NorthWest: return Otc::West;
        default: return direction;
    }
}

void showSorryNotPossible()
{
    const std::string text = qtTrId("sorry_not_possible").toStdString();
    g_gameEvents.publish("g_game", "onTextMessage", nullptr, { std::any(Otc::MessageFailure), std::any(std::string_view(text)) });
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
    m_hooks.emplace_back("g_game", "onServerStop", [](LuaObject*, const std::vector<std::any>&) {
        if (const LocalPlayerPtr player = g_game.getLocalPlayer())
            player->stopPendingWalk();
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
    // FUN_100e6cc40: the fixed keys, then the preset's Go* bindings.
    const std::array<std::pair<Otc::Direction, TKey>, 12> fixed{ {
        { Otc::West, { Qt::Key_Left, false } },
        { Otc::West, { Qt::Key_Left, true } },
        { Otc::East, { Qt::Key_Right, false } },
        { Otc::East, { Qt::Key_Right, true } },
        { Otc::North, { Qt::Key_Up, false } },
        { Otc::North, { Qt::Key_Up, true } },
        { Otc::South, { Qt::Key_Down, false } },
        { Otc::South, { Qt::Key_Down, true } },
        { Otc::NorthWest, { Qt::Key_Home, true } },
        { Otc::NorthEast, { Qt::Key_PageUp, true } },
        { Otc::SouthWest, { Qt::Key_End, true } },
        { Otc::SouthEast, { Qt::Key_PageDown, true } },
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

bool TPlayerMovementInputInterpreter::canWalk() const
{
    if (!tibia::world::session().isOnline())
        return false;
    // CIP TEventFilterManager: personal-mouse-shield dialogs (NPC, etc.) leave the
    // filter up so walk/hotkeys still run; only modal / focused-UI blocks match
    // hotkeysBlocked() (same gate as TKeyboardShortcutHandler).
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

Otc::Direction TPlayerMovementInputInterpreter::directionOf(const QKeyEvent* event) const
{
    TKey key{ event->key(), event->modifiers().testFlag(Qt::KeypadModifier) };
    if (isArrow(key.key))
        key.keypad = false;
    const TDirectionKeys& directions = m_directions[m_chatMode && m_chatMode() != 0 ? 1 : 0];
    const auto it = std::find_if(directions.begin(), directions.end(), [&key](const std::vector<TKey>& keys) {
        return std::find(keys.begin(), keys.end(), key) != keys.end();
    });
    return it == directions.end() ? Otc::InvalidDirection : Otc::Direction(it - directions.begin());
}

void TPlayerMovementInputInterpreter::onKeyPressed(const QKeyEvent* event)
{
    if (event->isAutoRepeat())
        return;
    const Otc::Direction direction = directionOf(event);
    if (direction == Otc::InvalidDirection || !canWalk())
        return;
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if (direction <= Otc::West && rotates(modifiers)) {
        // CIP TMovementIntentRotate / TPlayerMovementIntent::rotate.
        const TMovementIntentRotate rotateIntent(static_cast<int>(direction));
        const TPlayerMovementIntent intent = TPlayerMovementIntent::rotate(rotateIntent.direction());
        tibia::world::session().turn(static_cast<Otc::Direction>(intent.direction()));
        return;
    }
    if (modifiers != Qt::NoModifier)
        return;
    const TMovementIntentDirection walkIntent(static_cast<int>(direction));
    const TPlayerMovementIntent intent = TPlayerMovementIntent::direction(walkIntent.direction());
    m_held = static_cast<Otc::Direction>(intent.direction());
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
    m_held = Otc::InvalidDirection;
    m_heldKey = 0;
    if (m_pending == Otc::InvalidDirection)
        m_timer->stop();
}

void TPlayerMovementInputInterpreter::stopRepeat()
{
    (void)TMovementIntentStop{};
    (void)TPlayerMovementIntent::stop();
    m_held = m_pending = Otc::InvalidDirection;
    m_heldKey = 0;
    m_timer->stop();
}

void TPlayerMovementInputInterpreter::onTick()
{
    if (!canWalk()) {
        stopRepeat();
        return;
    }
    if (m_held != Otc::InvalidDirection && m_elapsed.elapsed() > (m_repeats > 0 ? RepeatDelayMs : m_keyboardDelayMs)) {
        m_elapsed.restart();
        ++m_repeats;
        step(m_held);
    } else if (m_pending != Otc::InvalidDirection) {
        step(m_pending);
    }
    if (m_held == Otc::InvalidDirection && m_pending == Otc::InvalidDirection)
        m_timer->stop();
}

void TPlayerMovementInputInterpreter::step(const Otc::Direction direction)
{
    if (!tibia::world::player())
        return;
    if (!tibia::world::motion().canStep()) {
        m_pending = direction;
        return;
    }
    m_pending = Otc::InvalidDirection;
    if (tibia::world::motion().blocksStep(direction)) {
        if (m_turnTowardsMoveDirection && facing(direction) != tibia::world::player()->getDirection())
            tibia::world::session().turn(facing(direction));
        else {
            const TPathPlanningFailedException error("blocked step");
            (void)error.message();
            showSorryNotPossible();
        }
        return;
    }
    // Soft: keyboard walk cancels click-to-walk / follow and applies auto-chase.
    if (const LocalPlayerPtr& player = g_game.getLocalPlayer()) {
        if (player->isAutoWalking() || g_game.isFollowing()) {
            player->stopAutoWalk();
            if (g_game.isFollowing())
                g_game.cancelFollow();
        }
    }
    publish(makeAction(A::APPLY_AUTOCHASE_SETTING));
    const TMovementIntentDirection walkIntent(static_cast<int>(direction));
    tibia::world::session().walk(static_cast<Otc::Direction>(walkIntent.direction()));
}

}
