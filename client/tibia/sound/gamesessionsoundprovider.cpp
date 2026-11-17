#include "sound/gamesessionsoundprovider.h"

#include "sound/soundstorage.h"

namespace tibia::sound {

TGameSessionSoundProvider::TGameSessionSoundProvider(std::shared_ptr<TSoundStorage> soundStorage)
    : m_soundStorage(std::move(soundStorage))
{
    if (m_soundStorage)
        m_connection = connect(m_soundStorage.get(), &TSoundStorage::soundsToPlayChanged, this,
                               &TGameSessionSoundProvider::onSoundsToPlayChanged);
}

TGameSessionSoundProvider::~TGameSessionSoundProvider()
{
    disconnect(m_connection);
}

void TGameSessionSoundProvider::onSoundsToPlayChanged()
{
    Q_EMIT soundsToPlayChanged();
}

}
