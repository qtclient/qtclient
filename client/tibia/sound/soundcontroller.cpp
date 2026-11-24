#include "sound/soundcontroller.h"
#include "game/gamesessionbase.h"

#include "creatures/creaturestorage.h"
#include "game/playerdata.h"
#include "input/gameaction.h"

#include "sound/gamesessionsoundprovider.h"
#include "sound/objectsoundcontroller.h"
#include "sound/soundstorage.h"

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
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
}

void TSoundController::update(const int64_t nowMs)
{
    TSoundControllerBase::update(nowMs);
    m_music->update(nowMs);
    m_ambienceStream->update(nowMs);
    m_ambienceObjectSound->update(nowMs);
    m_combatSound->update(nowMs);
}

void TSoundController::onEffectSoundsChanged()
{
    if (!m_deferredEffectSoundsTimer.isActive())
        m_deferredEffectSoundsTimer.start();
}

void TSoundController::deferredPlayChangedEffectSounds()
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    const std::vector<TSoundToPlay> sounds = session->gameSessionSoundProvider()->soundStorage()->takeSoundsToPlay();
    m_combatSound->playEffectSounds(sounds);
}

void TSoundController::connectToPlayerPositionWasUpdated()
{
    m_gameSessionConnections.push_back(connect(m_playerCreature.get(), &creatures::TCreature::positionWasUpdated, this,
                                               &TSoundController::onPlayerPositionChanged, Qt::QueuedConnection));
}

void TSoundController::onPlayerPositionChanged(const tibia::worldmap::TWorldMapCoordinate position)
{
    m_combatSound->setPlayerPosition(position.toPosition());
}

void TSoundController::onPlayerDataChanged()
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    m_music->setPlayerFlag(session ? session->playerData()->isFighting() : false);
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
    m_gameSession = GameSession;
    // qtclient's sessions are TGameSessionBase; CIP asks the IGameSession.
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_gameSessionConnections.push_back(connect(session->gameSessionSoundProvider().get(),
                                               &TGameSessionSoundProvider::soundsToPlayChanged, this,
                                               &TSoundController::onEffectSoundsChanged));
    m_playerCreature = session->playerCreature().lock();
    if (m_playerCreature) {
        // A session that already has its player only follows the position.
        connectToPlayerPositionWasUpdated();
        return;
    }
    m_gameSessionConnections.push_back(connect(session, &game::TGameSessionBase::playerCreatureAddedToCreatureStorage, this,
                                               [this](const std::weak_ptr<creatures::TCreature>& pPlayer) {
                                                   m_playerCreature = pPlayer.lock();
                                                   connectToPlayerPositionWasUpdated();
                                               }));
    m_gameSessionConnections.push_back(connect(session->playerData().get(), &game::TPlayerData::playerDataChanged, this,
                                               &TSoundController::onPlayerDataChanged));
    m_gameSessionConnections.push_back(connect(session->objectSoundController().get(),
                                               &TObjectSoundController::ambienceObjectStreamCountChanged, this,
                                               &TSoundController::onAmbienceObjectStreamCountChanged));
    onPlayerDataChanged();
}
