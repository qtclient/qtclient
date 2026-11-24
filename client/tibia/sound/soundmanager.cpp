#include "sound/soundmanager.h"

#include "sound/sounddecompressorthreadpool.h"

#include "readonlymemoryblob.h"

#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcSoundTokens, "sound.tokens", QtFatalMsg)

namespace tibia::sound {

namespace {

/// Tickets of TPlayableSound: one per use of a cached sound.
int g_nextTicket = 0;

bool isState(const TPlayableSoundData& data, const ESoundDataState state)
{
    const auto* value = std::get_if<ESoundDataState>(&data);
    return value && *value == state;
}

}

TPlayableSound::TPlayableSound(TSoundCacheEntry* entry, const TSoundMetadata& metadata)
    : m_metadata(metadata)
{
    attach(entry);
}

TPlayableSound::TPlayableSound(const TPlayableSound& other)
    : m_metadata(other.m_metadata)
{
    attach(other.m_entry);
}

TPlayableSound& TPlayableSound::operator=(const TPlayableSound& other)
{
    if (this != &other) {
        release();
        m_metadata = other.m_metadata;
        attach(other.m_entry);
    }
    return *this;
}

TPlayableSound::TPlayableSound(TPlayableSound&& other) noexcept
    : m_ticket(other.m_ticket)
    , m_entry(other.m_entry)
    , m_metadata(std::move(other.m_metadata))
{
    other.m_entry = nullptr;
    other.m_ticket = 0;
}

TPlayableSound& TPlayableSound::operator=(TPlayableSound&& other) noexcept
{
    if (this != &other) {
        release();
        m_ticket = other.m_ticket;
        m_entry = other.m_entry;
        m_metadata = std::move(other.m_metadata);
        other.m_entry = nullptr;
        other.m_ticket = 0;
    }
    return *this;
}

TPlayableSound::~TPlayableSound()
{
    release();
}

void TPlayableSound::attach(TSoundCacheEntry* entry)
{
    m_entry = entry;
    if (!m_entry)
        return;
    m_ticket = g_nextTicket++;
    m_entry->users.push_back(m_ticket);
}

void TPlayableSound::release()
{
    if (m_entry) {
        auto& users = m_entry->users;
        users.erase(std::remove(users.begin(), users.end(), m_ticket), users.end());
        m_entry = nullptr;
    }
    m_ticket = 0;
}

bool TPlayableSound::isReady() const
{
    if (m_metadata.id == 0 || !m_entry)
        return false;
    return !std::holds_alternative<ESoundDataState>(m_entry->playableData);
}

bool TPlayableSound::isLoading() const
{
    return m_entry && isState(m_entry->playableData, ESoundDataState::Loading);
}

TSoundManager::TSoundManager(std::shared_ptr<TSoundMetadataManager> metadata, std::string soundsDirectory,
                             const int32_t cacheSeconds)
    : m_metadata(std::move(metadata))
    , m_soundsDirectory(std::move(soundsDirectory))
    , m_decompressor(std::make_unique<TSoundDecompressorThreadpool>())
    , m_cacheSeconds(cacheSeconds)
{
    connect(m_decompressor.get(), &TSoundDecompressorThreadpool::convertingToPlayableSoundDataFinished, this,
            &TSoundManager::addPlayableSoundData);
    connect(m_decompressor.get(), &TSoundDecompressorThreadpool::convertingSoundDataFinished, this,
            &TSoundManager::onPreprocessingSoundDataFinished);
}

TSoundManager::~TSoundManager() = default;

QString TSoundManager::soundName(const TSoundEffectID soundId) const
{
    if (const auto sound = m_metadata ? m_metadata->sound(soundId) : std::nullopt)
        return soundDisplayName(*sound);
    return QStringLiteral("Unknown");
}

/// Every sound of the metadata gets an entry (an entry already there stays):
/// a sound of load type 0 plays from its file in the sounds directory; any
/// other has no data until its asset arrives (addSoundAssetData).
void TSoundManager::onSoundMetadataChanged()
{
    for (const auto& [id, sound] : m_metadata->sounds()) {
        TSoundCacheEntry entry;
        entry.soundId = id;
        if (sound.loadType == 0)
            entry.soundData = TSoundFile{ m_soundsDirectory + sound.fileName.toStdString() };
        else
            entry.soundData = ESoundDataState::Unavailable;
        entry.playableData = ESoundDataState::Unloaded;
        m_entries.try_emplace(id, std::move(entry));
    }
}

/// The asset of the sound stored as `FileName` was loaded. A sound of load
/// type 2 is decoded to raw samples on the decompressor (its data is loading
/// until onPreprocessingSoundDataFinished); one of load type 1 keeps the
/// bytes. Either way its playable data is made again on the next use.
void TSoundManager::addSoundAssetData(const QString& FileName, const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& pData)
{
    const std::optional<TSoundMetadata> sound = m_metadata->soundByFileName(FileName);
    if (!sound)
        return;
    TSoundCacheEntry& entry = m_entries.at(sound->id);
    if (sound->loadType == 2) {
        entry.soundData = m_decompressor->soundData(sound->id, TSoundData{ pData->getData() }, ESoundLoadTarget::RawSamples);
        ++m_preprocessingCount;
    } else if (sound->loadType == 1) {
        entry.soundData = TSoundData{ pData->getData() };
    }
    entry.playableData = ESoundDataState::Unloaded;
}

void TSoundManager::startLoading(TSoundCacheEntry& entry, const TSoundMetadata& metadata)
{
    entry.playableData =
        m_decompressor->playableSoundData(entry.soundId, entry.soundData, static_cast<ESoundLoadTarget>(metadata.cacheType));
}

std::optional<TPlayableSound> TSoundManager::playableSound(const TSoundEffectID soundId)
{
    const std::optional<TSoundMetadata> metadata = m_metadata ? m_metadata->sound(soundId) : std::nullopt;
    if (!metadata)
        return std::nullopt;
    const auto it = m_entries.find(soundId);
    if (it == m_entries.end())
        return std::nullopt;
    TSoundCacheEntry& entry = it->second;
    if (isState(entry.playableData, ESoundDataState::Unloaded)) {
        startLoading(entry, *metadata);
        if (isState(entry.playableData, ESoundDataState::Loading) && lcSoundTokens().isDebugEnabled())
            qCDebug(lcSoundTokens) << "Sound" << soundName(soundId) << "will be loaded and played asynchronously";
    }
    TPlayableSound playable(&entry, *metadata);
    entry.lastUseSeconds = m_timestampMs / 1000;
    return playable;
}

void TSoundManager::onPreprocessingSoundDataFinished(const TSoundEffectID soundId, const TSoundData& soundData)
{
    const auto it = m_entries.find(soundId);
    if (it == m_entries.end())
        return;
    it->second.soundData = soundData;
    Q_EMIT playableSoundsChanged();
}

void TSoundManager::addPlayableSoundData(const TSoundEffectID soundId, const TPlayableSoundData& playableSoundData)
{
    ++m_loadedCount;
    qCDebug(lcSoundTokens) << "sound id" << soundName(soundId) << "is added to playable sounds";
    const auto it = m_entries.find(soundId);
    if (it == m_entries.end())
        return;
    it->second.playableData = playableSoundData;
    Q_EMIT playableSoundsChanged();
}

void TSoundManager::cleanupCache()
{
    const int64_t seconds = m_timestampMs / 1000;
    if (m_lastCleanupSeconds == seconds)
        return;
    m_lastCleanupSeconds = seconds;
    for (auto& [id, entry] : m_entries) {
        if (!entry.users.empty())
            continue;
        const std::optional<TSoundMetadata> metadata = m_metadata ? m_metadata->sound(id) : std::nullopt;
        if (!metadata)
            continue;
        bool remove = metadata->keepLoaded == 0;
        if (metadata->keepLoaded == 1) {
            if (entry.lastUseSeconds < seconds - m_cacheSeconds) {
                remove = true;
                if (!isState(entry.playableData, ESoundDataState::Unloaded))
                    qCDebug(lcSoundTokens) << "Sound" << soundName(id)
                                           << "is no longer in use and can be removed from the cache";
            }
        }
        if (remove)
            entry.playableData = ESoundDataState::Unloaded;
    }
}

void TSoundManager::updateWithTimestamp(const qint64 timestampMilliseconds)
{
    m_timestampMs = timestampMilliseconds;
    cleanupCache();
}

}
