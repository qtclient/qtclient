#pragma once

#include "sound/audiobackend.h"
#include "sound/soundmetadatamanager.h"

#include <cip_gen.h>

#include <QByteArray>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tibia {
namespace sound {

class TSoundDecompressorThreadpool;

using TSoundEffectID = uint16_t;

/// Where a sound's data is (CIP: ESoundDataState for no data).
enum class ESoundDataState : int { Unloaded = 0, Loading = 1, Unavailable = 2 };

/// A sound file in the data file system (CIP: QFileInfo).
struct TSoundFile
{
    std::string virtualPath;
};

/// A sound's source data (CIP: TSoundData).
using TSoundData = std::variant<TSoundFile, QByteArray, TRawSoundSamples, ESoundDataState>;
/// A sound's data in the form it plays from (CIP: TPlayableSoundData): a file
/// or bytes to stream, or a decoded buffer.
using TPlayableSoundData = std::variant<TSoundFile, QByteArray, TSoundBuffer, ESoundDataState>;

/// What a sound is converted to before it plays.
enum class ESoundLoadTarget : int { StreamFile = 0, StreamMemory = 1, RawSamples = 2, Buffer = 3 };

/// A sound in TSoundManager's cache.
struct TSoundCacheEntry
{
    TSoundEffectID soundId = 0;
    TSoundData soundData = ESoundDataState::Unloaded;
    TPlayableSoundData playableData = ESoundDataState::Unloaded;
    /// Tickets of the TPlayableSound objects that use the data.
    std::vector<int> users;
    int64_t lastUseSeconds = 0;
};

/// A use of a cached sound (CIP: TPlayableSound): keeps the data in the cache
/// while it exists.
class TPlayableSound
{
public:
    TPlayableSound() = default;
    TPlayableSound(TSoundCacheEntry* entry, const TSoundMetadata& metadata);
    TPlayableSound(const TPlayableSound& other);
    TPlayableSound& operator=(const TPlayableSound& other);
    TPlayableSound(TPlayableSound&& other) noexcept;
    TPlayableSound& operator=(TPlayableSound&& other) noexcept;
    ~TPlayableSound();

    /// The sound exists and its data can play now.
    bool isReady() const;
    /// The data is still being converted.
    bool isLoading() const;
    const TSoundMetadata& metadata() const { return m_metadata; }
    TSoundEffectID soundId() const { return m_metadata.id; }
    const TPlayableSoundData* data() const { return m_entry ? &m_entry->playableData : nullptr; }

private:
    void attach(TSoundCacheEntry* entry);
    void release();

    int m_ticket = 0;
    TSoundCacheEntry* m_entry = nullptr;
    TSoundMetadata m_metadata;
};

/// CIP sound::TSoundManager: the cache of playable sound data.
class TSoundManager final : public cip::tibia::sound::TSoundManager
{
    Q_OBJECT
public:
    TSoundManager(std::shared_ptr<TSoundMetadataManager> metadata, std::string soundsDirectory, int32_t cacheSeconds);
    ~TSoundManager() override;

    /// The sound `soundId` to play; starts converting its data when needed.
    std::optional<TPlayableSound> playableSound(TSoundEffectID soundId);
    /// The sound's name for logs.
    QString soundName(TSoundEffectID soundId) const;

public Q_SLOTS:
    void onPreprocessingSoundDataFinished(tibia::sound::TSoundEffectID soundId, const tibia::sound::TSoundData& soundData);
    void addPlayableSoundData(tibia::sound::TSoundEffectID soundId,
                              const tibia::sound::TPlayableSoundData& playableSoundData);
    void cleanupCache() override;
    void onSoundMetadataChanged() override;
    void updateWithTimestamp(qint64 timestampMilliseconds);

private:
    void startLoading(TSoundCacheEntry& entry, const TSoundMetadata& metadata);

    std::shared_ptr<TSoundMetadataManager> m_metadata;
    std::string m_soundsDirectory;
    std::unordered_map<TSoundEffectID, TSoundCacheEntry> m_entries;
    std::unique_ptr<TSoundDecompressorThreadpool> m_decompressor;
    int64_t m_soundCount = 0;
    int64_t m_loadedCount = 0;
    int32_t m_cacheSeconds = 60;
    int64_t m_timestampMs = 0;
    int64_t m_lastCleanupSeconds = 0;
};

}
}
