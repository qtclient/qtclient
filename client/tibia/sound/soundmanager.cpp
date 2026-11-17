#include "sound/soundmanager.h"

#include "sound/sounddecompressorthreadpool.h"

#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcSoundTokens, "sound.tokens", QtWarningMsg)

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
        return sound->originalFileName.isEmpty() ? sound->fileName : sound->originalFileName;
    return QStringLiteral("Unknown");
}

void TSoundManager::onSoundMetadataChanged()
{
    if (!m_metadata)
        return;
    for (const auto& [id, sound] : m_metadata->sounds()) {
        TSoundCacheEntry entry;
        entry.soundId = id;
        // Every sound streams from its file until it is converted; CipSoft
        // only starts sounds with load type 0 that way and preloads the others.
        entry.soundData = TSoundFile{ m_soundsDirectory + sound.fileName.toStdString() };
        entry.playableData = ESoundDataState::Unloaded;
        m_entries.insert_or_assign(id, std::move(entry));
    }
    m_soundCount = static_cast<int64_t>(m_entries.size());
}

void TSoundManager::startLoading(TSoundCacheEntry& entry, const TSoundMetadata& metadata)
{
    const auto target = static_cast<ESoundLoadTarget>(metadata.cacheType);
    if (static_cast<int>(entry.soundData.index()) == static_cast<int>(target)) {
        if (const auto* file = std::get_if<TSoundFile>(&entry.soundData))
            entry.playableData = *file;
        else if (const auto* bytes = std::get_if<QByteArray>(&entry.soundData))
            entry.playableData = *bytes;
        return;
    }
    if (target == ESoundLoadTarget::Buffer) {
        if (const auto* samples = std::get_if<TRawSoundSamples>(&entry.soundData)) {
            entry.playableData = TSoundBuffer{ std::make_shared<const TRawSoundSamples>(*samples) };
            return;
        }
    }
    // CipSoft's asset loader has a streamed sound's bytes in memory already;
    // qtclient reads the file here (no decoding).
    if (target == ESoundLoadTarget::StreamFile || target == ESoundLoadTarget::StreamMemory) {
        entry.playableData = TSoundDecompressorThreadpool::toPlayableSoundData(entry.soundData, target);
        return;
    }
    m_decompressor->convertToPlayableSoundData(entry.soundId, entry.soundData, target);
    entry.playableData = ESoundDataState::Loading;
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
