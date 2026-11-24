#include "soundhelper.h"

#include "config/clientoptions.h"
#include "input/gameaction.h"
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

void followSoundOptions(config::TClientOptions* options)
{
    if (options)
        QObject::connect(options, &config::TClientOptions::soundOptionsChanged, options, &applyOptionVolumes);
}

}
