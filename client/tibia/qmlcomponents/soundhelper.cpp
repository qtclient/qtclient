#include "soundhelper.h"

#include "app/clientoptions.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "sound/soundcontroller.h"
#include "sound/weatheraudio.h"

#include <QObject>

namespace tibia::qmlcomponents {

TSoundHelperService::TSoundHelperService(QObject* parent)
    : cip::tibia::qmlcomponents::TSoundHelperService(parent)
{}

namespace {
TSoundHelperService* g_instance = nullptr;
}

TSoundHelperService* TSoundHelperService::instance() { return g_instance; }

void TSoundHelperService::setInstance(TSoundHelperService* service) { g_instance = service; }

/// Publishes a SOUND_TRIGGER game action for the ESound id.
void TSoundHelperService::playSound(const ESound Sound)
{
    if (g_instance && g_instance != this) {
        g_instance->playSound(Sound);
        return;
    }
    Q_EMIT publishGameAction(std::make_shared<input::gameactions::TGameActionSoundTrigger>(
        input::gameactions::TGameActionSoundTrigger::TSoundEffect{ static_cast<int>(Sound) }));
}

void applyOptionVolumes()
{
    // The sound controllers follow TClientOptions themselves; the pack's
    // weather bed follows here.
    sound::TWeatherAudio::instance().updateVolume();
}

void installSoundActionHandler()
{
    // CIP: TSoundController::handleGameAction. The senders (chat storage, …)
    // already applied their channel options, so a chat sound counts as gated.
    input::setActionHandler(input::EGameAction::SOUND_TRIGGER, [](const input::gameactions::TGameActionSoundTrigger& action) {
        sound::TSoundController* controller = sound::TSoundController::instance();
        if (!controller)
            return true;
        // CIP TGameActionSoundTrigger's variant: pause/resume the looping sounds
        // (the game screen publishes the resume) or play an effect once.
        if (const std::optional<bool> pause = action.pauseLoopingSounds()) {
            controller->handleSoundTrigger(*pause);
            return true;
        }
        const int effect = action.soundEffectId().value_or(0);
        if (effect <= 0)
            return true;
        sound::TSoundEffectRequest request;
        request.effectId = static_cast<sound::TSoundEffectID>(effect);
        request.chat = sound::TChatSoundContext{ true };
        controller->handleSoundTrigger(request);
        return true;
    });
    QObject::connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged,
                     app::ClientOptionsNotifier::instance(), [](const QString& name) {
                         if (name.startsWith(QLatin1String("sound")))
                             applyOptionVolumes();
                     });
}

}
