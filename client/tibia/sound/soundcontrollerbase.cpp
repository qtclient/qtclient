#include "sound/soundcontrollerbase.h"

#include "randomnumbergenerator.h"

#include <QJSEngine>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <variant>

Q_LOGGING_CATEGORY(lcSound, "sound", QtFatalMsg)

namespace tibia::sound {

double randomBetween(const double min, const double max)
{
    if (max <= min)
        return min;
    return min + (max - min) * QRandomGenerator::global()->generateDouble();
}

int64_t randomBetween(const int64_t min, const int64_t max)
{
    if (max <= min)
        return min;
    // Cast both ends: on Linux int64_t is long and max+1 is long, which is
    // ambiguous with QRandomGenerator::bounded(quint32, quint32) vs qint64.
    return QRandomGenerator::global()->bounded(static_cast<qint64>(min),
                                               static_cast<qint64>(max) + 1);
}

TSoundControllerBase::TSoundControllerBase(std::shared_ptr<TSoundMetadataManager> metadata,
                                           std::shared_ptr<TSoundManager> soundManager,
                                           std::weak_ptr<TSoundPlayerManager> playerManager,
                                           std::shared_ptr<TSoundConfiguration> configuration)
    : m_metadata(std::move(metadata))
    , m_soundManager(std::move(soundManager))
    , m_playerManager(std::move(playerManager))
    , m_configuration(std::move(configuration))
{
    if (m_configuration)
        connect(m_configuration.get(), &TSoundConfiguration::soundConfigurationChanged, this,
                &TSoundControllerBase::onSoundConfigurationChanged);
    onSoundConfigurationChanged();
}

TSoundControllerBase::~TSoundControllerBase()
{
    m_singleShots.clear();
    for (TActiveSoundPlayer* player : m_activePlayers)
        delete player;
}

void TSoundControllerBase::update(const int64_t nowMs)
{
    m_nowMs = nowMs;
    for (TActiveSoundPlayer* player : m_activePlayers)
        player->update(nowMs);
    for (auto it = m_activePlayers.begin(); it != m_activePlayers.end();) {
        if ((*it)->isPlaying()) {
            ++it;
            continue;
        }
        delete *it;
        it = m_activePlayers.erase(it);
    }
    releaseFinishedPlayers(m_singleShots);
}

namespace {

/// One of the sounds, picked at random (the only one without asking the random
/// number generator); 0 without sounds.
uint16_t randomSoundId(const std::vector<uint16_t>& soundIds)
{
    if (soundIds.size() < 2)
        return soundIds.empty() ? 0 : soundIds.front();
    const quint64 index = shared::TRandomNumberGenerator::instance()->getRandomUnsignedLong(0, soundIds.size() - 1);
    return index < soundIds.size() ? soundIds[index] : 0;
}

}

QString TSoundControllerBase::numericSoundEffectName(const uint16_t effectId) const
{
    QString sounds = QStringLiteral("Unknown");
    if (const std::optional<TNumericSoundEffectMetadata> effect = m_metadata->numericSoundEffect(effectId)) {
        if (std::holds_alternative<std::monostate>(effect->sound)) {
            sounds = QStringLiteral("unknown");
        } else if (const auto* single = std::get_if<TSingleSoundEffectMetadata>(&effect->sound)) {
            sounds = QStringLiteral("Single sound effect(%1)").arg(m_soundManager->soundName(single->soundId));
        } else if (const auto* random = std::get_if<TRandomSoundMetadata>(&effect->sound)) {
            QStringList names;
            for (const uint16_t soundId : random->soundIds)
                names.append(m_soundManager->soundName(soundId));
            sounds = QStringLiteral("Random Sound Effect(%1)").arg(names.join(QLatin1Char(',')));
        }
    }
    return QStringLiteral("Numeric sound effect (%1, %2)").arg(effectId).arg(sounds);
}

TSoundEffectParameters TSoundControllerBase::effectParameters(const TSoundEffectRequest& request) const
{
    TSoundEffectParameters parameters;
    const std::optional<TNumericSoundEffectMetadata> effect =
        m_metadata ? m_metadata->numericSoundEffect(request.effectId) : std::nullopt;
    if (!effect)
        return parameters;
    if (const auto* single = std::get_if<TSingleSoundEffectMetadata>(&effect->sound)) {
        parameters.soundId = single->soundId;
    } else if (const auto* random = std::get_if<TRandomSoundMetadata>(&effect->sound)) {
        parameters.soundId = randomSoundId(random->soundIds);
    }
    parameters.pitch = static_cast<float>(randomBetween(static_cast<double>(effect->pitchMin), static_cast<double>(effect->pitchMax)));
    const auto randomVolume = static_cast<float>(randomBetween(static_cast<double>(effect->volumeMin), static_cast<double>(effect->volumeMax)));
    parameters.volumeModifiers.random = static_cast<uint8_t>(static_cast<int>(randomVolume * 100.0f));
    const TSoundConfiguration::TEffectSettings settings =
        m_configuration ? m_configuration->effectSettings(effect->numericSoundType, request.chat, request.source)
                        : TSoundConfiguration::TEffectSettings{};
    parameters.volumeModifiers.volume = settings.enabled ? std::max(settings.volume, 0.0f) : 0.0f;
    return parameters;
}

std::optional<TSoundPlayerToken> TSoundControllerBase::playSingleShot(const TSoundEffectParameters& parameters)
{
    if (parameters.soundId == 0)
        return std::nullopt;
    return playSingleShot(parameters.pitch, parameters.soundId, parameters.volumeModifiers);
}

std::optional<TSoundPlayerToken> TSoundControllerBase::playSingleShot(const float pitch, const TSoundEffectID soundId,
                                                                      const TVolumeModifiers& modifiers)
{
    if (modifiers.volume == 0.0f)
        return std::nullopt;
    std::optional<TPlayableSound> playable = m_soundManager ? m_soundManager->playableSound(soundId) : std::nullopt;
    const std::shared_ptr<TSoundPlayerManager> manager = m_playerManager.lock();
    if (!manager || !playable || !playable->isReady()) {
        qDebug() << "Could not find Sound Effect ID" << soundId;
        return std::nullopt;
    }
    std::optional<TSoundPlayerToken> token = manager->acquirePlayer();
    if (!token) {
        qDebug() << "Could not get free player for single shot sound effect" << soundId;
        return std::nullopt;
    }
    QString message;
    QTextStream stream(&message);
    stream << "Play single shot effect" << " " << m_soundManager->soundName(playable->soundId())
           << " (free sound effects " << manager->freePlayerCount() << ")";
    if (pitch != 1.0f)
        stream << QStringLiteral(" with pitch of %1").arg(static_cast<double>(pitch));
    if (modifiers.distance != 100)
        stream << QStringLiteral(" and distance modifier %1").arg(int(modifiers.distance));
    stream.flush();
    qDebug().noquote() << message;
    TSoundPlayer* player = token->player();
    player->setSound(*playable);
    player->setVolumeModifiers(modifiers);
    player->setLooping(false);
    player->setPitch(pitch);
    player->play();
    return token;
}

void TSoundControllerBase::keepSingleShot(TSoundPlayerToken token)
{
    m_singleShots.push_back(std::move(token));
}

size_t TSoundControllerBase::freePlayerCount() const
{
    const std::shared_ptr<TSoundPlayerManager> manager = m_playerManager.lock();
    return manager ? manager->freePlayerCount() : 0;
}

TActiveSoundPlayer* TSoundControllerBase::createActivePlayer()
{
    auto* player = new TActiveSoundPlayer(m_playerManager);
    QJSEngine::setObjectOwnership(player, QJSEngine::CppOwnership);
    return player;
}

void TSoundControllerBase::addActivePlayer(TActiveSoundPlayer* player)
{
    m_activePlayers.push_back(player);
}

}
