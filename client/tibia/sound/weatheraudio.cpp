#include "sound/weatheraudio.h"

#include "sound/soundcontroller.h"

#include <algorithm>

namespace tibia::sound {

TWeatherAudio& TWeatherAudio::instance()
{
    static TWeatherAudio audio;
    return audio;
}

TWeatherAudio::TWeatherAudio() = default;

float TWeatherAudio::ambienceVolume() const
{
    const TSoundController* controller = TSoundController::instance();
    if (!controller || !controller->configuration()->isAmbienceEnabled())
        return 0.0f;
    return controller->configuration()->ambienceVolume();
}

uint16_t TWeatherAudio::currentAmbienceId() const
{
    const TSoundController* controller = TSoundController::instance();
    return controller ? controller->ambienceStream().ambienceStreamId() : 0;
}

void TWeatherAudio::beginAmbienceHold()
{
    if (m_held)
        return;
    m_held = true;
    if (TSoundController* controller = TSoundController::instance())
        controller->ambienceStream().setAmbienceStream(0);
}

void TWeatherAudio::endAmbienceHold(const uint16_t fallbackRestoreId)
{
    m_held = false;
    const uint16_t restore = m_hasDeferred ? m_deferredStreamId : fallbackRestoreId;
    m_hasDeferred = false;
    m_deferredStreamId = 0;
    m_gain = 1.0f;
    if (m_bed)
        m_bed->close();
    if (TSoundController* controller = TSoundController::instance())
        controller->setAmbienceStream(restore);
}

bool TWeatherAudio::deferAmbienceStream(const uint16_t streamId)
{
    if (!m_held)
        return false;
    m_deferredStreamId = streamId;
    m_hasDeferred = true;
    return true;
}

bool TWeatherAudio::playHeldAmbience(const uint16_t ambienceStreamId, const bool /*loop*/)
{
    TSoundController* controller = TSoundController::instance();
    if (!controller)
        return false;
    if (m_bed)
        m_bed->close();
    controller->setAmbienceStream(ambienceStreamId);
    return controller->ambienceStream().ambienceStreamId() == ambienceStreamId;
}

bool TWeatherAudio::playHeldAmbiencePath(const std::string& virtualPath, const bool loop)
{
    if (virtualPath.empty() || !TSoundController::instance())
        return false;
    if (!m_bed)
        m_bed = std::make_unique<TAudioSource>();
    if (!m_bed->openFile(virtualPath))
        return false;
    TSoundController::instance()->ambienceStream().setAmbienceStream(0);
    m_bed->setLooping(loop);
    m_bed->setVolume(ambienceVolume() * m_gain);
    m_bed->play();
    return true;
}

void TWeatherAudio::setAmbienceVoiceGain(const float gain)
{
    m_gain = std::clamp(gain, 0.0f, 1.0f);
    updateVolume();
}

void TWeatherAudio::updateVolume()
{
    if (m_bed)
        m_bed->setVolume(ambienceVolume() * m_gain);
}

bool TWeatherAudio::playEffect(const uint16_t soundEffectId, const float gain, const float /*pan*/)
{
    TSoundController* controller = TSoundController::instance();
    if (!controller || soundEffectId == 0 || gain <= 0.0f)
        return false;
    TSoundEffectRequest request;
    request.effectId = soundEffectId;
    TSoundEffectParameters parameters = controller->effectParameters(request);
    parameters.volumeModifiers.volume = ambienceVolume() * std::clamp(gain, 0.0f, 1.0f);
    std::optional<TSoundPlayerToken> token = controller->playSingleShot(parameters);
    if (!token)
        return false;
    controller->keepSingleShot(std::move(*token));
    return true;
}

bool TWeatherAudio::playAmbienceOneshot(const std::string& virtualPath, const float gain, const float /*pan*/)
{
    const float volume = ambienceVolume() * std::clamp(gain, 0.0f, 1.0f);
    if (virtualPath.empty() || volume <= 0.0f)
        return false;
    std::unique_ptr<TAudioSource>& source = m_oneshots[m_nextOneshot];
    m_nextOneshot = (m_nextOneshot + 1) % m_oneshots.size();
    if (!source)
        source = std::make_unique<TAudioSource>();
    if (!source->openFile(virtualPath))
        return false;
    source->setLooping(false);
    source->setVolume(volume);
    source->play();
    return true;
}

}
