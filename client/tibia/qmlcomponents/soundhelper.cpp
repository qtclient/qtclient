#include "soundhelper.h"

#include "app/clientoptions.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <framework/core/logger.h>
#include "sound/soundstorage.h"
#include <tibia/world/game.h>

#include <QJsonObject>
#include <QObject>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>

namespace tibia::qmlcomponents {

namespace {

using ESound = cip::tibia::qmlcomponents::TSoundHelperService::ESound;

bool optionBool(const QJsonObject& options, const char* key, const bool fallback = true)
{
    const QJsonValue value = options.value(QLatin1String(key));
    return value.isUndefined() ? fallback : value.toBool(fallback);
}

int optionInt(const QJsonObject& options, const char* key, const int fallback)
{
    const QJsonValue value = options.value(QLatin1String(key));
    return value.isUndefined() ? fallback : value.toInt(fallback);
}

/// Whether TUISoundsQMLOptionsPage allows this ESound (CIP option ids 0xaa–0xbe).
bool uiSoundAllowed(const int soundId, const QJsonObject& options)
{
    switch (static_cast<ESound>(soundId)) {
    case ESound::BUTTON_PRESS:
    case ESound::BUTTON_RELEASE:
    case ESound::OPEN_DIALOG_OR_WIDGET:
    case ESound::SCREENSHOT:
    case ESound::STORE_ANIMATION_BUY:
    case ESound::STORE_ANIMATION_RATTLING:
    case ESound::QUEST_TRACKER_ADD:
    case ESound::QUEST_TRACKER_REMOVE:
    case ESound::SEND_CHAT_MESSAGE:
        return optionBool(options, "soundUiInteractionsEnabled");
    case ESound::CHAT_MESSAGE_ARRIVED:
        return optionBool(options, "ChatEnabled");
    case ESound::PRIVATE_MESSAGE_IN_LOCAL_CHAT:
        return optionBool(options, "PrivateMessagesWithoutTabEnabled")
            || optionBool(options, "PrivateMessagesEnabled");
    case ESound::VIP_LIST_LOGIN:
    case ESound::VIP_LIST_LOGOUT:
        return optionBool(options, "soundVipEnabled");
    default:
        return optionBool(options, "soundUiInteractionsEnabled");
    }
}

float uiGain(const QJsonObject& options)
{
    const int master = std::clamp(optionInt(options, "soundMasterVolume", 100), 0, 100);
    const int ui = std::clamp(optionInt(options, "soundUiVolume", 100), 0, 100);
    return (master / 100.0f) * (ui / 100.0f);
}

void playTriggeredSound(const int soundId)
{
    if (soundId <= 0)
        return;
    // A stall while entering the world drains a burst of the same button
    // press. One click is one press and one release.
    if (soundId == static_cast<int>(ESound::BUTTON_PRESS) || soundId == static_cast<int>(ESound::BUTTON_RELEASE)) {
        using clock = std::chrono::steady_clock;
        static clock::time_point lastPress{};
        static clock::time_point lastRelease{};
        auto& last = soundId == static_cast<int>(ESound::BUTTON_PRESS) ? lastPress : lastRelease;
        const auto now = clock::now();
        if (last != clock::time_point{} && now - last < std::chrono::milliseconds(200))
            return;
        last = now;
    }
    const QJsonObject options = app::ClientOptions::load().options();
    if (optionInt(options, "soundMasterVolume", 100) <= 0)
        return;
    if (!uiSoundAllowed(soundId, options))
        return;
    const float gain = uiGain(options);
    if (gain <= 0.0f)
        return;
    if (auto* storage = sound::TSoundStorage::instance()) {
        if (!storage->playUiEffect(static_cast<uint16_t>(soundId), 1.0f) && storage->isBankLoaded())
            g_logger.warning("sound: no audio for effect {}", soundId);
    }
}

}

TSoundHelperService::TSoundHelperService(QObject* parent)
    : cip::tibia::qmlcomponents::TSoundHelperService(parent)
{}

/// CIP 0x1010c4870: builds SOUND_TRIGGER (0x12f) with the ESound id and publishes.
void TSoundHelperService::playSound(const ESound Sound)
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::SOUND_TRIGGER));
    action->parameter = static_cast<int>(Sound);
    input::publish(action);
}

void updateLoginAnthem(const QJsonObject& options)
{
    static bool anthemQueued = false;
    const int master = std::clamp(optionInt(options, "soundMasterVolume", 100), 0, 100);
    const int musicVol = std::clamp(optionInt(options, "soundMusicVolume", 50), 0, 100);
    const bool anthem = optionBool(options, "soundAnthemEnabled", true);
    auto* storage = sound::TSoundStorage::instance();
    const bool online = g_game.isOnline();
    // Login anthem only while offline. Entering the world must not stop music
    // the server already started on this channel.
    if (online || master <= 0 || musicVol <= 0 || !anthem) {
        if (storage)
            storage->stopLoginAnthem();
        anthemQueued = false;
        return;
    }
    if (anthemQueued)
        return;
    // Product anthem lives under data/sounds/ (modules/ retired with Lua /play).
    // ensureEngine() is lazy; playFile starts it. Retry on the next option tick
    // if the file is missing or decode fails.
    if (storage && storage->playFile("/data/sounds/startup", sound::TSoundStorage::Channel::Music, true))
        anthemQueued = true;
}

void applyOptionVolumes()
{
    const QJsonObject options = app::ClientOptions::load().options();
    if (auto* storage = sound::TSoundStorage::instance())
        storage->applyClientOptions(options);
    updateLoginAnthem(options);
}

void installSoundActionHandler()
{
    input::setActionHandler(input::EGameAction::SOUND_TRIGGER, [](const input::TGameAction& action) {
        playTriggeredSound(action.parameter);
        return true;
    });
    applyOptionVolumes();
    QObject::connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged,
                     app::ClientOptionsNotifier::instance(), [](const QString& name) {
                         if (name.startsWith(QLatin1String("sound"))
                             || name == QLatin1String("usedSoundDevice"))
                             applyOptionVolumes();
                     });
}

}
