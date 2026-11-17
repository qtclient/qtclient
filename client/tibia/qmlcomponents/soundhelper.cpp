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

/// Publishes a SOUND_TRIGGER game action for the ESound id.
void TSoundHelperService::playSound(const ESound Sound)
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::SOUND_TRIGGER));
    action->parameter = static_cast<int>(Sound);
    input::publish(action);
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
    input::setActionHandler(input::EGameAction::SOUND_TRIGGER, [](const input::TGameAction& action) {
        sound::TSoundController* controller = sound::TSoundController::instance();
        if (!controller || action.parameter <= 0)
            return true;
        sound::TSoundEffectRequest request;
        request.effectId = static_cast<sound::TSoundEffectID>(action.parameter);
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
