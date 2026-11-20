#include "sound/soundcontroller.h"
#include "game/gamesessionbase.h"

#include "input/gameaction.h"

#include "sound/gamesessionsoundprovider.h"
#include "sound/objectsoundcontroller.h"
#include "sound/soundstorage.h"

#include <tibia/game/gamesession.h>

namespace tibia::sound {

namespace {
TSoundController* g_soundController = nullptr;
}

TSoundController* TSoundController::instance()
{
    return g_soundController;
}

void TSoundController::setInstance(TSoundController* controller)
{
    g_soundController = controller;
}

TSoundController::TSoundController(std::shared_ptr<TSoundMetadataManager> metadata,
                                   std::shared_ptr<TSoundManager> soundManager,
                                   std::weak_ptr<TSoundPlayerManager> playerManager,
                                   std::shared_ptr<TSoundConfiguration> configuration)
    : TSoundControllerBase(metadata, soundManager, playerManager, configuration)
    , m_music(std::make_shared<TSoundControllerMusic>(metadata, soundManager, playerManager, configuration))
    , m_ambienceStream(std::make_shared<TSoundControllerAmbienceStream>(metadata, soundManager, playerManager, configuration))
    , m_ambienceObjectSound(
          std::make_shared<TSoundControllerAmbienceObjectSound>(metadata, soundManager, playerManager, configuration))
    , m_combatSound(std::make_shared<TSoundControllerCombatSound>(metadata, soundManager, playerManager, configuration))
{
    m_deferredEffectSoundsTimer.setSingleShot(true);
    connect(&m_deferredEffectSoundsTimer, &QTimer::timeout, this, &TSoundController::deferredPlayChangedEffectSounds);
    m_queuedSoundsTimer.setSingleShot(true);
    connect(&m_queuedSoundsTimer, &QTimer::timeout, this, &TSoundController::onQueuedSoundsTimerTriggered);
}

TSoundController::~TSoundController()
{
    if (g_soundController == this)
        g_soundController = nullptr;
    disconnect(m_providerConnection);
}

void TSoundController::update(const int64_t nowMs)
{
    TSoundControllerBase::update(nowMs);
    m_music->update(nowMs);
    m_ambienceStream->update(nowMs);
    m_ambienceObjectSound->update(nowMs);
    m_combatSound->update(nowMs);
}

void TSoundController::setGameSessionSoundProvider(std::shared_ptr<TGameSessionSoundProvider> provider)
{
    disconnect(m_providerConnection);
    m_provider = std::move(provider);
    if (m_provider)
        m_providerConnection = connect(m_provider.get(), &TGameSessionSoundProvider::soundsToPlayChanged, this,
                                       &TSoundController::onEffectSoundsChanged);
}

void TSoundController::onEffectSoundsChanged()
{
    if (!m_deferredEffectSoundsTimer.isActive())
        m_deferredEffectSoundsTimer.start();
}

void TSoundController::deferredPlayChangedEffectSounds()
{
    if (!m_provider || !m_provider->soundStorage())
        return;
    // CipSoft follows the player's position signal; qtclient reads it here.
    if (tibia::game::hasLocalPlayer())
        onPlayerPositionChanged(tibia::game::localPlayerPosition());
    const std::vector<TSoundToPlay> sounds = m_provider->soundStorage()->soundsToPlay();
    m_combatSound->playEffectSounds(sounds);
}

void TSoundController::onPlayerPositionChanged(const Position& position)
{
    m_combatSound->setPlayerPosition(position);
}

void TSoundController::onAmbienceObjectStreamCountChanged(const uint16_t soundEffectId, const size_t count,
                                                         const int nearestDistance)
{
    m_ambienceObjectSound->ambienceObjectStreamCountChanged(soundEffectId, count, nearestDistance);
}

void TSoundController::setMusicTemplate(const uint16_t templateId)
{
    m_music->setCurrentMusicTemplate(templateId);
}

void TSoundController::setAmbienceStream(const uint16_t streamId)
{
    m_ambienceStream->setAmbienceStream(streamId);
}

void TSoundController::handleGameAction(const tibia::input::IGameActionPtr& pGameAction)
{
    const auto* action = tibia::input::gameActionAs<tibia::input::gameactions::TGameActionSoundTrigger>(pGameAction);
    if (!action)
        return;
    // TGameActionSoundTrigger's variant: pause/resume the looping sounds (the
    // game screen publishes the resume) or play an effect once.
    if (const std::optional<bool> pause = action->pauseLoopingSounds()) {
        handleSoundTrigger(*pause);
        return;
    }
    const int effect = action->soundEffectId().value_or(0);
    if (effect <= 0)
        return;
    TSoundEffectRequest request;
    request.effectId = static_cast<TSoundEffectID>(effect);
    request.chat = TChatSoundContext{ true };
    handleSoundTrigger(request);
}

void TSoundController::handleSoundTrigger(const TSoundTrigger& trigger)
{
    if (const bool* suspend = std::get_if<bool>(&trigger)) {
        m_music->setSuspended(*suspend);
        m_ambienceStream->setSuspended(*suspend);
        m_ambienceObjectSound->setSuspended(*suspend);
        if (*suspend) {
            m_ambienceStream->stop(true);
            m_ambienceObjectSound->stopAllAmbienceObjectSounds();
            m_combatSound->stopAll();
        }
        return;
    }
    const TSoundEffectParameters parameters = effectParameters(std::get<TSoundEffectRequest>(trigger));
    const float volume = parameters.volumeModifiers.effectiveVolume();
    if (volume <= 0.0f)
        return;
    const auto [it, inserted] = m_queuedSounds.emplace(parameters.soundId, parameters);
    if (!inserted && it->second.volumeModifiers.effectiveVolume() < volume)
        it->second = parameters;
    if (!m_queuedSoundsTimer.isActive())
        m_queuedSoundsTimer.start();
}

void TSoundController::onQueuedSoundsTimerTriggered()
{
    for (const auto& [soundId, parameters] : m_queuedSounds) {
        if (std::optional<TSoundPlayerToken> token = playSingleShot(parameters))
            keepSingleShot(std::move(*token));
    }
    m_queuedSounds.clear();
}

}

void tibia::sound::TSoundController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    disconnect(m_objectSoundConnection);
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    setGameSessionSoundProvider(session ? session->gameSessionSoundProvider() : nullptr);
    if (session && session->objectSoundController())
        m_objectSoundConnection = connect(session->objectSoundController().get(), &TObjectSoundController::ambienceObjectStreamCountChanged,
                                          this, &TSoundController::onAmbienceObjectStreamCountChanged);
}
