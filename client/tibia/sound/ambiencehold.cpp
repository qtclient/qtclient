#include "sound/ambiencehold.h"

#include "sound/soundcontroller.h"

#include <algorithm>

namespace tibia::sound {

TAmbienceHold& TAmbienceHold::instance()
{
    static TAmbienceHold hold;
    return hold;
}

TAmbienceHold::TAmbienceHold() = default;

void TAmbienceHold::setSoundController(TSoundController* controller)
{
    m_controller = controller;
}

float TAmbienceHold::ambienceVolume() const
{
    const TSoundController* controller = m_controller;
    if (!controller || !controller->configuration()->isAmbienceEnabled())
        return 0.0f;
    return controller->configuration()->ambienceVolume();
}

uint16_t TAmbienceHold::currentAmbienceId() const
{
    const TSoundController* controller = m_controller;
    return controller ? controller->ambienceStream().ambienceStreamId() : 0;
}

void TAmbienceHold::begin()
{
    if (m_held)
        return;
    m_held = true;
    if (TSoundController* controller = m_controller)
        controller->ambienceStream().setAmbienceStream(0);
}

void TAmbienceHold::end(const uint16_t fallbackRestoreId)
{
    m_held = false;
    const uint16_t restore = m_hasDeferred ? m_deferredStreamId : fallbackRestoreId;
    m_hasDeferred = false;
    m_deferredStreamId = 0;
    m_gain = 1.0f;
    if (m_bed)
        m_bed->close();
    if (TSoundController* controller = m_controller)
        controller->setAmbienceStream(restore);
}

bool TAmbienceHold::deferAmbienceStream(const uint16_t streamId)
{
    if (!m_held)
        return false;
    m_deferredStreamId = streamId;
    m_hasDeferred = true;
    return true;
}

bool TAmbienceHold::playStream(const uint16_t ambienceStreamId)
{
    TSoundController* controller = m_controller;
    if (!controller)
        return false;
    if (m_bed)
        m_bed->close();
    controller->setAmbienceStream(ambienceStreamId);
    return controller->ambienceStream().ambienceStreamId() == ambienceStreamId;
}

bool TAmbienceHold::playFile(const std::string& virtualPath, const bool loop)
{
    if (virtualPath.empty() || !m_controller)
        return false;
    if (!m_bed)
        m_bed = std::make_unique<TAudioSource>();
    if (!m_bed->openFile(virtualPath))
        return false;
    m_controller->ambienceStream().setAmbienceStream(0);
    m_bed->setLooping(loop);
    m_bed->setVolume(ambienceVolume() * m_gain);
    m_bed->play();
    return true;
}

void TAmbienceHold::setGain(const float gain)
{
    m_gain = std::clamp(gain, 0.0f, 1.0f);
    updateVolume();
}

void TAmbienceHold::updateVolume()
{
    if (m_bed)
        m_bed->setVolume(ambienceVolume() * m_gain);
}

bool TAmbienceHold::playEffect(const uint16_t soundEffectId, const float gain)
{
    TSoundController* controller = m_controller;
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

bool TAmbienceHold::playOneshot(const std::string& virtualPath, const float gain)
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

namespace {

uint16_t abiCurrentAmbienceId()
{
    return TAmbienceHold::instance().currentAmbienceId();
}

void abiBeginHold()
{
    TAmbienceHold::instance().begin();
}

void abiEndHold(const uint16_t fallbackRestoreId)
{
    TAmbienceHold::instance().end(fallbackRestoreId);
}

int abiPlayHeldStream(const uint16_t ambienceStreamId)
{
    return TAmbienceHold::instance().playStream(ambienceStreamId) ? 1 : 0;
}

int abiPlayHeldFile(const char* virtualPath, const int loop)
{
    return virtualPath && TAmbienceHold::instance().playFile(virtualPath, loop != 0) ? 1 : 0;
}

void abiSetHoldGain(const float gain)
{
    TAmbienceHold::instance().setGain(gain);
}

int abiPlayEffect(const uint16_t soundEffectId, const float gain)
{
    return TAmbienceHold::instance().playEffect(soundEffectId, gain) ? 1 : 0;
}

int abiPlayOneshot(const char* virtualPath, const float gain)
{
    return virtualPath && TAmbienceHold::instance().playOneshot(virtualPath, gain) ? 1 : 0;
}

const QtClientSoundApiV1 g_soundApi = {
    .current_ambience_id = &abiCurrentAmbienceId,
    .begin_ambience_hold = &abiBeginHold,
    .end_ambience_hold = &abiEndHold,
    .play_held_ambience_stream = &abiPlayHeldStream,
    .play_held_ambience_file = &abiPlayHeldFile,
    .set_ambience_hold_gain = &abiSetHoldGain,
    .play_ambience_effect = &abiPlayEffect,
    .play_ambience_oneshot = &abiPlayOneshot,
};

} // namespace

const QtClientSoundApiV1* soundApiV1()
{
    return &g_soundApi;
}

}
