#include "client/gameclientbase.h"

#include "config/clientoptions.h"
#include "game/gameactionrouter.h"
#include "gamewindow/storeandresourcebalancehelpercontroller.h"
#include "sound/soundconfiguration.h"
#include "sound/soundcontroller.h"
#include "sound/soundengine.h"
#include "sound/soundmanager.h"
#include "sound/soundmetadatamanager.h"
#include "sound/soundplayermanager.h"

#include <QObject>

#include <stdexcept>

namespace tibia::client {

TGameClientBase::TGameClientBase() = default;

TGameClientBase::~TGameClientBase() = default;

std::shared_ptr<game::TGameActionRouter> TGameClientBase::gameActionRouter() const
{
    if (!m_pGameActionRouter)
        throw std::logic_error("m_pGameActionRouter was not initialized yet");
    return m_pGameActionRouter;
}

gamewindow::TStoreAndResourceBalanceHelperController* TGameClientBase::storeAndResourceBalanceHelperController() const
{
    if (!m_pStoreAndResourceBalanceHelperController)
        throw std::logic_error("m_pStoreAndResourceBalanceHelperController was not initialized yet");
    return m_pStoreAndResourceBalanceHelperController.get();
}

std::shared_ptr<sound::TSoundMetadataManager> TGameClientBase::soundMetadataManager() const
{
    if (!m_pSoundMetadataManager)
        throw std::logic_error("m_pSoundMetadataManager was not initialized yet");
    return m_pSoundMetadataManager;
}

void TGameClientBase::initializeSound()
{
    const sound::TSoundSettings settings;
    m_pSoundConfiguration = std::make_shared<sound::TSoundConfiguration>(settings, config::TClientOptions::instance());
    if (!settings.noSound)
        sound::TSoundEngine::instance().initialize(m_pSoundConfiguration);
    if (!sound::TSoundEngine::instance().isInitialized())
        return;
    m_pSoundMetadataManager = std::make_shared<sound::TSoundMetadataManager>(nullptr, m_pSoundConfiguration->settings().ambienceDelayFactor);
    // qtclient's sound files live in the data search path's `sounds/`.
    m_pSoundManager = std::make_shared<sound::TSoundManager>(m_pSoundMetadataManager, "sounds/", settings.soundCacheSeconds);
    m_pSoundPlayerManager = std::make_shared<sound::TSoundPlayerManager>(settings.soundPlayerCount);
    m_pSoundController = std::make_shared<sound::TSoundController>(m_pSoundMetadataManager, m_pSoundManager,
                                                                   m_pSoundPlayerManager, m_pSoundConfiguration);
    QObject::connect(m_pSoundManager.get(), &sound::TSoundManager::playableSoundsChanged, m_pSoundPlayerManager.get(),
                     &sound::TSoundPlayerManager::onSoundManagerPlayableSoundsChanged);
}

}
