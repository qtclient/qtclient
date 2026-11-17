#include "soundhelper.h"

#include "app/clientoptions.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QtGlobal>
#include <format>
#include "sound/soundstorage.h"

#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <tibia/game/gamesession.h>

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

/// SOUND_TRIGGER effects waiting for the next event-loop pass. CipSoft
/// queues each effect once and plays the queue from a zero-interval
/// single-shot timer, so a burst of the same effect plays once.
std::vector<int>& queuedSounds()
{
    static std::vector<int> queue;
    return queue;
}

void playQueuedSounds()
{
    std::vector<int> queue;
    queue.swap(queuedSounds());
    auto* storage = sound::TSoundStorage::instance();
    if (!storage)
        return;
    for (const int soundId : queue) {
        if (!storage->playUiEffect(static_cast<uint16_t>(soundId), 1.0f) && storage->isBankLoaded())
            qWarning("%s", std::format("sound: no audio for effect {}", soundId).c_str());
    }
}

QTimer& queuedSoundsTimer()
{
    static QTimer* timer = [] {
        auto* t = new QTimer;
        t->setSingleShot(true);
        QObject::connect(t, &QTimer::timeout, playQueuedSounds);
        return t;
    }();
    return *timer;
}

void playTriggeredSound(const int soundId)
{
    if (soundId <= 0)
        return;
    const QJsonObject options = app::ClientOptions::load().options();
    if (optionInt(options, "soundMasterVolume", 100) <= 0)
        return;
    if (!uiSoundAllowed(soundId, options))
        return;
    const float gain = uiGain(options);
    if (gain <= 0.0f)
        return;
    std::vector<int>& queue = queuedSounds();
    if (std::find(queue.begin(), queue.end(), soundId) == queue.end())
        queue.push_back(soundId);
    QTimer& timer = queuedSoundsTimer();
    if (!timer.isActive())
        timer.start();
}

}

TSoundHelperService::TSoundHelperService(QObject* parent)
    : cip::tibia::qmlcomponents::TSoundHelperService(parent)
{}

/// Publishes a SOUND_TRIGGER game action for the ESound id.
void TSoundHelperService::playSound(const ESound Sound)
{
    input::publish(std::make_shared<input::gameactions::TGameActionSoundTrigger>(input::gameactions::TGameActionSoundTrigger::TSoundEffect{ static_cast<int>(Sound) }));
}

void updateLoginAnthem(const QJsonObject& options)
{
    static bool anthemQueued = false;
    const int master = std::clamp(optionInt(options, "soundMasterVolume", 100), 0, 100);
    const int musicVol = std::clamp(optionInt(options, "soundMusicVolume", 50), 0, 100);
    const bool anthem = optionBool(options, "soundAnthemEnabled", true);
    auto* storage = sound::TSoundStorage::instance();
    const bool online = tibia::game::isOnline();
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
    // ensureEngine is lazy; playFile starts it. Retry on the next option tick
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
    input::setActionHandler(input::EGameAction::SOUND_TRIGGER, [](const input::gameactions::TGameActionSoundTrigger& action) {
        if (const std::optional<int> effect = action.soundEffectId())
            playTriggeredSound(*effect);
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
